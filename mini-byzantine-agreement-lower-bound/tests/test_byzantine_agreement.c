/**
 * @file test_byzantine_agreement.c
 * @brief Assertion-based tests for Byzantine agreement lower bound module.
 *
 * Tests cover L1-L6: core definitions, property verification,
 * impossibility proofs, EIG algorithm, consensus types.
 */

#include "byzantine_agreement.h"
#include "eig_algorithm.h"
#include "impossibility_proof.h"
#include "consensus_types.h"
#include "phase_protocols.h"
#include "msg_automaton.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

static int tests_passed = 0;
static int tests_run = 0;

#define TEST(name) do { tests_run++; printf("  %s... ", name); } while(0)
#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while(0)
#define CHECK(cond) do { if (!(cond)) { FAIL(#cond); return; } } while(0)

/* ================================================================
 * L1: Core Definition Tests
 * ================================================================ */

static void test_system_init(void)
{
    TEST("system_init_valid");
    ba_system_t sys;
    int ret = ba_system_init(&sys, 7, 2, BA_MSG_ORAL, BA_SYNC);
    CHECK(ret == 0);
    CHECK(sys.num_processes == 7);
    CHECK(sys.max_faulty == 2);
    CHECK(sys.msg_model == BA_MSG_ORAL);
    CHECK(sys.sync_model == BA_SYNC);
    PASS();
}

static void test_system_init_invalid(void)
{
    TEST("system_init_invalid_params");
    ba_system_t sys;
    CHECK(ba_system_init(&sys, 0, 0, BA_MSG_ORAL, BA_SYNC) == -1);
    CHECK(ba_system_init(&sys, 20, 0, BA_MSG_ORAL, BA_SYNC) == -1);
    CHECK(ba_system_init(&sys, 7, 7, BA_MSG_ORAL, BA_SYNC) == -1);
    CHECK(ba_system_init(NULL, 7, 2, BA_MSG_ORAL, BA_SYNC) == -1);
    PASS();
}

static void test_configure_processes(void)
{
    TEST("configure_processes");
    ba_system_t sys;
    ba_system_init(&sys, 4, 1, BA_MSG_ORAL, BA_SYNC);
    int32_t values[4] = {0, 0, 1, 0};
    ba_fault_type_t faults[4] = {BA_CORRECT, BA_CORRECT, BA_BYZANTINE, BA_CORRECT};
    CHECK(ba_configure_processes(&sys, values, faults) == 0);
    CHECK(sys.processes[0].initial_value == 0);
    CHECK(sys.processes[2].fault_type == BA_BYZANTINE);
    CHECK(sys.processes[3].fault_type == BA_CORRECT);
    PASS();
}

/* ================================================================
 * L4: Impossibility / Lower Bound Tests
 * ================================================================ */

static void test_ba_is_achievable(void)
{
    TEST("ba_is_achievable_sync_oral");
    ba_system_t sys;

    /* N=4, f=1: 3*1 < 4 → achievable */
    ba_system_init(&sys, 4, 1, BA_MSG_ORAL, BA_SYNC);
    CHECK(ba_is_achievable(&sys) == true);

    /* N=3, f=1: 3*1 >= 3 → NOT achievable */
    ba_system_init(&sys, 3, 1, BA_MSG_ORAL, BA_SYNC);
    CHECK(ba_is_achievable(&sys) == false);

    /* N=7, f=2: 3*2 < 7 → achievable */
    ba_system_init(&sys, 7, 2, BA_MSG_ORAL, BA_SYNC);
    CHECK(ba_is_achievable(&sys) == true);

    /* N=10, f=3: 3*3 < 10 → achievable */
    ba_system_init(&sys, 10, 3, BA_MSG_ORAL, BA_SYNC);
    CHECK(ba_is_achievable(&sys) == true);

    /* N=10, f=4: 3*4 >= 10 → NOT achievable */
    ba_system_init(&sys, 10, 4, BA_MSG_ORAL, BA_SYNC);
    CHECK(ba_is_achievable(&sys) == false);
    PASS();
}

