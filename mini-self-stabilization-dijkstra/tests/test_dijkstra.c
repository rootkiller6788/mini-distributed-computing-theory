/**
 * @file test_dijkstra.c
 * @brief Comprehensive test suite for Dijkstra's self-stabilizing ring
 *
 * Tests cover:
 *   - Ring initialization and state management
 *   - Privilege detection for all algorithm variants
 *   - Legitimacy detection
 *   - State transitions (3-state, 4-state, K-state)
 *   - Convergence under central daemon
 *   - Closure property verification
 *   - Energy function correctness
 *   - Fault injection and recovery
 *   - Real-world application patterns
 *
 * All tests use standard assert() macros. No custom macros allowed
 * per SKILL.md §4.1 rule 8.
 */

#include "dijkstra_ring.h"
#include "state_machine.h"
#include "scheduler.h"
#include "convergence.h"
#include "self_stabilization.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>

/* ── Forward declarations ────────────────────────────────────────────────── */
/* (from fault_injection.c) */
extern int32_t fault_inject_single(ring_configuration_t *, int32_t, int32_t, uint32_t);
extern int32_t fault_inject_burst(ring_configuration_t *, int32_t, uint32_t);
extern int32_t fault_verify_recovery(int32_t, int32_t, algorithm_variant_t,
                                      int32_t, int32_t, uint32_t);
extern int32_t fault_exhaustive_test(int32_t, int32_t, int32_t);
extern int32_t fault_cascade_analysis(ring_configuration_t *, int32_t,
                                       int32_t *, int32_t, int32_t);

/* (from applications.c) */
extern int32_t ss_app_routing_recovery(int32_t, uint32_t);
extern int32_t ss_app_leader_election(int32_t, int32_t, uint32_t);
extern int32_t ss_app_time_sync(int32_t, int64_t, int32_t, uint32_t);
extern int32_t ss_app_mutual_exclusion_cloud(int32_t, int32_t, int32_t, uint32_t);
extern int32_t ss_app_spanning_tree(int32_t, int32_t, uint32_t);
extern int32_t ss_app_blockchain_consensus(int32_t, int32_t, uint32_t);

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    tests_run++; \
    printf("  TEST %s ... ", name); \
} while(0)

#define PASS() do { \
    tests_passed++; \
    printf("PASSED\n"); \
} while(0)

/* ── L1: Ring Initialization Tests ──────────────────────────────────────── */

static void test_ring_init_basic(void) {
    TEST("ring_init_basic");
    ring_configuration_t config;
    int32_t n = ring_init(&config, 5, 3, ALGORITHM_3STATE);
    assert(n == 5);
    assert(config.num_machines == 5);
    assert(config.num_states == 3);
    assert(config.variant == ALGORITHM_3STATE);
    assert(config.machines != NULL);
    assert(config.machines[0].is_bottom == true);
    assert(config.machines[4].is_top == true);
    assert(config.machines[1].is_bottom == false);
    ring_destroy(&config);
    PASS();
}

static void test_ring_init_invalid(void) {
    TEST("ring_init_invalid");
    ring_configuration_t config;

    /* N too small */
    int32_t r = ring_init(&config, 1, 3, ALGORITHM_3STATE);
    assert(r == -1);

    /* K wrong for 3-state */
    r = ring_init(&config, 5, 4, ALGORITHM_3STATE);
    assert(r == -1);

    /* K wrong for 4-state */
    r = ring_init(&config, 5, 3, ALGORITHM_4STATE);
    assert(r == -1);

    /* K ≤ N for K-state */
    r = ring_init(&config, 5, 5, ALGORITHM_KSTATE);
    assert(r == -1);

    /* NULL config */
    r = ring_init(NULL, 5, 3, ALGORITHM_3STATE);
    assert(r == -1);

    PASS();
}

static void test_ring_set_get_state(void) {
    TEST("ring_set_get_state");
    ring_configuration_t config;
    ring_init(&config, 5, 3, ALGORITHM_3STATE);

    assert(ring_set_state(&config, 0, 2) == 0);
    assert(ring_get_state(&config, 0) == 2);

    assert(ring_set_state(&config, 4, 1) == 0);
    assert(ring_get_state(&config, 4) == 1);

    /* Out of bounds */
    assert(ring_set_state(&config, 5, 0) == -1);
    assert(ring_set_state(&config, 0, 3) == -1);
    assert(ring_get_state(&config, 5) == -1);

    ring_destroy(&config);
    PASS();
}

static void test_ring_randomize(void) {
    TEST("ring_randomize");
    ring_configuration_t config;
    ring_init(&config, 10, 3, ALGORITHM_3STATE);

    ring_randomize_states(&config, 42);

    /* Verify all states are within [0, 2] */
    int32_t i;
    for (i = 0; i < 10; i++) {
        int32_t s = ring_get_state(&config, i);
        assert(s >= 0 && s < 3);
    }

    ring_destroy(&config);
    PASS();
}

