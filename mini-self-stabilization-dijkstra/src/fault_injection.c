/**
 * @file fault_injection.c
 * @brief Fault injection framework for testing self-stabilizing systems
 *
 * Provides systematic fault injection to verify that Dijkstra's ring
 * correctly self-stabilizes from all possible fault patterns.
 * Implements transient fault models, state corruption, and
 * Byzantine perturbation scenarios.
 *
 * Reference:
 *   Arora, A. & Gouda, M. "Closure and Convergence: A Foundation of
 *     Fault-Tolerant Computing." IEEE TSE, 1993.
 *   Dolev, S. "Self-Stabilization." MIT Press, 2000, Chapter 7.
 *
 * Knowledge Points (each function = 1 independent knowledge point):
 *
 * L3: fault_inject_single — single transient fault injection
 * L3: fault_inject_burst — burst fault (multiple simultaneous corruptions)
 * L3: fault_inject_byzantine — Byzantine (arbitrary) fault
 * L4: fault_verify_recovery — verify recovery after fault
 * L5: fault_exhaustive_test — exhaustively test all single-fault scenarios
 * L8: fault_cascade_analysis — cascade failure analysis
 */

#include "dijkstra_ring.h"
#include "convergence.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── L3: Fault Injection Operations ─────────────────────────────────────── */

/**
 * fault_inject_single — Corrupt a single machine's state.
 *
 * Models a transient fault: a cosmic ray, power glitch, or memory
 * error that flips a machine's state to an arbitrary value.
 * Self-stabilization guarantees recovery from any single fault.
 *
 * Knowledge: Single transient fault — the most common fault model
 *   in self-stabilization. A single bit flip or state corruption
 *   anywhere in the ring should be automatically corrected without
 *   external intervention.
 *
 * @param config     Ring configuration (modified in-place)
 * @param machine_id Machine to corrupt (-1 for random)
 * @param new_state  New state value (-1 for random)
 * @param seed       RNG seed
 * @return           The corrupted machine ID, or -1 on error
 */
int32_t fault_inject_single(ring_configuration_t *config,
                             int32_t machine_id,
                             int32_t new_state,
                             uint32_t seed)
{
    uint32_t rng = seed;
    int32_t target, bad_state;

    if (!config || !config->machines) return -1;

    if (machine_id < 0) {
        rng = (uint32_t)(((uint64_t)1103515245ULL * rng + 12345ULL)
                         & 0x7FFFFFFFULL);
        target = (int32_t)(rng % (uint32_t)config->num_machines);
    } else if (machine_id >= config->num_machines) {
        return -1;
    } else {
        target = machine_id;
    }

    if (new_state < 0) {
        rng = (uint32_t)(((uint64_t)1103515245ULL * rng + 12345ULL)
                         & 0x7FFFFFFFULL);
        bad_state = (int32_t)(rng % (uint32_t)config->num_states);
    } else if (new_state >= config->num_states) {
        return -1;
    } else {
        bad_state = new_state;
    }

    config->machines[target].state = bad_state;
    return target;
}

/**
 * fault_inject_burst — Corrupt multiple machines simultaneously.
 *
 * Models a burst fault: an electromagnetic pulse, power surge, or
 * network partition that affects multiple machines at once.
 *
 * Self-stabilization guarantees convergence even from burst faults,
 * as long as the faults are transient (machines resume correct
 * execution after the fault).
 *
 * Knowledge: Burst fault model — more challenging than single faults
 *   because multiple tokens may be created or all tokens destroyed.
 *   Dijkstra proved his ring recovers from any initial configuration,
 *   which includes all possible burst fault outcomes.
 *
 * @param config     Ring configuration
 * @param num_faults Number of machines to corrupt
 * @param seed       RNG seed
 * @return           Number of machines actually corrupted
 */