static void test_async_impossible(void)
{
    TEST("async_always_impossible_FLP");
    ba_system_t sys;

    /* FLP: asynchronous consensus impossible with even 1 crash */
    ba_system_init(&sys, 100, 1, BA_MSG_ORAL, BA_ASYNC);
    CHECK(ba_is_achievable(&sys) == false);

    ba_system_init(&sys, 100, 0, BA_MSG_SIGNED, BA_ASYNC);
    CHECK(ba_is_achievable(&sys) == false);
    PASS();
}

static void test_max_tolerable_faults(void)
{
    TEST("max_tolerable_faults");
    /* N=3, oral → floor((3-1)/3) = 0 */
    CHECK(ba_max_tolerable_faults(3, BA_MSG_ORAL, BA_SYNC) == 0);
    /* N=4, oral → floor((4-1)/3) = 1 */
    CHECK(ba_max_tolerable_faults(4, BA_MSG_ORAL, BA_SYNC) == 1);
    /* N=7, oral → floor((7-1)/3) = 2 */
    CHECK(ba_max_tolerable_faults(7, BA_MSG_ORAL, BA_SYNC) == 2);
    /* N=10, oral → floor((10-1)/3) = 3 */
    CHECK(ba_max_tolerable_faults(10, BA_MSG_ORAL, BA_SYNC) == 3);
    /* N=10, signed → floor((10-1)/2) = 4 */
    CHECK(ba_max_tolerable_faults(10, BA_MSG_SIGNED, BA_SYNC) == 4);
    /* N=1, oral → (0)/3 = 0 */
    CHECK(ba_max_tolerable_faults(1, BA_MSG_ORAL, BA_SYNC) == 0);
    /* Async → 0 */
    CHECK(ba_max_tolerable_faults(10, BA_MSG_ORAL, BA_ASYNC) == 0);
    PASS();
}

static void test_lsp82_proof_construction(void)
{
    TEST("lsp82_proof_construction");
    lsp82_proof_t proof;
    CHECK(lsp82_construct_proof(&proof) == 0);
    CHECK(proof.proof_valid == true);
    CHECK(proof.num_indist_pairs == 2);
    CHECK(proof.scenarios[SCENARIO_A].g_correct == true);
    CHECK(proof.scenarios[SCENARIO_A].g_value == 0);
    CHECK(proof.scenarios[SCENARIO_B].g_value == 1);
    CHECK(proof.scenarios[SCENARIO_C].g_correct == false);
    CHECK(strlen(proof.contradiction_chain) > 50);
    lsp82_proof_destroy(&proof);
    PASS();
}

static void test_lsp82_proof_verify(void)
{
    TEST("lsp82_proof_verify");
    lsp82_proof_t proof;
    lsp82_construct_proof(&proof);
    CHECK(lsp82_verify_proof(&proof) == true);
    lsp82_proof_destroy(&proof);
    PASS();
}

static void test_simulation_argument(void)
{
    TEST("simulation_argument");
    /* f >= N/3 should return true (impossible) */
    CHECK(lsp82_simulation_argument(3, 1) == true);
    CHECK(lsp82_simulation_argument(6, 2) == true);
    CHECK(lsp82_simulation_argument(9, 3) == true);
    /* f < N/3 should return false */
    CHECK(lsp82_simulation_argument(4, 1) == false);
    CHECK(lsp82_simulation_argument(7, 2) == false);
    CHECK(lsp82_simulation_argument(10, 3) == false);
    PASS();
}

static void test_impossibility_check(void)
{
    TEST("impossibility_check_general");
    bool achievable;
    const char *thm;

    /* Sync oral: N=3, f=1 → impossible */
    CHECK(impossibility_check(3, 1, BA_MSG_ORAL, BA_SYNC, &achievable, &thm) == 0);
    CHECK(achievable == false);
    CHECK(thm != NULL);

    /* Sync oral: N=4, f=1 → possible */
    CHECK(impossibility_check(4, 1, BA_MSG_ORAL, BA_SYNC, &achievable, &thm) == 0);
    CHECK(achievable == true);

    /* Sync signed: N=4, f=2 → possible (f < N) */
    CHECK(impossibility_check(4, 2, BA_MSG_SIGNED, BA_SYNC, &achievable, &thm) == 0);
    CHECK(achievable == true);

    /* Async → impossible */
    CHECK(impossibility_check(100, 1, BA_MSG_ORAL, BA_ASYNC, &achievable, &thm) == 0);
    CHECK(achievable == false);
    PASS();
}

