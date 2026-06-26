/*
 * test_consensus.c - Unit tests for consensus and fault tolerance module.
 * Assert-based testing covering all core APIs.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include "consensus_types.h"
#include "consensus_model.h"
#include "paxos.h"
#include "raft.h"
#include "pbft.h"
#include "quorum.h"
#include "nakamoto.h"

/* Byzantine agreement function declarations (from src/byzantine_agreement.c) */
#define ORAL_MSG_RETREAT -1
#define ORAL_MSG_ATTACK   1
extern int  oral_message_om(int commander, const int *lieutenants, int num_lieutenants,
                             int m, int commander_value, int *results, const int *honest);
extern int  min_processes_for_agreement(int f, fault_type_t fault_model,
                                         bool authenticated_channels);
extern int  max_tolerated_faults(int n, fault_type_t fault_model,
                                  bool authenticated_channels);

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { tests_run++; printf("  TEST %s... ", #name); } while(0)
#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while(0)

/* === L1-L2: Fault tolerance bounds === */
static void test_fault_bounds(void)
{
    TEST(fault_bounds);
    assert(consensus_bound_check(3, 1, FAULT_CRASH) == 1);   /* 3 > 2*1 */
    assert(consensus_bound_check(4, 1, FAULT_CRASH) == 1);   /* 4 > 2*1 */
    assert(consensus_bound_check(2, 1, FAULT_CRASH) == 0);   /* 2 <= 2*1 */
    assert(consensus_bound_check(4, 1, FAULT_BYZANTINE) == 2); /* 4 > 3*1 */
    assert(consensus_bound_check(3, 1, FAULT_BYZANTINE) == 0); /* 3 <= 3*1 */
    assert(consensus_bound_check(7, 2, FAULT_BYZANTINE) == 2); /* 7 > 3*2 */
    PASS();
}

/* === L4: FLP impossibility === */
static void test_flp_impossible(void)
{
    TEST(flp_impossible);
    /* Asynchronous + crash + deterministic = impossible */
    assert(flp_impossible(3, FAULT_CRASH, MODEL_ASYNCHRONOUS, false) == true);
    /* Asynchronous + crash + randomized = possible */
    assert(flp_impossible(3, FAULT_CRASH, MODEL_ASYNCHRONOUS, true) == false);
    /* Synchronous = always possible */
    assert(flp_impossible(3, FAULT_CRASH, MODEL_SYNCHRONOUS, false) == false);
    /* No faults = possible */
    assert(flp_impossible(3, FAULT_NONE, MODEL_ASYNCHRONOUS, false) == false);
    PASS();
}

/* === L4: Synchronous round lower bounds === */
static void test_round_bounds(void)
{
    TEST(round_bounds);
    assert(sync_min_rounds(4, 1, FAULT_CRASH) == 2);     /* f+1 = 2 */
    assert(sync_min_rounds(7, 2, FAULT_BYZANTINE) == 3);  /* f+1 = 3 */
    assert(sync_min_rounds(10, 3, FAULT_OMISSION) == 4);  /* f+1 = 4 */
    assert(sync_min_rounds(3, 0, FAULT_NONE) == 1);       /* no fault: 1 */
    PASS();
}

/* === L3: Replicated log operations === */
static void test_replicated_log(void)
{
    TEST(replicated_log);
    replicated_log_t log;
    memset(&log, 0, sizeof(log));

    log_entry_t e1;
    memset(&e1, 0, sizeof(e1));
    e1.term = 1; e1.index = 1;
    strcpy(e1.command, "SET x=1");

    assert(replicated_log_append(&log, e1) == 0);
    assert(log.length == 1);

    log_entry_t e2;
    memset(&e2, 0, sizeof(e2));
    e2.term = 2; e2.index = 2;
    strcpy(e2.command, "SET y=2");
    assert(replicated_log_append(&log, e2) == 0);
    assert(log.length == 2);

    log_entry_t got = replicated_log_get(&log, 1);
    assert(got.term == 1);
    assert(strcmp(got.command, "SET x=1") == 0);

    got = replicated_log_get(&log, 2);
    assert(got.term == 2);

    /* Truncate from index 2 */
    assert(replicated_log_truncate(&log, 2) == 0);
    assert(log.length == 1);

    if (log.entries) free(log.entries);
    PASS();
}

/* === L5: Paxos basic instance === */
static void test_paxos_basic(void)
{
    TEST(paxos_basic);
    int num_acceptors = 5;
    paxos_acceptor_t *acceptors = (paxos_acceptor_t *)calloc(num_acceptors, sizeof(paxos_acceptor_t));
    for (int i = 0; i < num_acceptors; i++) {
        acceptors[i].promised.round = 0;
        acceptors[i].promised.server_id = 0;
        acceptors[i].has_accepted = false;
    }

    paxos_instance_t inst;
    memset(&inst, 0, sizeof(inst));
    inst.instance_id = 1;
    inst.num_acceptors = num_acceptors;
    inst.peer_acceptors = (paxos_acceptor_t *)calloc(num_acceptors, sizeof(paxos_acceptor_t));

    int result = paxos_run_instance(&inst, 42, acceptors, num_acceptors);
    assert(result == 42);
    assert(inst.is_decided);
    assert(inst.decided_value == 42);

    free(acceptors);
    if (inst.peer_acceptors) free(inst.peer_acceptors);
    PASS();
}

/* === L5: Paxos safety verification === */
static void test_paxos_safety(void)
{
    TEST(paxos_safety);
    paxos_instance_t instances[3];
    memset(instances, 0, sizeof(instances));
    instances[0].is_decided = true; instances[0].decided_value = 10;
    instances[1].is_decided = true; instances[1].decided_value = 10;
    instances[2].is_decided = true; instances[2].decided_value = 10;

    assert(paxos_verify_safety(instances, 3) == true);

    /* Introduce conflict */
    instances[2].decided_value = 20;
    assert(paxos_verify_safety(instances, 3) == false);
    PASS();
}

/* === L5: Multi-Paxos === */
static void test_multi_paxos(void)
{
    TEST(multi_paxos);
    int num_acceptors = 5;
    paxos_acceptor_t *acceptors = (paxos_acceptor_t *)calloc(num_acceptors, sizeof(paxos_acceptor_t));
    for (int i = 0; i < num_acceptors; i++) {
        acceptors[i].promised.round = 0;
        acceptors[i].promised.server_id = 0;
    }

    multi_paxos_t mp;
    assert(multi_paxos_init(&mp, 3, num_acceptors) == 0);

    assert(multi_paxos_elect_leader(&mp, 0, acceptors, num_acceptors) == 0);
    assert(mp.leader_id == 0);
    assert(mp.phase1_done);

    int r = multi_paxos_propose(&mp, 0, 100, acceptors, num_acceptors);
    assert(r == 100);

    multi_paxos_destroy(&mp);
    free(acceptors);
    PASS();
}

/* === L5: Raft leader election === */
static void test_raft_election(void)
{
    TEST(raft_election);
    raft_cluster_t cluster;
    assert(raft_cluster_init(&cluster, 5) >= 0);

    /* Server 0 becomes candidate */
    cluster.servers[0].state = RAFT_CANDIDATE;
    cluster.servers[0].current_term = 1;
    cluster.servers[0].voted_for = 0;

    int result = raft_start_election(&cluster.servers[0], &cluster);
    assert(result == 0);  /* should be elected */
    assert(cluster.servers[0].state == RAFT_LEADER);
    assert(cluster.servers[0].leader_id == 0);

    raft_cluster_destroy(&cluster);
    PASS();
}

/* === L5: Raft log replication === */
static void test_raft_log_replication(void)
{
    TEST(raft_log_replication);
    raft_cluster_t cluster;
    raft_cluster_init(&cluster, 5);

    /* Elect leader */
    cluster.servers[0].state = RAFT_CANDIDATE;
    cluster.servers[0].current_term = 1;
    cluster.servers[0].voted_for = 0;
    raft_start_election(&cluster.servers[0], &cluster);

    /* Replicate a command */
    int idx = raft_append_command(&cluster.servers[0], &cluster, "SET x=42", 1, 1);
    assert(idx >= 1);
    assert(cluster.servers[0].log.len >= 1);

    raft_cluster_destroy(&cluster);
    PASS();
}

/* === L5: Raft safety verification === */
static void test_raft_safety(void)
{
    TEST(raft_safety);
    raft_cluster_t cluster;
    raft_cluster_init(&cluster, 3);

    /* After initial state, safety should hold vacuously */
    assert(raft_verify_state_machine_safety(&cluster) == true);

    raft_cluster_destroy(&cluster);
    PASS();
}

/* === L5: PBFT system init and bounds === */
static void test_pbft_init(void)
{
    TEST(pbft_init);
    pbft_system_t sys;
    /* 4 replicas, f=1 (4 >= 3*1+1) */
    int n = pbft_system_init(&sys, 4, 1);
    assert(n == 4);
    assert(sys.num_replicas == 4);
    assert(sys.max_faulty == 1);
    pbft_system_destroy(&sys);

    /* Should fail: 3 replicas, f=1 (3 < 3*1+1) */
    pbft_system_t sys2;
    int n2 = pbft_system_init(&sys2, 3, 1);
    assert(n2 == -1);
    PASS();
}

/* === L5: PBFT pre-prepare sending === */
static void test_pbft_pre_prepare(void)
{
    TEST(pbft_pre_prepare);
    pbft_system_t sys;
    pbft_system_init(&sys, 4, 1);

    int seq = pbft_send_pre_prepare(&sys, 0, 100, 42);
    assert(seq == 1);  /* first sequence number */
    assert(sys.replicas[0].last_seq_num == 1);

    pbft_system_destroy(&sys);
    PASS();
}

/* === L3: Quorum majority construction === */
static void test_quorum_majority(void)
{
    TEST(quorum_majority);
    quorum_system_t *qs = quorum_create_majority(5);
    assert(qs != NULL);
    assert(qs->total_processes == 5);
    assert(qs->quorum_size == 3);  /* floor(5/2)+1 */
    assert(quorum_has_intersection(qs) == true);

    double load = quorum_compute_load(qs, NULL);
    assert(load > 0.0);
    assert(load <= 1.0);

    double cap = quorum_compute_capacity(qs);
    assert(cap > 0.0);

    quorum_system_free(qs);
    PASS();
}

/* === L3: Quorum grid construction === */
static void test_quorum_grid(void)
{
    TEST(quorum_grid);
    quorum_system_t *qs = quorum_create_grid(3);
    assert(qs != NULL);
    assert(qs->total_processes == 9);
    assert(qs->quorum_size == 5);  /* 2*3 - 1 = 5 */
    assert(quorum_has_intersection(qs) == true);
    quorum_system_free(qs);
    PASS();
}

/* === L3: Quorum read/write === */
static void test_quorum_rw(void)
{
    TEST(quorum_rw);
    int values[5] = {10, 50, 10, 30, 10};
    int clocks[5] = {1, 5, 1, 3, 1};

    quorum_system_t *qs = quorum_create_majority(5);
    int read_val = quorum_read(qs, values, clocks);
    assert(read_val == 50);  /* highest clock value in first quorum [0,1,2] */

    int new_clock = quorum_write(qs, values, clocks, 99);
    assert(new_clock == 6);  /* max clock(5) + 1 */

    quorum_system_free(qs);
    PASS();
}

/* === L8: B-Masking quorum construction === */
static void test_quorum_b_masking(void)
{
    TEST(quorum_b_masking);
    quorum_system_t *qs = quorum_create_b_masking(7, 2);  /* n=7, f=2, 7>=3*2+1 */
    assert(qs != NULL);
    assert(qs->total_processes == 7);
    assert(quorum_has_byzantine_intersection(qs, 2) == true);
    quorum_system_free(qs);
    PASS();
}

/* === L5: Nakamoto block mining === */
static void test_nakamoto_mining(void)
{
    TEST(nakamoto_mining);
    nakamoto_block_t block;
    memset(&block, 0, sizeof(block));
    block.block_height = 0;
    block.difficulty_bits = 10;

    uint64_t tries = nakamoto_mine_block(&block, 10);
    assert(tries > 0);
    assert(block.nonce > 0);
    PASS();
}

/* === L2: Nakamoto chain comparison === */
static void test_nakamoto_chain(void)
{
    TEST(nakamoto_chain);
    nakamoto_blockchain_t chain_a, chain_b;
    memset(&chain_a, 0, sizeof(chain_a));
    memset(&chain_b, 0, sizeof(chain_b));

    chain_a.total_work = 100; chain_a.length = 10;
    chain_b.total_work = 200; chain_b.length = 20;

    assert(nakamoto_compare_chains(&chain_a, &chain_b) == -1);
    assert(nakamoto_compare_chains(&chain_b, &chain_a) == 1);
    PASS();
}

/* === L8: Nakamoto reorg probability === */
static void test_nakamoto_reorg_prob(void)
{
    TEST(nakamoto_reorg_prob);
    double p = nakamoto_reorg_probability(0.1, 6);
    assert(p > 0.0);
    assert(p < 0.01);  /* < 0.1% with 6 confirmations */

    double p2 = nakamoto_reorg_probability(0.1, 1);
    assert(p2 > p);  /* fewer confirmations = higher risk */

    /* 51% attacker always succeeds */
    double p3 = nakamoto_reorg_probability(0.51, 100);
    assert(p3 >= 0.999);
    PASS();
}

/* === L8: Selfish mining revenue === */
static void test_selfish_mining(void)
{
    TEST(selfish_mining);
    double rev = nakamoto_selfish_mining_revenue(0.1, 0.5);
    assert(rev > 0.0);
    assert(rev < 1.0);

    /* With 0 hash power, no revenue */
    double rev0 = nakamoto_selfish_mining_revenue(0.0, 0.5);
    assert(rev0 <= 0.0);
    PASS();
}

/* === L2: Consensus safety === */
static void test_consensus_safety(void)
{
    TEST(consensus_safety);
    consensus_config_t cfg;
    assert(consensus_config_init(&cfg, 3, 1, FAULT_CRASH, MODEL_SYNCHRONOUS) == 3);

    /* Set all correct nodes to decided with same value */
    for (int i = 0; i < 3; i++) {
        cfg.nodes[i].decide_state = STATE_DECIDED;
        cfg.nodes[i].decided_value = 7;
    }

    consensus_safety_t safety;
    assert(consensus_verify_properties(cfg.nodes, 3, &safety) == 0);
    assert(safety.agreement_holds == true);
    assert(safety.termination_holds == true);

    /* Introduce disagreement */
    cfg.nodes[2].decided_value = 8;
    consensus_verify_properties(cfg.nodes, 3, &safety);
    assert(safety.agreement_holds == false);

    consensus_config_destroy(&cfg);
    PASS();
}

/* === L4: Consensus solvability === */
static void test_consensus_solvability(void)
{
    TEST(consensus_solvability);
    assert(consensus_is_solvable(MODEL_SYNCHRONOUS, FAULT_CRASH, 5, 2, false) == true);
    assert(consensus_is_solvable(MODEL_ASYNCHRONOUS, FAULT_CRASH, 5, 2, false) == false);
    assert(consensus_is_solvable(MODEL_ASYNCHRONOUS, FAULT_CRASH, 5, 2, true) == true);
    assert(consensus_is_solvable(MODEL_PARTIALLY_SYNCHRONOUS, FAULT_BYZANTINE, 7, 2, false) == true);
    PASS();
}

/* === L4: FLP audit === */
static void test_flp_audit(void)
{
    TEST(flp_audit);
    flp_audit_t audit;
    flp_audit_run(&audit, 5, 2, MODEL_ASYNCHRONOUS, false, false);
    assert(audit.flp_applies == true);
    assert(strlen(audit.explanation) > 0);

    flp_audit_run(&audit, 5, 2, MODEL_ASYNCHRONOUS, true, false);
    assert(audit.randomized_circumvention == true);

    flp_audit_run(&audit, 5, 2, MODEL_SYNCHRONOUS, false, false);
    assert(audit.flp_applies == false);
    PASS();
}

/* === L5: Byzantine Oral Messages === */
static void test_byzantine_oral(void)
{
    TEST(byzantine_oral);
    /* n=4, m=1: OM(1) requires 4 > 3*1 */
    int lieutenants[] = {1, 2, 3};
    int results[3] = {0};
    int honest[] = {1, 1, 1, 1};  /* all honest for test */

    int rc = oral_message_om(0, lieutenants, 3, 1, ORAL_MSG_ATTACK, results, honest);
    assert(rc == 0);
    /* With all honest, all should decide ATTACK */
    for (int i = 0; i < 3; i++) {
        assert(results[i] == ORAL_MSG_ATTACK);
    }
    PASS();
}

/* === L5: Byzantine min processes === */
static void test_byzantine_bounds(void)
{
    TEST(byzantine_bounds);
    assert(min_processes_for_agreement(1, FAULT_BYZANTINE, false) == 4);  /* 3*1+1 */
    assert(min_processes_for_agreement(1, FAULT_BYZANTINE, true) == 2);   /* 1+1 with sigs */
    assert(min_processes_for_agreement(1, FAULT_CRASH, false) == 3);      /* 2*1+1 */
    assert(max_tolerated_faults(7, FAULT_BYZANTINE, false) == 2);   /* floor(6/3) */
    assert(max_tolerated_faults(7, FAULT_CRASH, false) == 3);       /* floor(6/2) */
    PASS();
}

int main(void)
{
    printf("=== mini-consensus-fault-tolerance test suite ===\n\n");

    test_fault_bounds();
    test_flp_impossible();
    test_round_bounds();
    test_replicated_log();
    test_paxos_basic();
    test_paxos_safety();
    test_multi_paxos();
    test_raft_election();
    test_raft_log_replication();
    test_raft_safety();
    test_pbft_init();
    test_pbft_pre_prepare();
    test_quorum_majority();
    test_quorum_grid();
    test_quorum_rw();
    test_quorum_b_masking();
    test_nakamoto_mining();
    test_nakamoto_chain();
    test_nakamoto_reorg_prob();
    test_selfish_mining();
    test_consensus_safety();
    test_consensus_solvability();
    test_flp_audit();
    test_byzantine_oral();
    test_byzantine_bounds();

    printf("\n=== Results: %d/%d tests passed ===\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("ALL TESTS PASSED.\n");
        return 0;
    } else {
        printf("%d TESTS FAILED.\n", tests_run - tests_passed);
        return 1;
    }
}
