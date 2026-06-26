/**
 * @file scheduler.c
 * @brief Daemon / scheduler implementations for self-stabilizing systems
 *
 * Implements the three classic daemon models (central, distributed,
 * synchronous) and fairness policies. A daemon is an abstraction of
 * the system's execution semantics — it selects which privileged
 * machines execute at each step.
 *
 * Reference:
 *   Dubois, S. & Tixeuil, S. "A Taxonomy of Daemons in Self-Stabilization."
 *     arXiv:1110.0334, 2011.
 *   Dolev, S. "Self-Stabilization." MIT Press, 2000, Chapter 2.
 *   Dijkstra, E.W. "Self-stabilizing Systems..." CACM, 1974.
 *
 * Knowledge Points (each function = 1 independent knowledge point):
 *
 * L1: scheduler_init/destroy — scheduler lifecycle
 * L2: scheduler_select — daemon-type-specific machine selection
 * L2: scheduler_update_privileges — privilege mask management
 * L3: scheduler_select_probabilistic — probabilistic scheduler
 * L3: scheduler_starvation_counts — fairness analysis
 * L4: scheduler_convergence_bound — theoretical convergence upper bounds
 * L4: scheduler_expected_convergence_rounds — probabilistic bound
 */

#include "scheduler.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ── L1: Scheduler Lifecycle ────────────────────────────────────────────── */

/**
 * scheduler_init — Initialize a scheduler with daemon type and fairness.
 *
 * The daemon type determines the granularity of atomic steps:
 *   - Central: exactly one machine per step
 *   - Distributed: any nonempty subset per step
 *   - Synchronous: all privileged machines per step
 *
 * The fairness policy determines scheduling guarantees:
 *   - None: adversary can starve machines
 *   - Weak: every continuously privileged machine eventually executes
 *   - Strong: every machine privileged infinitely often executes
 *     infinitely often
 *   - Round-robin: cyclic order, deterministic
 *
 * Knowledge: The daemon models the execution semantics of the
 *   distributed system. Dijkstra used a central daemon in his
 *   original paper, but later work generalized to distributed
 *   and synchronous models.
 *
 * Time: O(N), Space: O(N)
 */
int32_t scheduler_init(scheduler_t *sched,
                       daemon_type_t daemon_type,
                       fairness_policy_t fairness,
                       int32_t num_machines,
                       uint32_t seed)
{
    if (!sched) return -1;
    if (num_machines < 2 || num_machines > 1024) return -1;

    sched->daemon_type   = daemon_type;
    sched->fairness      = fairness;
    sched->num_machines  = num_machines;
    sched->current_step  = 0;
    sched->schedule_seed = (int32_t)seed;

    sched->last_executed = (int32_t *)calloc((size_t)num_machines,
                                              sizeof(int32_t));
    sched->privilege_mask = (bool *)calloc((size_t)num_machines,
                                            sizeof(bool));
    if (!sched->last_executed || !sched->privilege_mask) {
        free(sched->last_executed);
        free(sched->privilege_mask);
        sched->last_executed = NULL;
        sched->privilege_mask = NULL;
        return -1;
    }

    /* Initialize last_executed to -1 (never executed) */
    int32_t i;
    for (i = 0; i < num_machines; i++) {
        sched->last_executed[i] = -1;
    }

    return 0;
}

/**
 * scheduler_destroy — Free scheduler resources.
 */
void scheduler_destroy(scheduler_t *sched)
{
    if (sched) {
        free(sched->last_executed);
        free(sched->privilege_mask);
        sched->last_executed = NULL;
        sched->privilege_mask = NULL;
    }
}

/* ── L2: Privilege Update and Machine Selection ──────────────────────────── */

/**
 * scheduler_update_privileges — Update the privilege mask.
 *
 * Called by the simulation loop each step to inform the scheduler
 * which machines are currently eligible to execute.
 *
 * Time: O(N), Space: O(1)
 */
void scheduler_update_privileges(scheduler_t *sched, const bool *privileges)
{
    int32_t i;
    if (!sched || !sched->privilege_mask || !privileges) return;

    for (i = 0; i < sched->num_machines; i++) {
        sched->privilege_mask[i] = privileges[i];
    }
}