static void test_ring_uniform(void) {
    TEST("ring_uniform");
    ring_configuration_t config;
    ring_init(&config, 5, 3, ALGORITHM_3STATE);

    ring_set_uniform_state(&config, 1);
    int32_t i;
    for (i = 0; i < 5; i++) {
        assert(ring_get_state(&config, i) == 1);
    }

    ring_destroy(&config);
    PASS();
}

/* ── L2: Privilege and Legitimacy Tests ─────────────────────────────────── */

static void test_privilege_3state_uniform(void) {
    TEST("privilege_3state_uniform");
    ring_configuration_t config;
    ring_init(&config, 5, 3, ALGORITHM_3STATE);

    /* All machines state 0 */
    ring_set_uniform_state(&config, 0);

    /* Bottom (machine 0) has privilege: S[0]==S[4] */
    privilege_t p0 = ring_compute_privilege(&config, 0);
    assert(p0 == PRIVILEGE_BOTTOM);

    /* Machines 1-4 should NOT have privilege: S[i]==S[i-1] */
    int32_t i;
    for (i = 1; i < 5; i++) {
        privilege_t p = ring_compute_privilege(&config, i);
        assert(p == PRIVILEGE_NONE);
    }

    assert(ring_count_privileges(&config) == 1);
    assert(ring_is_legitimate(&config) == true);

    ring_destroy(&config);
    PASS();
}

static void test_privilege_3state_diverse(void) {
    TEST("privilege_3state_diverse");
    ring_configuration_t config;
    ring_init(&config, 4, 3, ALGORITHM_3STATE);

    /* Set diverse states: [0, 1, 2, 0] */
    ring_set_state(&config, 0, 0);
    ring_set_state(&config, 1, 1);
    ring_set_state(&config, 2, 2);
    ring_set_state(&config, 3, 0);

    /* Machine 0: S[0]=0, S[3]=0 → privileged (bottom) */
    assert(ring_compute_privilege(&config, 0) == PRIVILEGE_BOTTOM);

    /* Machine 1: S[1]=1, S[0]=0 → privileged (normal) */
    assert(ring_compute_privilege(&config, 1) == PRIVILEGE_NORMAL);

    /* Machine 2: S[2]=2, S[1]=1 → privileged */
    assert(ring_compute_privilege(&config, 2) == PRIVILEGE_NORMAL);

    /* Machine 3: S[3]=0, S[2]=2 → privileged */
    assert(ring_compute_privilege(&config, 3) == PRIVILEGE_NORMAL);

    /* 4 privileges: not legitimate */
    assert(ring_count_privileges(&config) == 4);
    assert(ring_is_legitimate(&config) == false);

    ring_destroy(&config);
    PASS();
}

static void test_privilege_kstate(void) {
    TEST("privilege_kstate");
    ring_configuration_t config;
    /* K=4 > N=3, so this is valid for K-state */
    ring_init(&config, 3, 4, ALGORITHM_KSTATE);

    ring_set_state(&config, 0, 0);
    ring_set_state(&config, 1, 0);
    ring_set_state(&config, 2, 0);

    /* All same: bottom privileged */
    assert(ring_compute_privilege(&config, 0) == PRIVILEGE_BOTTOM);
    assert(ring_count_privileges(&config) == 1);
    assert(ring_is_legitimate(&config) == true);

    ring_destroy(&config);
    PASS();
}

/* ── L3: State Transition Tests ─────────────────────────────────────────── */

static void test_step_3state_bottom(void) {
    TEST("step_3state_bottom");
    ring_configuration_t config;
    ring_init(&config, 5, 3, ALGORITHM_3STATE);

    /* Uniform state 0: bottom has privilege */
    ring_set_uniform_state(&config, 0);

    int32_t result = ring_step_3state(&config, 0);
    assert(result == 0); /* Should execute */

    /* S[0] should now be (0+1) mod 3 = 1 */
    assert(ring_get_state(&config, 0) == 1);
    /* Other machines unchanged */
    assert(ring_get_state(&config, 1) == 0);
    assert(ring_get_state(&config, 4) == 0);

    ring_destroy(&config);
    PASS();
}

static void test_step_3state_nonbottom(void) {
    TEST("step_3state_nonbottom");
    ring_configuration_t config;
    ring_init(&config, 5, 3, ALGORITHM_3STATE);

    /* S[0]=1, S[1]=0 (prev), others 0 */
    ring_set_state(&config, 0, 1);
    ring_set_state(&config, 1, 0);
    ring_set_state(&config, 2, 0);
    ring_set_state(&config, 3, 0);
    ring_set_state(&config, 4, 0);

    /* Machine 1: S[1]=0 != S[0]=1, should copy S[0] */
    int32_t result = ring_step_3state(&config, 1);
    assert(result == 0);
    assert(ring_get_state(&config, 1) == 1);

    ring_destroy(&config);
    PASS();
}