static void test_flp_proof(void)
{
    TEST("flp_proof_construction");
    flp_proof_t proof;
    CHECK(flp_construct_proof(&proof, 2) == 0);
    CHECK(proof.initial_bivalent == true);
    CHECK(proof.bivalence_preserved == true);
    CHECK(proof.num_configs == 1);
    CHECK(flp_verify_proof(&proof) == true);
    flp_proof_destroy(&proof);
    PASS();
}

static void test_ds_bound(void)
{
    TEST("dolev_strong_bound");
    ds_bound_result_t result;
    CHECK(ds_compute_bound(10, &result) == 0);
    CHECK(result.N == 10);
    CHECK(result.f == 4);  /* floor((10-1)/2) = 4 */
    CHECK(result.strong_validity == true);
    CHECK(result.agreement_with_signatures == true);
    PASS();
}

/* ================================================================
 * L5: EIG Algorithm Tests
 * ================================================================ */

static void test_eig_tree_init(void)
{
    TEST("eig_tree_init");
    eig_tree_t tree;
    CHECK(eig_tree_init(&tree, 4, 1, 0, 0) == 0);
    CHECK(tree.N == 4);
    CHECK(tree.f == 1);
    CHECK(tree.depth == 2);
    CHECK(tree.owner_pid == 0);
    CHECK(tree.root != NULL);
    CHECK(tree.total_nodes > 0);
    eig_tree_destroy(&tree);
    PASS();
}

static void test_eig_tree_insert_lookup(void)
{
    TEST("eig_tree_insert_lookup");
    eig_tree_t tree;
    eig_tree_init(&tree, 4, 1, 0, 0);

    /* Insert value at path [2] (process 2 told me directly) */
    int32_t path1[1] = {2};
    CHECK(eig_tree_insert(&tree, path1, 1, 5) == 0);

    int32_t val;
    CHECK(eig_tree_lookup(&tree, path1, 1, &val) == 0);
    CHECK(val == 5);

    /* Insert at path [1, 3] (process 3 told me that process 1 said) */
    int32_t path2[2] = {1, 3};
    CHECK(eig_tree_insert(&tree, path2, 2, 7) == 0);
    CHECK(eig_tree_lookup(&tree, path2, 2, &val) == 0);
    CHECK(val == 7);

    /* Invalid path */
    int32_t path3[1] = {10};
    CHECK(eig_tree_insert(&tree, path3, 1, 9) == -1);

    eig_tree_destroy(&tree);
    PASS();
}

static void test_eig_majority(void)
{
    TEST("eig_majority");
    /* Simple majority */
    int32_t vals1[5] = {0, 0, 0, 1, 1};
    CHECK(eig_majority(vals1, 5) == 0);

    /* No majority */
    int32_t vals2[4] = {0, 0, 1, 1};
    CHECK(eig_majority(vals2, 4) == -1);

    /* Unanimous */
    int32_t vals3[3] = {5, 5, 5};
    CHECK(eig_majority(vals3, 3) == 5);

    /* Single element (value must be within BA_MAX_VALUES range) */
    int32_t vals4[1] = {7};
    CHECK(eig_majority(vals4, 1) == 7);

    /* Empty: should return -1 */
    CHECK(eig_majority(NULL, 0) == -1);

    /* All same but not strict majority? 2 out of 5 is not > 2.5 */
    int32_t vals5[5] = {0, 0, 1, 1, 1};
    CHECK(eig_majority(vals5, 5) == 1);  /* 1 appears 3 times > 2.5 */

    PASS();
}

static void test_eig_tree_resolve(void)
{
    TEST("eig_tree_resolve");
    /* Build a simple EIG tree: N=3, f=1, all values=0 */
    eig_tree_t tree;
    eig_tree_init(&tree, 3, 1, 0, 0);

    /* Fill all nodes with 0 */
    for (int32_t i = 0; i < 3; i++) {
        int32_t p1[1] = {i};
        eig_tree_insert(&tree, p1, 1, 0);
        for (int32_t j = 0; j < 3; j++) {
            if (j == i) continue;
            int32_t p2[2] = {i, j};
            eig_tree_insert(&tree, p2, 2, 0);
        }
    }

    int32_t decision;
    CHECK(eig_tree_resolve(&tree, &decision) == 0);
    CHECK(decision == 0);
    CHECK(tree.has_decided == true);
    eig_tree_destroy(&tree);
    PASS();
}

