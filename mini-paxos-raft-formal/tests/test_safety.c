/**
 * test_safety.c ¡ª Tests for safety proofs and invariants
 * Uses static allocation to avoid stack overflow from large structs.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "../include/paxos_raft_types.h"
#include "../include/consensus_model.h"
#include "../include/invariant_checker.h"
#include "../include/paxos_core.h"
#include "../include/raft_core.h"

/* Static allocation to avoid stack overflow */
static system_state_t g_state;

static void test_intersecting_majorities(void) {
    int maj_a[] = {0, 1, 2};
    int maj_b[] = {2, 3, 4};
    assert(safety_intersecting_majorities_lemma(5, maj_a, 3, maj_b, 3));

    int disjoint_a[] = {0, 1};
    int disjoint_b[] = {2, 3};
    assert(!safety_intersecting_majorities_lemma(5, disjoint_a, 2, disjoint_b, 2));

    printf("  PASS: test_intersecting_majorities\n");
}

static void test_paxos_invariants(void) {
    system_state_t *st = &g_state;
    memset(st, 0, sizeof(*st));

    st->num_proposers = 1;
    st->num_acceptors = 3;
    st->num_learners = 2;

    st->learners[0].has_learned = true;
    st->learners[0].learned_value.data[0] = 'V';
    st->learners[0].learned_value.length = 1;

    st->learners[1].has_learned = true;
    st->learners[1].learned_value.data[0] = 'V';
    st->learners[1].learned_value.length = 1;

    assert(inv_paxos_agreement(st));

    st->learners[1].learned_value.data[0] = 'W';
    assert(!inv_paxos_agreement(st));

    printf("  PASS: test_paxos_invariants\n");
}

static void test_raft_invariants(void) {
    system_state_t *st = &g_state;
    memset(st, 0, sizeof(*st));
    st->num_raft_servers = 3;

    for (int i = 0; i < 3; i++) {
        st->raft_servers[i].persistent.state = RAFT_FOLLOWER;
        st->raft_servers[i].persistent.current_term = 1;
    }
    assert(inv_raft_election_safety(st));

    st->raft_servers[0].persistent.state = RAFT_LEADER;
    st->raft_servers[1].persistent.state = RAFT_LEADER;
    assert(!inv_raft_election_safety(st));

    printf("  PASS: test_raft_invariants\n");
}

static void test_invariant_check_all(void) {
    system_state_t *st = &g_state;
    memset(st, 0, sizeof(*st));
    st->num_proposers = 1;
    st->num_acceptors = 3;
    st->num_learners = 2;
    st->num_raft_servers = 1;

    const char *violations[16];
    int count = invariant_check_all(st, violations);
    assert(count == 0);

    st->learners[0].has_learned = true;
    st->learners[0].learned_value.data[0] = 'A';
    st->learners[0].learned_value.length = 1;
    st->learners[1].has_learned = true;
    st->learners[1].learned_value.data[0] = 'B';
    st->learners[1].learned_value.length = 1;

    count = invariant_check_all(st, violations);
    assert(count >= 1);
    printf("  PASS: test_invariant_check_all (%d violations)\n", count);
}

static void test_flp_bivalent(void) {
    int values_01[] = {0, 1};
    assert(flp_is_bivalent_initial(values_01, 2, 1));

    int values_00[] = {0, 0};
    assert(!flp_is_bivalent_initial(values_00, 2, 1));

    printf("  PASS: test_flp_bivalent\n");
}

static void test_state_machine_safety(void) {
    static raft_server_t servers[3];
    for (int i = 0; i < 3; i++) {
        raft_server_init(&servers[i], (node_id_t)i, 3, 150);
    }

    consensus_value_t cmd;
    cmd.data[0] = 'X'; cmd.length = 1;
    servers[0].persistent.log[0].term = 1;
    servers[0].persistent.log[0].command = cmd;
    servers[0].persistent.log_length = 1;
    servers[0].persistent.last_applied = 1;

    servers[1].persistent.log[0].term = 1;
    servers[1].persistent.log[0].command = cmd;
    servers[1].persistent.log_length = 1;
    servers[1].persistent.last_applied = 1;

    servers[2].persistent.log[0].term = 1;
    servers[2].persistent.log[0].command = cmd;
    servers[2].persistent.log_length = 1;
    servers[2].persistent.last_applied = 0;

    assert(raft_verify_state_machine_safety(servers, 3));
    printf("  PASS: test_state_machine_safety\n");
}

int main(void) {
    printf("=== Safety Proof Tests ===\n");
    test_intersecting_majorities();
    test_paxos_invariants();
    test_raft_invariants();
    test_invariant_check_all();
    test_flp_bivalent();
    test_state_machine_safety();
    printf("All safety tests passed!\n");
    return 0;
}