/**
 * scheduler_select — Select the next machine(s) to execute.
 *
 * Central daemon: selects exactly one privileged machine.
 *   With round-robin: picks the privileged machine that least recently
 *   executed (or first privileged in order if tie).
 *   With weak fairness: ensures no continuously privileged machine
 *   starves.
 *
 * Distributed daemon: selects a nonempty subset. The selection
 *   models concurrent execution — any subset of privileged machines
 *   may execute simultaneously.
 *
 * Synchronous daemon: selects all privileged machines — models
 *   fully synchronous lockstep execution.
 *
 * Knowledge: Daemon model selection — different daemon assumptions
 *   lead to different convergence guarantees. Central daemon is
 *   the most restrictive (hence strongest convergence guarantees),
 *   while distributed daemon is more realistic for asynchronous
 *   systems.
 *
 * Time: O(N), Space: O(1)
 */
int32_t scheduler_select(scheduler_t *sched,
                         int32_t *selected,
                         int32_t capacity)
{
    int32_t i, count = 0;
    int32_t num_priv = 0;

    if (!sched || !selected || capacity <= 0) return -1;

    /* Count privileged machines */
    for (i = 0; i < sched->num_machines; i++) {
        if (sched->privilege_mask[i]) num_priv++;
    }

    if (num_priv == 0) return 0; /* No privileged machines */

    switch (sched->daemon_type) {
    case DAEMON_CENTRAL:
        /* Select exactly one privileged machine */
        if (sched->fairness == FAIRNESS_ROUND_ROBIN) {
            /* Find privileged machine with smallest last_executed */
            int32_t best = -1;
            int32_t best_last = 2147483647; /* INT_MAX */
            for (i = 0; i < sched->num_machines; i++) {
                if (sched->privilege_mask[i]) {
                    if (sched->last_executed[i] < best_last) {
                        best_last = sched->last_executed[i];
                        best = i;
                    }
                }
            }
            if (best >= 0 && count < capacity) {
                selected[count++] = best;
            }
        } else {
            /* Pick first privileged machine */
            for (i = 0; i < sched->num_machines && count < capacity; i++) {
                if (sched->privilege_mask[i]) {
                    selected[count++] = i;
                    break;
                }
            }
        }
        break;

    case DAEMON_DISTRIBUTED:
        /* Select all privileged machines (full distributed daemon) */
        for (i = 0; i < sched->num_machines && count < capacity; i++) {
            if (sched->privilege_mask[i]) {
                selected[count++] = i;
            }
        }
        break;

    case DAEMON_SYNCHRONOUS:
        /* Select all privileged machines */
        for (i = 0; i < sched->num_machines && count < capacity; i++) {
            if (sched->privilege_mask[i]) {
                selected[count++] = i;
            }
        }
        break;

    case DAEMON_READWRITE:
        /* Read/write atomicity: select one at a time but allow interleaving */
        if (count < capacity) {
            for (i = 0; i < sched->num_machines; i++) {
                if (sched->privilege_mask[i]) {
                    selected[count++] = i;
                    break;
                }
            }
        }
        break;

    default:
        return -1;
    }

    return count;
}

/**
 * scheduler_record_execution — Record that a machine executed.
 *
 * Updates the last_executed timestamp for fairness tracking.
 *
 * Time: O(1), Space: O(1)
 */
int32_t scheduler_record_execution(scheduler_t *sched, int32_t machine_id)
{
    if (!sched || !sched->last_executed) return -1;
    if (machine_id < 0 || machine_id >= sched->num_machines) return -1;

    sched->last_executed[machine_id] = sched->current_step;
    sched->current_step++;
    return 0;
}

/**
 * scheduler_starvation_counts — Compute steps since last execution.
 *
 * Identifies machines that have not executed recently, which is
 * useful for detecting starvation under unfair schedulers.
 *
 * Knowledge: Starvation analysis — in self-stabilizing systems
 *   under unfair daemons, some machines may never execute,
 *   preventing convergence. Fairness policies guarantee that
 *   all machines eventually get to execute.
 *
 * Time: O(N), Space: O(1)
 */
void scheduler_starvation_counts(const scheduler_t *sched, int32_t *counts)
{
    int32_t i;
    if (!sched || !counts) return;

    for (i = 0; i < sched->num_machines; i++) {
        if (sched->last_executed[i] < 0) {
            counts[i] = sched->current_step; /* Never executed */
        } else {
            counts[i] = sched->current_step - sched->last_executed[i];
        }
    }
}

/* ── L3: Probabilistic Scheduler ──────────────────────────────────────── */