static void test_eig_canonical_n4_f1(void)
{
    TEST("eig_canonical_n4_f1");
    CHECK(eig_canonical_n4_f1_demo() == 0);
    PASS();
}

static void test_eig_n7_f2_threshold(void)
{
    TEST("eig_canonical_n7_f2_threshold");
    CHECK(eig_canonical_n7_f2_validate() == 0);
    PASS();
}

/* ================================================================
 * L2: Consensus Types Tests
 * ================================================================ */

static void test_consensus_thresholds(void)
{
    TEST("consensus_thresholds");
    consensus_threshold_t th;

    /* Crash-stop: N=5, f max = 2 */
    CHECK(consensus_get_threshold(CONSENSUS_CRASH_STOP, 5, &th) == 0);
    CHECK(th.max_faulty == 2);
    CHECK(th.fault_ratio == 0.5);

    /* Byzantine oral: N=7, f max = 2 */
    CHECK(consensus_get_threshold(CONSENSUS_BYZANTINE_ORAL, 7, &th) == 0);
    CHECK(th.max_faulty == 2);
    CHECK(th.fault_ratio == 1.0/3.0);

    /* Randomized: N=11, f max = 2 */
    CHECK(consensus_get_threshold(CONSENSUS_RANDOMIZED, 11, &th) == 0);
    CHECK(th.max_faulty == 2);

    PASS();
}

static void test_consensus_reductions(void)
{
    TEST("consensus_reductions");
    consensus_reduction_t red;

    /* Signed BFT → Oral BFT */
    CHECK(consensus_has_reduction(CONSENSUS_BYZANTINE_SIGNED,
          CONSENSUS_BYZANTINE_ORAL, &red) == true);
    CHECK(red.is_tight == true);

    /* BFT → Crash */
    CHECK(consensus_has_reduction(CONSENSUS_BYZANTINE_ORAL,
          CONSENSUS_CRASH_STOP, &red) == true);

    /* No reduction: Crash → BFT */
    CHECK(consensus_has_reduction(CONSENSUS_CRASH_STOP,
          CONSENSUS_BYZANTINE_ORAL, &red) == false);

    PASS();
}

static void test_paxos(void)
{
    TEST("paxos");
    paxos_state_t paxos;
    CHECK(paxos_init(&paxos, 5, 2) == 0);
    CHECK(paxos.N == 5);
    CHECK(paxos.quorum_size == 3);

    /* Phase 1 */
    CHECK(paxos_phase1_prepare(&paxos, 1) == 0);
    CHECK(paxos.proposal_number == 1);

    /* Phase 2 */
    CHECK(paxos_phase2_accept(&paxos, 42) == 0);
    CHECK(paxos.has_decided == true);
    CHECK(paxos.decided_value == 42);

    /* Invalid: f >= N/2 */
    CHECK(paxos_init(&paxos, 5, 3) == -1);
    PASS();
}

static void test_pbft(void)
{
    TEST("pbft");
    pbft_state_t pbft;
    CHECK(pbft_init(&pbft, 7, 2) == 0);
    CHECK(pbft.N == 7);
    CHECK(pbft.f == 2);
    CHECK(pbft.quorum_size == 5);

    CHECK(pbft_pre_prepare(&pbft, 100) == 0);
    CHECK(pbft_prepare(&pbft) == 0);
    CHECK(pbft.prepared == true);
    CHECK(pbft_commit(&pbft) == 0);
    CHECK(pbft.committed == true);

    /* Invalid: f >= N/3 */
    CHECK(pbft_init(&pbft, 7, 3) == -1);
    PASS();
}