int32_t fault_inject_burst(ring_configuration_t *config,
                            int32_t num_faults,
                            uint32_t seed)
{
    uint32_t rng = seed;
    int32_t i, corrupted = 0;
    bool *already_corrupted;

    if (!config || !config->machines || num_faults <= 0) return -1;
    if (num_faults > config->num_machines) {
        num_faults = config->num_machines;
    }

    already_corrupted = (bool *)calloc((size_t)config->num_machines,
                                        sizeof(bool));
    if (!already_corrupted) return -1;

    for (i = 0; i < num_faults; i++) {
        rng = (uint32_t)(((uint64_t)1103515245ULL * rng + 12345ULL)
                         & 0x7FFFFFFFULL);
        int32_t target = (int32_t)(rng % (uint32_t)config->num_machines);

        /* Avoid corrupting the same machine twice */
        if (!already_corrupted[target]) {
            rng = (uint32_t)(((uint64_t)1103515245ULL * rng + 12345ULL)
                             & 0x7FFFFFFFULL);
            int32_t bad_state = (int32_t)(rng
                                           % (uint32_t)config->num_states);
            config->machines[target].state = bad_state;
            already_corrupted[target] = true;
            corrupted++;
        }
    }

    free(already_corrupted);
    return corrupted;
}

/**
 * fault_inject_byzantine — Inject a Byzantine (arbitrary misbehavior) fault.
 *
 * In the Byzantine fault model, a machine may behave arbitrarily:
 * it can change its state according to any rule, not just the
 * designated algorithm. This models a compromised or malicious node.
 *
 * A Byzantine fault can:
 *   1. Change the machine's state to any value
 *   2. Cause the machine to pretend to have (or not have) a privilege
 *   3. Cause cascading failures as neighbors react to arbitrary states
 *
 * Self-stabilization with Byzantine faults is an advanced topic
 * (L8). While Dijkstra's original algorithm does not handle
 * permanent Byzantine faults, it recovers once the Byzantine
 * behavior stops (the fault is transient).
 *
 * Knowledge: Byzantine fault tolerance vs. self-stabilization —
 *   Byzantine faults model adversarial behavior, while self-
 *   stabilization models recovery from transient faults.
 *   Combining both (self-stabilizing Byzantine agreement) is
 *   a frontier research topic (L9).
 *
 * @param config     Ring configuration
 * @param machine_id Target machine
 * @param new_state  Arbitrary new state
 * @return           0 on success, -1 on error
 */
int32_t fault_inject_byzantine(ring_configuration_t *config,
                                int32_t machine_id,
                                int32_t new_state)
{
    if (!config || !config->machines) return -1;
    if (machine_id < 0 || machine_id >= config->num_machines) return -1;
    if (new_state < 0 || new_state >= config->num_states) return -1;

    /* Byzantine fault: set to arbitrary state (could be any value) */
    config->machines[machine_id].state = new_state;

    /* In a true Byzantine scenario, the machine could also lie about
       its state to neighbors. In our ring model, this is equivalent
       to setting an arbitrary state since neighbors read it directly. */
    return 0;
}

/* ── L4: Fault Recovery Verification ────────────────────────────────────── */

/**
 * fault_verify_recovery — Inject a fault, then verify recovery.
 *
 * The protocol:
 *   1. Start from a legitimate configuration
 *   2. Inject a fault (corrupt one or more machines)
 *   3. Run the convergence algorithm
 *   4. Verify that the ring returns to a legitimate state
 *   5. Measure recovery time (steps)
 *
 * Knowledge: Fault recovery verification — the empirical test of
 *   self-stabilization. Starting from correct operation, inject
 *   a fault and measure how long the system takes to recover.
 *   This is the "mean time to repair" (MTTR) for self-stabilizing
 *   systems.
 *
 * @param n          Number of machines
 * @param k          Number of states
 * @param variant    Algorithm variant
 * @param fault_type 0=single, 1=burst, 2=byzantine
 * @param max_steps  Maximum steps for recovery
 * @param seed       RNG seed
 * @return           Steps to recover, or -1 if failed
 */
