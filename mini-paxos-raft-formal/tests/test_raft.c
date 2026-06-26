/**
 * test_raft.c — Tests for Raft consensus algorithm
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "../include/paxos_raft_types.h"
#include "../include/raft_core.h"

static void test_server_init(void) {
    raft_server_t s;
    raft_server_init(&s, 0, 5, 150);

    assert(s.persistent.node_id == 0);
    assert(s.persistent.state == RAFT_FOLLOWER);
    assert(s.persistent.current_term == 0);
    assert(s.persistent.voted_for == MAX_NODES);
    assert(s.persistent.log_length == 0);
    assert(s.persistent.commit_index == 0);
    printf("  PASS: test_server_init\n");
}

static void test_become_candidate(void) {
    raft_server_t s;
    raft_server_init(&s, 2, 5, 150);

    assert(raft_become_candidate(&s));
    assert(s.persistent.state == RAFT_CANDIDATE);
    assert(s.persistent.current_term == 1);  /* Incremented from 0 */
    assert(s.persistent.voted_for == 2);     /* Voted for self */
    printf("  PASS: test_become_candidate\n");
}

static void test_request_vote_grant(void) {
    raft_server_t s;
    raft_server_init(&s, 1, 5, 150);

    raft_request_vote_args_t args;
    args.term = 1;
    args.candidate_id = 0;
    args.last_log_index = 0;
    args.last_log_term = 0;

    raft_request_vote_reply_t reply;
    bool handled = raft_handle_request_vote(&s, &args, &reply);

    assert(handled);
    assert(reply.vote_granted);
    assert(s.persistent.voted_for == 0);
    assert(s.persistent.current_term == 1);
    printf("  PASS: test_request_vote_grant\n");
}

static void test_request_vote_deny_lower_term(void) {
    raft_server_t s;
    raft_server_init(&s, 1, 5, 150);
    s.persistent.current_term = 5;

    raft_request_vote_args_t args;
    args.term = 3;  /* Lower than current term */
    args.candidate_id = 0;
    args.last_log_index = 0;
    args.last_log_term = 0;

    raft_request_vote_reply_t reply;
    raft_handle_request_vote(&s, &args, &reply);

    assert(!reply.vote_granted);  /* Should deny */
    printf("  PASS: test_request_vote_deny_lower_term\n");
}

static void test_append_command(void) {
    raft_server_t s;
    raft_server_init(&s, 0, 3, 150);
    s.persistent.state = RAFT_LEADER;  /* Must be leader */
    s.persistent.current_term = 1;

    consensus_value_t cmd;
    cmd.data[0] = 'X';
    cmd.length = 1;

    index_t idx = raft_append_command(&s, &cmd);
    assert(idx == 1);
    assert(s.persistent.log_length == 1);
    assert(s.persistent.log[0].term == 1);
    assert(s.persistent.log[0].command.data[0] == 'X');

    /* Append another */
    cmd.data[0] = 'Y';
    idx = raft_append_command(&s, &cmd);
    assert(idx == 2);
    assert(s.persistent.log_length == 2);

    printf("  PASS: test_append_command\n");
}

static void test_append_entries(void) {
    /* Leader prepares AppendEntries for follower at index 0 */
    raft_server_t leader;
    raft_server_init(&leader, 0, 3, 150);
    leader.persistent.state = RAFT_LEADER;
    leader.persistent.current_term = 1;

    consensus_value_t cmd;
    cmd.data[0] = 'Z';
    cmd.length = 1;
    raft_append_command(&leader, &cmd);

    raft_append_entries_args_t args;
    bool prepared = raft_prepare_append_entries(&leader, 1, &args);

    assert(prepared);
    assert(args.term == 1);
    assert(args.leader_id == 0);
    assert(args.num_entries >= 1);

    /* Follower receives AppendEntries */
    raft_server_t follower;
    raft_server_init(&follower, 1, 3, 150);

    raft_append_entries_reply_t reply;
    bool handled = raft_handle_append_entries(&follower, &args, &reply);

    assert(handled);
    assert(reply.success);
    assert(follower.persistent.log_length == 1);
    assert(follower.persistent.log[0].command.data[0] == 'Z');

    printf("  PASS: test_append_entries\n");
}

static void test_advance_commit_index(void) {
    raft_server_t leader;
    raft_server_init(&leader, 0, 3, 150);
    leader.persistent.state = RAFT_LEADER;
    leader.persistent.current_term = 1;

    /* Append 2 commands */
    consensus_value_t cmd;
    cmd.data[0] = 'A'; cmd.length = 1;
    raft_append_command(&leader, &cmd);
    cmd.data[0] = 'B'; cmd.length = 1;
    raft_append_command(&leader, &cmd);

    /* Leader has both entries */
    leader.volatile_.match_index[0] = 2;  /* Self */

    /* One follower replicated both */
    leader.volatile_.match_index[1] = 2;

    index_t committed = raft_advance_commit_index(&leader, 3);
    /* With majority (2 out of 3), both entries should be committed */
    assert(committed >= 1);

    printf("  PASS: test_advance_commit_index\n");
}

static void test_election_majority(void) {
    assert(raft_has_election_majority(3, 5));   /* 3 > 5/2 = 2.5 */
    assert(!raft_has_election_majority(2, 5));  /* 2 <= 2.5 */
    assert(raft_has_election_majority(2, 3));   /* 2 > 3/2 = 1.5 */
    printf("  PASS: test_election_majority\n");
}

static void test_log_matching(void) {
    raft_server_t s1, s2;
    raft_server_init(&s1, 0, 3, 150);
    raft_server_init(&s2, 1, 3, 150);

    /* Build identical logs */
    consensus_value_t cmd;
    cmd.data[0] = 'X'; cmd.length = 1;

    s1.persistent.log[0].term = 1;
    s1.persistent.log[0].command = cmd;
    s1.persistent.log_length = 1;

    s2.persistent.log[0].term = 1;
    s2.persistent.log[0].command = cmd;
    s2.persistent.log_length = 1;

    assert(raft_verify_log_matching(s1.persistent.log, 1,
                                     s2.persistent.log, 1, 1));

    /* Mismatch test */
    s2.persistent.log[0].command.data[0] = 'Y';
    assert(!raft_verify_log_matching(s1.persistent.log, 1,
                                      s2.persistent.log, 1, 1));

    printf("  PASS: test_log_matching\n");
}

int main(void) {
    printf("=== Raft Consensus Tests ===\n");
    test_server_init();
    test_become_candidate();
    test_request_vote_grant();
    test_request_vote_deny_lower_term();
    test_append_command();
    test_append_entries();
    test_advance_commit_index();
    test_election_majority();
    test_log_matching();
    printf("All Raft tests passed!\n");
    return 0;
}