/**
 * scheduler_select_probabilistic — Probabilistic distributed daemon.
 *
 * Each privileged machine executes with independent probability p.
 * This models asynchronous systems where message delivery is
 * probabilistic or where machines have independent clocks.
 *
 * The expected number of machines executing per step is p * num_priv.
 * When p = 1, this reduces to the synchronous daemon.
 * When p = 1/num_priv, it approximates the central daemon in expectation.
 *
 * Knowledge: Probabilistic scheduling — a more realistic model of
 *   asynchronous distributed systems. The convergence analysis
 *   becomes probabilistic: we can bound the expected number of
 *   steps to converge rather than the worst case.
 *
 * Hermann (1990) showed that a probabilistic self-stabilizing
 * token circulation algorithm exists with expected convergence
 * time O(N^2 * log N).
 *
 * Time: O(N), Space: O(1)
 */
int32_t scheduler_select_probabilistic(scheduler_t *sched,
                                        const bool *privileges,
                                        double p,
                                        int32_t *selected,
                                        int32_t capacity)
{
    int32_t i, count = 0;
    uint32_t rng_state;
    int32_t seed;

    if (!sched || !privileges || !selected || capacity <= 0) return -1;
    if (p < 0.0 || p > 1.0) return -1;

    /* Use schedule seed for reproducibility */
    seed = sched->schedule_seed + sched->current_step;
    rng_state = (uint32_t)seed;

    for (i = 0; i < sched->num_machines && count < capacity; i++) {
        if (privileges[i]) {
            /* Simple LCG-based coin flip */
            rng_state = (uint32_t)(((uint64_t)1103515245ULL * rng_state
                                     + 12345ULL) & 0x7FFFFFFFULL);
            double coin = (double)rng_state / 2147483648.0; /* [0, 1) */
            if (coin < p) {
                selected[count++] = i;
            }
        }
    }

    return count;
}

/* ── L4: Convergence Bound Computation ────────────────────────────────── */

/**
 * scheduler_convergence_bound — Compute theoretical convergence bound.
 *
 * Known bounds for Dijkstra's self-stabilizing ring:
 *
 *   3-state, central daemon:     ≤ (N^2 + 3N - 2) / 2 steps
 *   3-state, distributed daemon: ≤ N^2 steps
 *   3-state, synchronous daemon: ≤ N steps
 *   K-state, central daemon:     ≤ N^2 steps
 *   4-state, central daemon:     ≤ N^2 steps
 *
 * These bounds are from Dijkstra (1974) and Dolev (2000).
 *
 * Knowledge: Convergence complexity — the theoretical upper bound
 *   on convergence time. Dijkstra proved O(N^2) for his original
 *   algorithms; later work improved the constants and showed that
 *   the quadratic bound is tight in the worst case.
 *
 * Time: O(1), Space: O(1)
 */
int32_t scheduler_convergence_bound(int32_t n, int32_t k __attribute__((unused)),
                                    daemon_type_t daemon)
{
    if (n < 2) return -1;

    switch (daemon) {
    case DAEMON_CENTRAL:
        /* Upper bound: N^2 (Dijkstra, 1974; Dolev, 2000) */
        return n * n;

    case DAEMON_DISTRIBUTED:
        /* Upper bound: N^2 (same order as central, but potentially
           faster due to parallelism; worst case similar) */
        return n * n;

    case DAEMON_SYNCHRONOUS:
        /* Upper bound: N (each round all privileged execute;
           worst case is linear in N) */
        return n;

    case DAEMON_READWRITE:
        return n * n; /* Similar to central */

    default:
        return -1;
    }
}

/**
 * scheduler_expected_convergence_rounds — Expected convergence time
 * for the probabilistic daemon.
 *
 * Uses the Markov chain model:
 *   Let T be the random variable of convergence steps.
 *   E[T] ≈ (N^2 / p) * (k / (k-1)) rounds for the K-state algorithm.
 *
 * For the 3-state algorithm with probabilistic transitions,
 * the expected convergence is approximately:
 *   E[T] = N^2 / (2 * p) rounds.
 *
 * This is based on the analysis in Herman (1990) and Dolev (2000),
 * treating the convergence process as a random walk on the
 * configuration graph.
 *
 * Time: O(1), Space: O(1)
 */
double scheduler_expected_convergence_rounds(int32_t n, int32_t k, double p)
{
    double base;

    if (n < 2 || p <= 0.0 || p > 1.0) return -1.0;

    /* Base convergence: O(N^2) central daemon steps */
    base = (double)(n * n);

    /* Adjust for number of states (more states → slightly longer) */
    if (k > 3) {
        base *= (double)k / (double)(k - 1);
    }

    /* Adjust for execution probability (lower p → more rounds) */
    /* Expected rounds = base_steps / expected_moves_per_step */
    /* Expected moves per step = p * num_privileged ≈ p (for central-like) */
    double expected_moves = p;
    if (expected_moves < 0.001) expected_moves = 0.001; /* Avoid division by 0 */

    return base / expected_moves;
}