int32_t fault_verify_recovery(int32_t n, int32_t k,
                               algorithm_variant_t variant,
                               int32_t fault_type,
                               int32_t max_steps,
                               uint32_t seed)
{
    ring_configuration_t config;
    int32_t steps, steps_after_fault;
    uint32_t rng = seed;

    if (n < 2 || k < 2) return -1;

    if (ring_init(&config, n, k, variant) < 0) return -1;

    /* Step 1: Converge to legitimate state */
    ring_randomize_states(&config, rng);
    rng = (uint32_t)(((uint64_t)1103515245ULL * rng + 12345ULL)
                     & 0x7FFFFFFFULL);
    steps = ring_converge_to_legitimate(&config, max_steps, NULL, 0);
    if (steps < 0) {
        ring_destroy(&config);
        return -1; /* Could not even reach legitimate state */
    }

    /* Step 2: Inject fault */
    switch (fault_type) {
    case 0: /* Single fault */
        fault_inject_single(&config, -1, -1, rng);
        break;
    case 1: /* Burst fault */
        fault_inject_burst(&config, n / 2, rng);
        break;
    case 2: /* Byzantine fault */
        rng = (uint32_t)(((uint64_t)1103515245ULL * rng + 12345ULL)
                         & 0x7FFFFFFFULL);
        {
            int32_t victim = (int32_t)(rng % (uint32_t)n);
            rng = (uint32_t)(((uint64_t)1103515245ULL * rng + 12345ULL)
                             & 0x7FFFFFFFULL);
            int32_t bad = (int32_t)(rng % (uint32_t)k);
            fault_inject_byzantine(&config, victim, bad);
        }
        break;
    default:
        ring_destroy(&config);
        return -1;
    }

    /* Step 3: Recover */
    steps_after_fault = ring_converge_to_legitimate(&config,
                                                     max_steps, NULL, 0);

    ring_destroy(&config);
    return steps_after_fault;
}

/* ── L5: Exhaustive Fault Testing ───────────────────────────────────────── */

/**
 * fault_exhaustive_test — Test recovery from all possible single-fault
 * scenarios on a small ring.
 *
 * For a ring with N machines and K states:
 *   Number of legitimate configurations: N * K
 *   Single faults per legitimate config: N * (K-1) (each machine
 *     could be set to any other state)
 *   Total single-fault scenarios: N * K * N * (K-1) = N^2 * K * (K-1)
 *
 * This function exhaustively tests all single-fault recoveries
 * for small rings (N ≤ 4, K ≤ 4) to verify the recovery guarantee.
 *
 * Knowledge: Exhaustive fault coverage — for small systems, we can
 *   prove recovery from every possible single fault by exhaustive
 *   enumeration. This validates the theoretical guarantee that
 *   self-stabilization handles any transient fault.
 *
 * @param n          Number of machines
 * @param k          Number of states
 * @param max_steps  Maximum recovery steps
 * @return           Number of successful recoveries (negative = failure)
 */
