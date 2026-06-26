/**
 * test_paxos.c — Tests for Basic Paxos protocol
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "../include/paxos_raft_types.h"
#include "../include/paxos_core.h"

static void test_proposer_init(void) {
    paxos_proposer_t p;
    paxos_proposer_init(&p, 42);
    assert(p.node_id == 42);
    assert(p.current_ballot == 0);
    assert(p.phase == 0);
    printf("  PASS: test_proposer_init\n");
}

static void test_phase1_prepare(void) {
    paxos_proposer_t proposer;
    paxos_proposer_init(&proposer, 0);

    node_id_t acceptor_ids[3] = {1, 2, 3};
    paxos_message_t msgs[3];

    int n = paxos_prepare(&proposer, 100, 3, acceptor_ids, msgs);
    assert(n == 3);
    assert(proposer.phase == 1);
    assert(proposer.current_ballot == 100);

    for (int i = 0; i < 3; i++) {
        assert(msgs[i].type == PAXOS_MSG_PREPARE);
        assert(msgs[i].from == 0);
        assert(msgs[i].to == acceptor_ids[i]);
        assert(msgs[i].ballot == 100);
    }
    printf("  PASS: test_phase1_prepare\n");
}

static void test_phase1_promise(void) {
    paxos_acceptor_t acceptor;
    acceptor.node_id = 1;
    acceptor.promise_ballot = 0;
    acceptor.has_accepted = false;

    paxos_message_t msg;
    msg.type = PAXOS_MSG_PREPARE;
    msg.from = 0;
    msg.to = 1;
    msg.ballot = 100;

    paxos_message_t response;
    bool promised = paxos_receive_prepare(&acceptor, &msg, &response);

    assert(promised);
    assert(acceptor.promise_ballot == 100);
    assert(response.type == PAXOS_MSG_PROMISE);
    assert(response.ballot == 100);
    printf("  PASS: test_phase1_promise\n");
}

static void test_phase1_promise_reject_lower(void) {
    paxos_acceptor_t acceptor;
    acceptor.node_id = 1;
    acceptor.promise_ballot = 200;
    acceptor.has_accepted = false;

    paxos_message_t msg;
    msg.type = PAXOS_MSG_PREPARE;
    msg.from = 0;
    msg.to = 1;
    msg.ballot = 100;  /* Lower than promise_ballot */

    paxos_message_t response;
    bool promised = paxos_receive_prepare(&acceptor, &msg, &response);

    assert(!promised);  /* Should reject lower ballot */
    assert(acceptor.promise_ballot == 200);  /* Unchanged */
    printf("  PASS: test_phase1_promise_reject_lower\n");
}

static void test_phase2_accept(void) {
    paxos_proposer_t proposer;
    paxos_proposer_init(&proposer, 0);
    proposer.current_ballot = 100;
    proposer.phase = 1;

    paxos_message_t promises[3];
    for (int i = 0; i < 3; i++) {
        promises[i].accepted_ballot = 0;
        memset(&promises[i].accepted_value, 0, sizeof(consensus_value_t));
    }

    consensus_value_t value;
    value.data[0] = 'X';
    value.length = 1;

    node_id_t acceptor_ids[3] = {1, 2, 3};
    paxos_message_t msgs[3];

    int n = paxos_accept(&proposer, 3, promises, &value,
                          3, acceptor_ids, msgs);
    assert(n == 3);
    assert(proposer.phase == 2);

    for (int i = 0; i < 3; i++) {
        assert(msgs[i].type == PAXOS_MSG_ACCEPT);
        assert(msgs[i].ballot == 100);
        assert(msgs[i].value.length == 1);
        assert(msgs[i].value.data[0] == 'X');
    }
    printf("  PASS: test_phase2_accept\n");
}

static void test_phase2_accepted(void) {
    paxos_acceptor_t acceptor;
    acceptor.node_id = 1;
    acceptor.promise_ballot = 100;
    acceptor.has_accepted = false;

    paxos_message_t msg;
    msg.type = PAXOS_MSG_ACCEPT;
    msg.from = 0;
    msg.to = 1;
    msg.ballot = 100;
    msg.value.data[0] = 'Y';
    msg.value.length = 1;

    node_id_t learner_ids[2] = {10, 11};
    paxos_message_t out[2];

    int n = paxos_receive_accept(&acceptor, &msg, 2, learner_ids, out);
    assert(n == 2);
    assert(acceptor.has_accepted);
    assert(acceptor.accepted_ballot == 100);
    assert(acceptor.accepted_value.data[0] == 'Y');

    for (int i = 0; i < 2; i++) {
        assert(out[i].type == PAXOS_MSG_ACCEPTED);
        assert(out[i].ballot == 100);
    }
    printf("  PASS: test_phase2_accepted\n");
}

static void test_paxos_full_instance(void) {
    consensus_value_t value;
    value.data[0] = 'A';
    value.data[1] = 'B';
    value.data[2] = 'C';
    value.length = 3;

    consensus_value_t result;
    bool success = paxos_run_instance(0, &value, 3, &result);

    assert(success);
    assert(result.length == 3);
    assert(memcmp(result.data, "ABC", 3) == 0);
    printf("  PASS: test_paxos_full_instance\n");
}

static void test_safety_invariant(void) {
    paxos_acceptor_t acceptors[3];

    for (int i = 0; i < 3; i++) {
        acceptors[i].node_id = i;
        acceptors[i].promise_ballot = 100;
        acceptors[i].accepted_ballot = 100;
        acceptors[i].accepted_value.data[0] = 'V';
        acceptors[i].accepted_value.length = 1;
        acceptors[i].has_accepted = true;
    }

    /* All acceptors have same value — safety should hold */
    assert(paxos_verify_safety_invariant(acceptors, 3));

    /* Introduce a conflicting value */
    acceptors[2].accepted_value.data[0] = 'W';
    assert(!paxos_verify_safety_invariant(acceptors, 3));

    printf("  PASS: test_safety_invariant\n");
}

int main(void) {
    printf("=== Paxos Protocol Tests ===\n");
    test_proposer_init();
    test_phase1_prepare();
    test_phase1_promise();
    test_phase1_promise_reject_lower();
    test_phase2_accept();
    test_phase2_accepted();
    test_paxos_full_instance();
    test_safety_invariant();
    printf("All Paxos tests passed!\n");
    return 0;
}