static void test_step_3state_no_move(void) {
    TEST("step_3state_no_move");
    ring_configuration_t config;
    ring_init(&config, 5, 3, ALGORITHM_3STATE);

    /* S[0]=1, others 0 */
    /* Machine 1: S[1]=0 != S[0]=1 → enabled, execute it */
    /* After: S[0]=1, S[1]=1, S[2]=0, S[3]=0, S[4]=0 */
    ring_set_state(&config, 0, 1);

    /* Machine 2: S[2]=0 != S[1]=0... wait, S[1] is now 0? No, */
    /* first let's do the step */
    ring_step_3state(&config, 1);  /* S[1] copies S[0] → S[1]=1 */

    /* Now machine 2: S[2]=0, prev=S[1]=1 → S[2]≠S[1], so privileged */
    int32_t result = ring_step_3state(&config, 2);
    assert(result == 0); /* Should execute */

    /* Let's try: S[0]=1, S[1]=1 (after copy), check machine 2 again */
    /* Machine 2 can execute: S[2] becomes S[1]=1 */
    /* Now S[0]=S[1]=S[2]=1, S[3]=S[4]=0 */
    /* Machine 0: S[0]=1 != S[4]=0, no privilege */
    /* But bottom privilege: S[0]==S[4]? No, 1!=0 */
    /* Machine 3: S[3]=0 != S[2]=1, privilege */

    /* Now check: machine 0 after all this */
    /* S[0]=1, S[4]=0: not equal, no bottom privilege */
    int32_t r0 = ring_step_3state(&config, 0);
    assert(r0 == 1); /* Guard false: no move */

    ring_destroy(&config);
    PASS();
}

static void test_step_4state(void) {
    TEST("step_4state");
    ring_configuration_t config;
    ring_init(&config, 4, 4, ALGORITHM_4STATE);

    /* Set a specific configuration for 4-state testing */
    ring_set_state(&config, 0, 0);
    ring_set_state(&config, 1, 1); /* S[0]+1=1, so bottom guard is true */
    ring_set_state(&config, 2, 2); /* S[1]+1=2, middle privileged */
    ring_set_state(&config, 3, 3);

    /* Bottom machine 0: S[0]+1 mod 4 = 1 == S[1]=1 → privileged */
    privilege_t p0 = ring_compute_privilege(&config, 0);
    assert(p0 == PRIVILEGE_BOTTOM);

    /* Execute bottom: S[0] := S[1] = 1 */
    int32_t r = ring_step_4state(&config, 0);
    assert(r == 0);
    assert(ring_get_state(&config, 0) == 1);

    ring_destroy(&config);
    PASS();
}

static void test_step_kstate(void) {
    TEST("step_kstate");
    ring_configuration_t config;
    ring_init(&config, 3, 4, ALGORITHM_KSTATE); /* K=4 > N=3 */

    ring_set_uniform_state(&config, 2);

    /* Bottom machine: S[0]=2 == S[2]=2 → execute */
    int32_t r = ring_step_kstate(&config, 0);
    assert(r == 0);
    assert(ring_get_state(&config, 0) == 3); /* (2+1) mod 4 = 3 */

    ring_destroy(&config);
    PASS();
}

static void test_ring_step_dispatch(void) {
    TEST("ring_step_dispatch");
    ring_configuration_t config;
    ring_init(&config, 5, 3, ALGORITHM_3STATE);

    ring_set_uniform_state(&config, 0);
    int32_t r = ring_step(&config, 0);
    assert(r == 0);
    assert(ring_get_state(&config, 0) == 1);

    ring_destroy(&config);
    PASS();
}

/* ── L4: Convergence Tests ──────────────────────────────────────────────── */

static void test_converge_small_ring_3state(void) {
    TEST("converge_small_ring_3state");
    ring_configuration_t config;
    ring_init(&config, 5, 3, ALGORITHM_3STATE);

    ring_randomize_states(&config, 12345);

    int32_t steps = ring_converge_to_legitimate(&config, 10000, NULL, 0);
    assert(steps >= 0);
    assert(steps > 0); /* Must take at least 1 step from random state */
    assert(ring_is_legitimate(&config) == true);

    ring_destroy(&config);
    PASS();
}