static void test_failure_detector(void)
{
    TEST("failure_detector");
    failure_detector_t fd;
    CHECK(failure_detector_init(&fd, FD_EVENTUALLY_STRONG, 5) == 0);
    CHECK(fd.N == 5);
    CHECK(fd.class == FD_EVENTUALLY_STRONG);

    /* All alive */
    for (int32_t i = 0; i < 5; i++) {
        failure_detector_heartbeat(&fd, i, true);
        CHECK(failure_detector_is_suspected(&fd, i) == false);
    }

    /* Process 2 crashes */
    failure_detector_heartbeat(&fd, 2, false);
    CHECK(failure_detector_is_suspected(&fd, 2) == true);

    /* Process 2 recovers */
    failure_detector_heartbeat(&fd, 2, true);
    CHECK(failure_detector_is_suspected(&fd, 2) == false);
    CHECK(fd.false_positives >= 1);  /* At least one false positive */
    PASS();
}

static void test_cap_theorem(void)
{
    TEST("cap_theorem");
    cap_system_t cap;

    /* Async: no consensus → AP */
    CHECK(cap_evaluate(5, 1, BA_MSG_ORAL, BA_ASYNC, &cap) == 0);
    CHECK(cap.provides_partition_tolerance == true);
    CHECK(cap.provides_availability == true);
    CHECK(cap.provides_consistency == false);

    /* Sync oral, f < N/3: CA */
    CHECK(cap_evaluate(7, 2, BA_MSG_ORAL, BA_SYNC, &cap) == 0);
    CHECK(cap.provides_consistency == true);

    /* Sync oral, f >= N/3: CP? */
    CHECK(cap_evaluate(6, 2, BA_MSG_ORAL, BA_SYNC, &cap) == 0);
    CHECK(cap.provides_consistency == false);

    PASS();
}

/* ================================================================
 * L5: Phase King Tests
 * ================================================================ */

static void test_phase_king(void)
{
    TEST("phase_king");
    phase_king_system_t pk;
    int32_t values[4] = {0, 0, 0, 1};
    ba_fault_type_t faults[4] = {BA_CORRECT, BA_CORRECT, BA_CORRECT, BA_BYZANTINE};

    CHECK(phase_king_init(&pk, 4, 0, values, faults) == 0);
    /* Phase King requires f < N/4, so with f=0 it's valid */

    ba_result_t result;
    CHECK(phase_king_run(&pk, &result) == 0);
    CHECK(result.agreement_holds == true);
    CHECK(result.termination_holds == true);
    PASS();
}

/* ================================================================
 * L5: Ben-Or Tests
 * ================================================================ */

static void test_benor(void)
{
    TEST("benor");
    benor_system_t bo;
    int32_t values[4] = {0, 0, 0, 0};
    ba_fault_type_t faults[4] = {BA_CORRECT, BA_CORRECT, BA_CORRECT, BA_CORRECT};

    CHECK(benor_init(&bo, 4, 0, values, faults, 12345) == 0);

    ba_result_t result;
    CHECK(benor_run(&bo, &result) == 0);
    /* With all correct and same initial value, should agree */
    CHECK(result.agreement_holds == true);
    CHECK(result.decided_value == 0);
    PASS();
}

/* ================================================================
 * L5: Dolev-Strong Tests
 * ================================================================ */

static void test_dolev_strong(void)
{
    TEST("dolev_strong");
    ds_system_t ds;
    int32_t values[4] = {0, 0, 0, 1};
    ba_fault_type_t faults[4] = {BA_CORRECT, BA_CORRECT, BA_CORRECT, BA_BYZANTINE};

    CHECK(ds_init(&ds, 4, 1, values, faults) == 0);

    ba_result_t result;
    CHECK(ds_run(&ds, &result) == 0);
    /* With signed messages, N=4, f=1 should achieve agreement */
    CHECK(result.agreement_holds == true);
    PASS();
}

/* ================================================================
 * L3: Message Automaton Tests
 * ================================================================ */

static void test_automaton_init(void)
{
    TEST("automaton_init");
    msg_automaton_t ma;
    transition_function_t delta = {0};
    output_function_t lambda = {0};

    CHECK(automaton_init(&ma, 3, 1, 0, 0, delta, lambda) == 0);
    CHECK(ma.state.pid == 0);
    CHECK(ma.state.estimate == 0);
    CHECK(ma.state.round == 0);
    CHECK(ma.state.value_tree != NULL);
    automaton_destroy(&ma);
    PASS();
}

