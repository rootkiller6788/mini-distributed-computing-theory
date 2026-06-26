/**
 * test_quorum.c — Tests for quorum system mathematics
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "../include/paxos_raft_types.h"
#include "../include/consensus_model.h"

static void test_quorum_init(void) {
    quorum_system_t qs;
    quorum_system_init(&qs, 5);
    assert(qs.universe_size == 5);
    assert(qs.quorum_size == 3);  /* floor(5/2)+1 = 3 */
    assert(qs.num_quorums == 0);
    printf("  PASS: test_quorum_init\n");
}

static void test_quorum_is_valid(void) {
    quorum_system_t qs;
    quorum_system_init(&qs, 5);

    quorum_t q;
    q.size = 3;
    q.nodes[0] = 0; q.nodes[1] = 1; q.nodes[2] = 2;
    assert(quorum_is_valid(&qs, &q));

    q.size = 2;  /* Not a majority (need > 2.5) */
    assert(!quorum_is_valid(&qs, &q));

    q.size = 5;
    q.nodes[0] = 0; q.nodes[1] = 1; q.nodes[2] = 2;
    q.nodes[3] = 3; q.nodes[4] = 4;
    assert(quorum_is_valid(&qs, &q));

    printf("  PASS: test_quorum_is_valid\n");
}

static void test_quorum_generate_all(void) {
    quorum_system_t qs;
    quorum_system_init(&qs, 5);
    int count = quorum_generate_all(&qs);
    /* n=5, q=3: C(5,3)+C(5,4)+C(5,5) = 10+5+1 = 16 */
    assert(count == 16);
    assert(quorum_intersection_holds(&qs));
    printf("  PASS: test_quorum_generate_all (count=%d)\n", count);
}

static void test_quorum_intersection(void) {
    quorum_t a, b;
    a.size = 3; a.nodes[0] = 0; a.nodes[1] = 1; a.nodes[2] = 2;
    b.size = 3; b.nodes[0] = 2; b.nodes[1] = 3; b.nodes[2] = 4;

    int isect[MAX_NODES];
    int size = quorum_intersection_size(&a, &b, isect);
    assert(size == 1);  /* Only node 2 is common */
    assert(isect[0] == 2);
    printf("  PASS: test_quorum_intersection\n");
}

static void test_quorum_max_faults(void) {
    quorum_system_t qs;
    quorum_system_init(&qs, 7);
    assert(quorum_max_fault_tolerance(&qs) == 3);
    quorum_system_init(&qs, 3);
    assert(quorum_max_fault_tolerance(&qs) == 1);
    printf("  PASS: test_quorum_max_faults\n");
}

static void test_byzantine_quorum(void) {
    assert(byzantine_quorum_size(1) == 3);   /* 2f+1 with f=1 */
    assert(byzantine_total_nodes(1) == 4);   /* 3f+1 with f=1 */
    assert(byzantine_total_nodes(2) == 7);

    quorum_t a, b;
    a.size = 3; a.nodes[0] = 0; a.nodes[1] = 1; a.nodes[2] = 2;
    b.size = 3; b.nodes[0] = 2; b.nodes[1] = 3; b.nodes[2] = 4;

    /* For n=7 (f=2), quorum size = 5, not 3 — test with proper sizes */
    a.size = 5; a.nodes[3] = 3; a.nodes[4] = 4;
    b.size = 5; b.nodes[3] = 5; b.nodes[4] = 6;
    assert(byzantine_quorum_intersection_check(7, &a, &b));

    printf("  PASS: test_byzantine_quorum\n");
}

int main(void) {
    printf("=== Quorum System Tests ===\n");
    test_quorum_init();
    test_quorum_is_valid();
    test_quorum_generate_all();
    test_quorum_intersection();
    test_quorum_max_faults();
    test_byzantine_quorum();
    printf("All quorum tests passed!\n");
    return 0;
}