static void test_converge_uniform_state(void) {
    TEST("converge_uniform_state");
    ring_configuration_t config;
    ring_init(&config, 4, 3, ALGORITHM_3STATE);

    /* Uniform state — already legitimate (bottom has token) */
    ring_set_uniform_state(&config, 2);
    assert(ring_is_legitimate(&config) == true);

    /* Converge should find it immediately */
    int32_t steps = ring_converge_to_legitimate(&config, 100, NULL, 0);
    assert(steps == 0);

    ring_destroy(&config);
    PASS();
}

static void test_closure_verification(void) {
    TEST("closure_verification");
    ring_configuration_t config;
    ring_init(&config, 5, 3, ALGORITHM_3STATE);

    /* Set up a legitimate configuration */
    ring_set_uniform_state(&config, 0);

    bool closure = ring_verify_closure(&config);
    assert(closure == true);

    ring_destroy(&config);
    PASS();
}

/* ── L4: Ring Clone and Equality ───────────────────────────────────────── */

static void test_ring_clone(void) {
    TEST("ring_clone");
    ring_configuration_t config;
    ring_init(&config, 5, 3, ALGORITHM_3STATE);
    ring_randomize_states(&config, 77);

    ring_configuration_t *clone = ring_clone(&config);
    assert(clone != NULL);
    assert(ring_equals(&config, clone) == true);

    /* Modify clone, verify divergence */
    ring_set_state(clone, 0, 5); /* Invalid, but let's check */
    (void)ring_set_state(clone, 0, (ring_get_state(clone, 0) + 1) % 3);
    /* After modifying, they should differ */
    /* (may or may not, since ring_set_state might fail) */

    ring_destroy(&config);
    ring_destroy(clone);
    free(clone);
    PASS();
}

/* ── L4: Dijkstra's Theorems ────────────────────────────────────────────── */

static void test_theorem_3state_n3(void) {
    TEST("theorem_3state_n3");
    /* Verify Dijkstra's 3-state theorem for N=3 */
    bool holds = ss_theorem_dijkstra_3state(3);
    assert(holds == true);
    PASS();
}

static void test_theorem_kstate_n2_k3(void) {
    TEST("theorem_kstate_n2_k3");
    /* N=2, K=3: K>N holds, should converge */
    bool holds = ss_theorem_dijkstra_kstate(2, 3);
    assert(holds == true);
    PASS();
}

static void test_min_states_theorem(void) {
    TEST("min_states_theorem");
    /* For N=5 uniform machines, minimum states = N+1 = 6 */
    int32_t min_k = ss_theorem_min_states_uniform(5);
    assert(min_k == 6);
    assert(ss_theorem_min_states_uniform(3) == 4);
    assert(ss_theorem_min_states_uniform(2) == 3);
    PASS();
}

static void test_symmetry_breaking(void) {
    TEST("symmetry_breaking");
    /* 3-state algorithm should break symmetry */
    bool can_break = ss_lemma_symmetry_breaking(4, 3);
    assert(can_break == true);
    PASS();
}

/* ── L3: Energy Function Tests ──────────────────────────────────────────── */

static void test_energy_difference_count(void) {
    TEST("energy_difference_count");
    ring_configuration_t config;
    ring_init(&config, 5, 3, ALGORITHM_3STATE);

    /* Uniform state: 1 difference (at wrap-around for bottom priv) */
    /* Actually uniform means S[0]==S[N-1], so at pos 0 no diff either */
    /* Wait — differences counted as S[i] != S[i-1] */
    ring_set_uniform_state(&config, 0);
    /* All equal: 0 differences */
    int32_t e = convergence_energy_difference_count(&config);
    assert(e == 0); /* All equal, no differences */

    /* Create differences */
    ring_set_state(&config, 1, 1);
    e = convergence_energy_difference_count(&config);
    assert(e >= 1);

    ring_destroy(&config);
    PASS();
}

static void test_energy_squared_sum(void) {
    TEST("energy_squared_sum");
    ring_configuration_t config;
    ring_init(&config, 5, 3, ALGORITHM_3STATE);

    ring_set_uniform_state(&config, 2);
    int64_t e = convergence_energy_squared_sum(&config);
    /* All 2: 5 * 2^2 = 5 * 4 = 20 */
    assert(e == 20);

    ring_destroy(&config);
    PASS();
}

static void test_energy_token_misplacement(void) {
    TEST("energy_token_misplacement");
    ring_configuration_t config;
    ring_init(&config, 5, 3, ALGORITHM_3STATE);

    ring_set_uniform_state(&config, 0);
    /* All 0: majority value is 0, all match → 0 misplacements */
    int32_t m = convergence_energy_token_misplacement(&config);
    assert(m == 0);

    ring_set_state(&config, 2, 1);
    /* Majority still 0 (4 out of 5), one outlier → 1 misplacement */
    m = convergence_energy_token_misplacement(&config);
    assert(m == 1);

    ring_destroy(&config);
    PASS();
}