static void test_automaton_config(void)
{
    TEST("automaton_config");
    automaton_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.N = 3;

    /* All decided 0 → 0-valent */
    for (int32_t i = 0; i < 3; i++) {
        cfg.states[i].decided = true;
        cfg.states[i].decision = 0;
    }
    CHECK(automaton_classify_valence(&cfg, 3, 0x00) == AUTOMATON_0_VALENT);

    /* All decided 1 → 1-valent */
    for (int32_t i = 0; i < 3; i++) {
        cfg.states[i].decision = 1;
    }
    CHECK(automaton_classify_valence(&cfg, 3, 0x00) == AUTOMATON_1_VALENT);

    /* Mixed → bivalent */
    cfg.states[0].decision = 0;
    cfg.states[1].decision = 1;
    CHECK(automaton_classify_valence(&cfg, 3, 0x00) == AUTOMATON_BIVALENT);
    PASS();
}

/* ================================================================
 * L7: Blockchain Tests
 * ================================================================ */

/* Forward declarations from blockchain_consensus.c */
extern double nakamoto_attack_success_probability(double q, int32_t z);
extern int32_t nakamoto_min_confirmations(double q, double epsilon);
extern int consensus_feasibility_analysis(int32_t f, int32_t N,
    double network_delay_s, double block_time_s, void *result);
extern int gasper_init(void *gs, int32_t num_validators);
extern int tendermint_init(void *tm, int32_t N, int32_t f);

static void test_nakamoto_probability(void)
{
    TEST("nakamoto_attack_probability");
    /* q=0.1 (10% hash power), z=6 confirmations → very low probability */
    double prob = nakamoto_attack_success_probability(0.1, 6);
    CHECK(prob < 0.001);
    CHECK(prob > 0.0);

    /* q=0.45 (45% hash power), z=6 → moderate probability */
    prob = nakamoto_attack_success_probability(0.45, 6);
    CHECK(prob > 0.01);
    CHECK(prob < 1.0);

    /* q >= 0.5 → attack always succeeds */
    prob = nakamoto_attack_success_probability(0.5, 1000);
    CHECK(prob == 1.0);
    PASS();
}

static void test_nakamoto_confirmations(void)
{
    TEST("nakamoto_min_confirmations");
    /* 10% adversary, 1e-6 safety → should need around 5-6 */
    int32_t k = nakamoto_min_confirmations(0.1, 1e-6);
    CHECK(k > 0 && k <= 20);

    /* 40% adversary, 1e-3 safety → needs many more */
    k = nakamoto_min_confirmations(0.4, 1e-3);
    CHECK(k > 0 && k <= 1000);

    /* >= 50% adversary → impossible */
    k = nakamoto_min_confirmations(0.51, 1e-6);
    CHECK(k == INT32_MAX);
    PASS();
}

/* ================================================================
 * Main
 * ================================================================ */

int main(void)
{
    printf("=== Byzantine Agreement Lower Bound — Test Suite ===\n\n");

    printf("L1: Core Definitions\n");
    test_system_init();
    test_system_init_invalid();
    test_configure_processes();

    printf("\nL4: Impossibility / Lower Bounds\n");
    test_ba_is_achievable();
    test_async_impossible();
    test_max_tolerable_faults();
    test_lsp82_proof_construction();
    test_lsp82_proof_verify();
    test_simulation_argument();
    test_impossibility_check();
    test_flp_proof();
    test_ds_bound();

    printf("\nL5: EIG Algorithm\n");
    test_eig_tree_init();
    test_eig_tree_insert_lookup();
    test_eig_majority();
    test_eig_tree_resolve();
    test_eig_canonical_n4_f1();
    test_eig_n7_f2_threshold();

    printf("\nL2: Consensus Types\n");
    test_consensus_thresholds();
    test_consensus_reductions();
    test_paxos();
    test_pbft();
    test_failure_detector();
    test_cap_theorem();

    printf("\nL5: Phase Protocols\n");
    test_phase_king();
    test_benor();
    test_dolev_strong();

    printf("\nL3: Message Automaton\n");
    test_automaton_init();
    test_automaton_config();

    printf("\nL7: Blockchain Applications\n");
    test_nakamoto_probability();
    test_nakamoto_confirmations();

    printf("\n========================================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);
    printf("========================================\n");

    return (tests_passed == tests_run) ? 0 : 1;
}