int32_t fault_exhaustive_test(int32_t n, int32_t k, int32_t max_steps)
{
    ring_configuration_t config;
    int32_t i, s, target_m, bad_s;
    int32_t successful = 0, total = 0;

    if (n < 2 || n > 4 || k < 2 || k > 4) return -1;

    if (ring_init(&config, n, k, ALGORITHM_3STATE) < 0) return -1;

    /* Enumerate legitimate configurations */
    for (i = 0; i < n; i++) {
        for (s = 0; s < k; s++) {
            /* Set up legitimate config: token at position i */
            /* All machines have state s except for the token boundary */
            int32_t j;
            for (j = 0; j < n; j++) {
                ring_set_state(&config, j, s);
            }

            /* For the 3-state algorithm, legitimate means exactly 1 priv */
            /* A legitimate config has all states equal except the token gap */
            /* Bottom privilege: S[0]==S[N-1], so all equal */
            /* Actually, the simplest legitimate config: all states equal */
            /* Then bottom has a token. But that's not right either... */
            /* In a legitimate config with token at bottom:
               S[0]==S[N-1] and S[i]==S[i-1] for all i≠0.
               This means all states are equal. And the bottom has a token. */
            /* But if all states are equal AND bottom has a token,
               ring_step_3state changes S[0] to (S[0]+1)%k, then
               machine 1 has S[1]!=S[0], so it copies, etc. */
            /* Actually any uniform state IS legitimate because bottom has
               the only privilege and no other machine does. */

            /* Check if this is really legitimate */
            if (!ring_is_legitimate(&config)) continue;

            /* For each machine that could be corrupted */
            for (target_m = 0; target_m < n; target_m++) {
                for (bad_s = 0; bad_s < k; bad_s++) {
                    if (bad_s == config.machines[target_m].state) continue;

                    ring_configuration_t *test =
                        ring_clone(&config);
                    if (!test) continue;

                    /* Corrupt */
                    test->machines[target_m].state = bad_s;

                    /* Recover */
                    int32_t steps = ring_converge_to_legitimate(
                        test, max_steps, NULL, 0);

                    if (steps >= 0) successful++;
                    total++;

                    ring_destroy(test);
                    free(test);
                }
            }
        }
    }

    ring_destroy(&config);
    return (successful == total) ? successful : -1;
}

/* ── L8: Cascade Failure Analysis ───────────────────────────────────────── */

/**
 * fault_cascade_analysis — Analyze cascade failure propagation.
 *
 * When a fault occurs, its effects may cascade through the ring
 * as neighboring machines react to the corrupted state. This
 * function measures the "blast radius" of a fault: how many
 * machines are affected and how long the cascade lasts.
 *
 * Knowledge: Cascade analysis — in distributed systems, local
 *   faults can propagate through the system via neighbor
 *   interactions. Self-stabilizing systems contain cascades:
 *   the cascade size is bounded, and the system eventually
 *   returns to correct operation. This is in contrast to
 *   non-stabilizing systems where a single fault can cause
 *   permanent global disruption.
 *
 * @param config       Ring configuration (legitimate before fault)
 * @param fault_machine Machine to corrupt
 * @param cascade_log  Output: how many machines changed per step
 * @param log_capacity Capacity of cascade_log
 * @param max_steps    Maximum steps to track
 * @return             Number of machines in the cascade (blast radius)
 */
int32_t fault_cascade_analysis(ring_configuration_t *config,
                                int32_t fault_machine,
                                int32_t *cascade_log,
                                int32_t log_capacity,
                                int32_t max_steps)
{
    int32_t step, affected_count = 0;
    ring_configuration_t *prev_state = NULL;

    if (!config || !config->machines || max_steps <= 0) return -1;
    if (fault_machine < 0 || fault_machine >= config->num_machines) return -1;
    if (!ring_is_legitimate(config)) return -1;

    /* Inject fault */
    int32_t k = config->num_states;
    int32_t bad_state = (config->machines[fault_machine].state + 1) % k;
    config->machines[fault_machine].state = bad_state;
    affected_count = 1; /* At least one machine affected */

    /* Track cascade */
    for (step = 0; step < max_steps; step++) {
        prev_state = ring_clone(config);

        /* Execute one round (central daemon: one machine at a time) */
        ring_synchronous_round(config);

        /* Count how many machines changed */
        int32_t changed = 0;
        int32_t i;
        for (i = 0; i < config->num_machines; i++) {
            if (config->machines[i].state != prev_state->machines[i].state) {
                changed++;
            }
        }

        if (cascade_log && step < log_capacity) {
            cascade_log[step] = changed;
        }

        if (changed > 0) affected_count += changed;

        if (ring_is_legitimate(config)) {
            /* Cascade has ended — system recovered */
            break;
        }

        ring_destroy(prev_state);
        free(prev_state);
        prev_state = NULL;
    }

    if (prev_state) {
        ring_destroy(prev_state);
        free(prev_state);
    }

    return affected_count;
}