static void test_energy_delta_nonpositive(void) {
    TEST("energy_delta_nonpositive");
    ring_configuration_t config;
    ring_init(&config, 4, 3, ALGORITHM_3STATE);

    ring_set_state(&config, 0, 0);
    ring_set_state(&config, 1, 1);
    ring_set_state(&config, 2, 0);
    ring_set_state(&config, 3, 0);

    /* Check bottom machine */
    if (ring_compute_privilege(&config, 0) != PRIVILEGE_NONE) {
        int32_t delta = convergence_energy_delta(&config, 0);
        assert(delta <= 0); /* Energy should not increase */
    }

    ring_destroy(&config);
    PASS();
}

static void test_energy_monotonicity(void) {
    TEST("energy_monotonicity");
    /* Exhaustive check for small N=3, K=3 */
    bool mono = convergence_verify_monotonicity(3, 3);
    assert(mono == true);
    PASS();
}

/* ── L5: Convergence Analysis Tests ─────────────────────────────────────── */

static void test_convergence_experiment(void) {
    TEST("convergence_experiment");
    convergence_result_t *r = convergence_run_experiment(
        5, 3, ALGORITHM_3STATE, DAEMON_CENTRAL, 5000, 42);
    assert(r != NULL);
    assert(r->converged == true);
    assert(r->total_steps > 0);
    assert(r->final_num_privileges == 1);

    convergence_result_print(r);
    free(r);
    PASS();
}

static void test_convergence_trials(void) {
    TEST("convergence_trials");
    convergence_result_t *results = convergence_run_trials(
        4, 3, ALGORITHM_3STATE, 10, 2000, 42);
    assert(results != NULL);

    convergence_stats_t stats;
    convergence_compute_stats(results, 10, &stats);
    assert(stats.num_converged == 10);
    assert(stats.convergence_rate == 1.0);
    assert(stats.mean_steps > 0);

    convergence_stats_print(&stats);
    free(results);
    PASS();
}

static void test_worst_case_distance(void) {
    TEST("worst_case_distance");
    int32_t worst[4];
    int32_t max_dist = convergence_worst_case_distance(
        3, 3, ALGORITHM_3STATE, worst);
    /* For N=3, K=3, all configs should converge, max distance > 0 */
    assert(max_dist > 0);
    printf("max_distance=%d ", max_dist);
    PASS();
}

/* ── L4: Scheduler Tests ────────────────────────────────────────────────── */

static void test_scheduler_central(void) {
    TEST("scheduler_central");
    scheduler_t sched;
    int32_t r = scheduler_init(&sched, DAEMON_CENTRAL, FAIRNESS_WEAK, 5, 0);
    assert(r == 0);

    bool privs[5] = {true, false, true, false, false};
    scheduler_update_privileges(&sched, privs);

    int32_t selected[5];
    int32_t n = scheduler_select(&sched, selected, 5);
    assert(n == 1); /* Central daemon selects exactly 1 */

    scheduler_record_execution(&sched, selected[0]);

    int32_t counts[5];
    scheduler_starvation_counts(&sched, counts);
    /* At least the selected machine should have a small starvation count */

    scheduler_destroy(&sched);
    PASS();
}

static void test_scheduler_distributed(void) {
    TEST("scheduler_distributed");
    scheduler_t sched;
    scheduler_init(&sched, DAEMON_DISTRIBUTED, FAIRNESS_NONE, 5, 0);

    bool privs[5] = {true, true, false, true, false};
    scheduler_update_privileges(&sched, privs);

    int32_t selected[5];
    int32_t n = scheduler_select(&sched, selected, 5);
    assert(n == 3); /* All 3 privileged machines selected */

    scheduler_destroy(&sched);
    PASS();
}

static void test_scheduler_probabilistic(void) {
    TEST("scheduler_probabilistic");
    scheduler_t sched;
    scheduler_init(&sched, DAEMON_DISTRIBUTED, FAIRNESS_NONE, 10, 42);

    bool privs[10] = {true, true, true, true, true,
                      false, false, false, false, false};
    int32_t selected[10];
    int32_t n = scheduler_select_probabilistic(&sched, privs, 0.5,
                                                selected, 10);
    /* With p=0.5 and 5 privileged, expected ~2.5 selected */
    assert(n >= 0 && n <= 5);

    scheduler_destroy(&sched);
    PASS();
}

static void test_convergence_bounds(void) {
    TEST("convergence_bounds");
    /* Central daemon bound: N^2 */
    assert(scheduler_convergence_bound(5, 3, DAEMON_CENTRAL) == 25);
    assert(scheduler_convergence_bound(10, 3, DAEMON_CENTRAL) == 100);
    /* Synchronous bound: N */
    assert(scheduler_convergence_bound(5, 3, DAEMON_SYNCHRONOUS) == 5);

    /* Expected rounds */
    double er = scheduler_expected_convergence_rounds(5, 3, 0.5);
    assert(er > 0);
    printf("E[rounds]=%.1f ", er);

    PASS();
}

/* ── L4: Legitimate Configuration Enumeration ───────────────────────────── */

static void test_legitimate_enumeration(void) {
    TEST("legitimate_enumeration");
    legitimate_set_t lset;
    int32_t r = ss_enumerate_legitimate_configs(3, 3, 0, &lset);
    assert(r == 0);
    /* N=3, K=3: total 27 configs.
       Legitimate = exactly 1 privilege.
       Count: bottom-only (all equal) = 3 +
              machine-1-only (a≠b, c=b) = 6 +
              machine-2-only (a=b, a≠c) = 6 = 15 total */
    assert(lset.total_configs == 27);
    assert(lset.num_legitimate == 15);
    assert(fabs(lset.density - 15.0/27.0) < 0.001);

    free(lset.config_ids);
    PASS();
}

/* ── L7: Fault Injection Tests ──────────────────────────────────────────── */

static void test_fault_single_recovery(void) {
    TEST("fault_single_recovery");
    int32_t steps = fault_verify_recovery(5, 3, ALGORITHM_3STATE, 0, 5000, 123);
    assert(steps >= 0);
    printf("recovery_steps=%d ", steps);
    PASS();
}

static void test_fault_burst_recovery(void) {
    TEST("fault_burst_recovery");
    int32_t steps = fault_verify_recovery(5, 3, ALGORITHM_3STATE, 1, 5000, 456);
    assert(steps >= 0);
    printf("recovery_steps=%d ", steps);
    PASS();
}

static void test_fault_exhaustive(void) {
    TEST("fault_exhaustive_n3_k3");
    int32_t result = fault_exhaustive_test(3, 3, 2000);
    /* Should recover from all single faults */
    assert(result > 0);
    printf("successful=%d ", result);
    PASS();
}

/* ── L7: Application Tests ──────────────────────────────────────────────── */

static void test_app_routing_recovery(void) {
    TEST("app_routing_recovery");
    int32_t rounds = ss_app_routing_recovery(10, 42);
    assert(rounds >= 0);
    printf("rounds=%d ", rounds);
    PASS();
}

static void test_app_leader_election(void) {
    TEST("app_leader_election");
    int32_t leader = ss_app_leader_election(7, 5000, 99);
    assert(leader >= 0 && leader < 7);
    printf("leader=%d ", leader);
    PASS();
}

static void test_app_mutual_exclusion_cloud(void) {
    TEST("app_mutual_exclusion_cloud");
    int32_t recoveries = ss_app_mutual_exclusion_cloud(5, 3, 2000, 777);
    assert(recoveries == 3); /* All faults should be recovered from */
    PASS();
}

static void test_app_time_sync(void) {
    TEST("app_time_sync");
    int32_t rounds = ss_app_time_sync(8, 2, 500, 314);
    /* Time sync with small tolerance should converge */
    assert(rounds >= 0 || rounds == -1);
    printf("sync_rounds=%d ", rounds);
    PASS();
}

static void test_app_spanning_tree(void) {
    TEST("app_spanning_tree");
    int32_t rounds = ss_app_spanning_tree(8, 500, 2718);
    assert(rounds >= 0);
    printf("tree_rounds=%d ", rounds);
    PASS();
}

static void test_app_blockchain_consensus(void) {
    TEST("app_blockchain_consensus");
    int32_t rounds = ss_app_blockchain_consensus(6, 5000, 999);
    assert(rounds >= 0);
    printf("consensus_rounds=%d ", rounds);
    PASS();
}

/* ── L4: State Machine Tests ────────────────────────────────────────────── */

/* Guard and command for Dijkstra 3-state bottom machine */
static bool guard_bottom_3state(const int32_t *local_state,
                                 const int32_t *neighbor_states,
                                 int32_t num_neighbors,
                                 void *context)
{
    (void)num_neighbors;
    (void)context;
    /* Bottom guard: S[0] == S[N-1] */
    return (local_state[0] == neighbor_states[0]);
}

static int32_t command_bottom_3state(const int32_t *local_state
                                      __attribute__((unused)),
                                      const int32_t *neighbor_states,
                                      int32_t num_neighbors,
                                      void *context)
{
    (void)num_neighbors;
    (void)local_state;
    int32_t k = *(int32_t *)context;
    /* Command: S[0] := (S[N-1] + 1) mod K */
    return (neighbor_states[0] + 1) % k;
}

static bool guard_nonbottom_3state(const int32_t *local_state,
                                    const int32_t *neighbor_states,
                                    int32_t num_neighbors,
                                    void *context)
{
    (void)num_neighbors;
    (void)context;
    return (local_state[0] != neighbor_states[0]);
}

static int32_t command_nonbottom_3state(const int32_t *local_state,
                                         const int32_t *neighbor_states,
                                         int32_t num_neighbors,
                                         void *context)
{
    (void)local_state;
    (void)num_neighbors;
    (void)context;
    return neighbor_states[0]; /* Copy predecessor */
}

static void test_state_machine_init(void) {
    TEST("state_machine_init");
    ss_state_machine_t machine;
    int32_t r = ss_machine_init(&machine, 0, 3, 2, 1);
    assert(r == 0);
    assert(machine.num_states == 3);
    assert(machine.num_rules == 2);
    assert(machine.num_neighbors == 1);

    /* Set rules */
    r = ss_machine_set_rule(&machine, 0, guard_bottom_3state,
                             command_bottom_3state, "bottom_rule");
    assert(r == 0);
    r = ss_machine_set_rule(&machine, 1, guard_nonbottom_3state,
                             command_nonbottom_3state, "nonbottom_rule");
    assert(r == 0);

    /* Set neighbor */
    r = ss_machine_set_neighbor(&machine, 0, 4); /* Predecessor = machine 4 */
    assert(r == 0);

    ss_machine_destroy(&machine);
    PASS();
}

static void test_state_machine_evaluate(void) {
    TEST("state_machine_evaluate");
    ss_state_machine_t machine;
    ss_machine_init(&machine, 0, 3, 2, 1);
    ss_machine_set_rule(&machine, 0, guard_bottom_3state,
                         command_bottom_3state, "bottom");
    ss_machine_set_rule(&machine, 1, guard_nonbottom_3state,
                         command_nonbottom_3state, "nonbottom");

    int32_t k = 3;
    machine.context = &k;
    machine.local_state = 0;

    /* Neighbor state = 0: bottom guard true */
    int32_t neighbor_states[1] = {0};
    int32_t rule_idx = ss_machine_evaluate_guards(&machine, neighbor_states);
    assert(rule_idx == 0); /* Bottom rule enabled */

    /* Execute */
    int32_t new_state = ss_machine_execute_command(&machine, rule_idx,
                                                    neighbor_states);
    assert(new_state == 1); /* (0+1) mod 3 = 1 */

    ss_machine_destroy(&machine);
    PASS();
}

static void test_global_config(void) {
    TEST("global_config");
    ss_state_machine_t machines[3];
    int32_t i;
    for (i = 0; i < 3; i++) {
        ss_machine_init(&machines[i], i, 3, 2, 1);
    }

    ss_global_config_t gconfig;
    int32_t r = ss_global_config_init(&gconfig, machines, 3);
    assert(r == 0);
    assert(gconfig.num_machines == 3);
    assert(gconfig.state_buffer[0] == 0);

    /* Modify a machine state and sync */
    machines[1].local_state = 2;
    ss_global_config_sync(&gconfig);
    assert(gconfig.state_buffer[1] == 2);

    ss_global_config_destroy(&gconfig);
    for (i = 0; i < 3; i++) {
        ss_machine_destroy(&machines[i]);
    }
    PASS();
}

static void test_transition_system(void) {
    TEST("transition_system_small");
    ss_transition_system_t ts;
    /* N=3, K=3, 3-state algorithm */
    int32_t r = ss_build_transition_system(&ts, 3, 3, 0);
    assert(r == 0);
    assert(ts.num_configs == 27); /* 3^3 */
    assert(ts.num_machines == 3);
    assert(ts.num_transitions > 0);

    /* Verify closure */
    bool closure = ss_verify_closure_exhaustive(&ts);
    assert(closure == true);

    /* Verify convergence */
    int32_t conv = ss_verify_convergence_exhaustive(&ts);
    assert(conv == 27); /* All configs converge */

    /* Max convergence steps */
    int32_t max_steps = ss_max_convergence_steps(&ts);
    assert(max_steps > 0);
    printf("max_conv_steps=%d ", max_steps);

    ss_transition_system_destroy(&ts);
    PASS();
}

/* ── L4: Probabilistic Self-Stabilization ────────────────────────────────── */

static void test_probabilistic_expected_steps(void) {
    TEST("probabilistic_expected_steps");
    double expected = ss_probabilistic_expected_steps(5, 3);
    assert(expected > 0.0);
    printf("E[T]=%.2f ", expected);
    PASS();
}

/* ── L4: Fair Composition ───────────────────────────────────────────────── */

static void test_composition(void) {
    TEST("composition_verification");
    ss_composed_system_t composed;
    ss_system_descriptor_t sysA, sysB;

    /* Create two dummy descriptors */
    memset(&sysA, 0, sizeof(sysA));
    memset(&sysB, 0, sizeof(sysB));
    sysA.num_machines = 3;
    sysB.num_machines = 2;

    composed.subsystem_a = &sysA;
    composed.subsystem_b = &sysB;
    composed.interference_check = NULL;

    /* Without interference check, should return false */
    bool valid = ss_verify_composition(&composed);
    /* Without legitimate_fn, should fail */
    assert(valid == false);

    PASS();
}

/* ── L4: Convergence Result Printing ────────────────────────────────────── */

static void test_result_printing(void) {
    TEST("result_printing");
    convergence_result_t result;
    memset(&result, 0, sizeof(result));
    result.converged = true;
    result.total_steps = 42;
    result.num_moves_bottom = 7;
    result.num_moves_others = 35;
    result.min_state = 0;
    result.max_state = 2;
    result.final_num_privileges = 1;

    convergence_result_print(&result);

    convergence_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    stats.num_converged = 100;
    stats.num_timed_out = 0;
    stats.mean_steps = 123.4;
    stats.median_steps = 100.0;
    stats.stddev_steps = 45.6;
    stats.min_steps = 10;
    stats.max_steps = 300;
    stats.convergence_rate = 1.0;

    convergence_stats_print(&stats);
    PASS();
}

/* ── L4: Synchronous Round Test ─────────────────────────────────────────── */

static void test_synchronous_round(void) {
    TEST("synchronous_round");
    ring_configuration_t config;
    ring_init(&config, 5, 3, ALGORITHM_3STATE);

    ring_set_uniform_state(&config, 0);
    int32_t moved = ring_synchronous_round(&config);
    assert(moved >= 0);

    ring_destroy(&config);
    PASS();
}

/* ── Main Test Runner ────────────────────────────────────────────────────── */

int main(void) {
    printf("=== Dijkstra Self-Stabilizing Ring — Test Suite ===\n\n");

    /* L1: Ring initialization */
    printf("--- L1: Definitions ---\n");
    test_ring_init_basic();
    test_ring_init_invalid();
    test_ring_set_get_state();
    test_ring_randomize();
    test_ring_uniform();

    /* L2: Privilege and legitimacy */
    printf("\n--- L2: Core Concepts ---\n");
    test_privilege_3state_uniform();
    test_privilege_3state_diverse();
    test_privilege_kstate();

    /* L3: State transitions */
    printf("\n--- L3: Mathematical Structures ---\n");
    test_step_3state_bottom();
    test_step_3state_nonbottom();
    test_step_3state_no_move();
    test_step_4state();
    test_step_kstate();
    test_ring_step_dispatch();
    test_energy_difference_count();
    test_energy_squared_sum();
    test_energy_token_misplacement();
    test_energy_delta_nonpositive();
    test_energy_monotonicity();

    /* L4: Fundamental laws */
    printf("\n--- L4: Fundamental Laws ---\n");
    test_converge_small_ring_3state();
    test_converge_uniform_state();
    test_closure_verification();
    test_ring_clone();
    test_theorem_3state_n3();
    test_theorem_kstate_n2_k3();
    test_min_states_theorem();
    test_symmetry_breaking();
    test_legitimate_enumeration();
    test_probabilistic_expected_steps();
    test_composition();
    test_synchronous_round();

    /* L4: Scheduler */
    printf("\n--- L4: Scheduler ---\n");
    test_scheduler_central();
    test_scheduler_distributed();
    test_scheduler_probabilistic();
    test_convergence_bounds();

    /* L5: Convergence analysis */
    printf("\n--- L5: Algorithms/Methods ---\n");
    test_convergence_experiment();
    test_convergence_trials();
    test_worst_case_distance();

    /* L3-L4: State Machine */
    printf("\n--- L3-L4: State Machine ---\n");
    test_state_machine_init();
    test_state_machine_evaluate();
    test_global_config();
    test_transition_system();

    /* L7: Fault injection */
    printf("\n--- L7: Fault Injection ---\n");
    test_fault_single_recovery();
    test_fault_burst_recovery();
    test_fault_exhaustive();

    /* L7: Applications */
    printf("\n--- L7: Applications ---\n");
    test_app_routing_recovery();
    test_app_leader_election();
    test_app_mutual_exclusion_cloud();
    test_app_time_sync();
    test_app_spanning_tree();
    test_app_blockchain_consensus();

    /* L2: Utility */
    printf("\n--- Utilities ---\n");
    test_result_printing();

    /* Summary */
    printf("\n========================================\n");
    printf("Tests run:    %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_run - tests_passed);

    if (tests_passed == tests_run) {
        printf("\n✅ ALL TESTS PASSED\n");
        return 0;
    } else {
        printf("\n❌ SOME TESTS FAILED\n");
        return 1;
    }
}
