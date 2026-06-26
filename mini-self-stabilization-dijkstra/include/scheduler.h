/**
 * @file scheduler.h
 * @brief Daemon / scheduler models for self-stabilizing systems
 *
 * A daemon (scheduler) is an adversary that selects which privileged
 * machine executes next. Different daemon models yield different
 * convergence guarantees. This module implements the three classic
 * daemon types and fairness policies.
 *
 * Daemon Types (Dolev, 2000):
 *   - Central daemon: exactly one privileged machine per step
 *   - Distributed daemon: any nonempty subset of privileged machines
 *   - Synchronous daemon: all privileged machines step simultaneously
 *
 * Reference:
 *   Dolev, S. "Self-Stabilization." MIT Press, 2000, Chapter 2.
 *   Dubois, S. & Tixeuil, S. "A Taxonomy of Daemons in Self-Stabilization."
 *     arXiv:1110.0334, 2011.
 *
 * Course Mappings:
 *   MIT 6.841 — Adversarial scheduling & fault models
 *   Cambridge Part III — Advanced distributed algorithms
 *   ETH 263-4650 — Daemon models & convergence analysis
 *
 * Knowledge Levels: L2 (Core Concepts), L3 (Math Structures)
 */

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── L1: Daemon Type Definitions ──────────────────────────────────────── */

/**
 * @brief Daemon / scheduler type.
 *
 * Determines the granularity of atomic steps in the distributed system.
 */
typedef enum {
    DAEMON_CENTRAL = 0,       /**< Central daemon: exactly 1 machine per step */
    DAEMON_DISTRIBUTED = 1,   /**< Distributed daemon: any nonempty subset */
    DAEMON_SYNCHRONOUS = 2,   /**< Synchronous daemon: all privileged machines */
    DAEMON_READWRITE = 3      /**< Read/write atomicity: read all, then write */
} daemon_type_t;

/**
 * @brief Fairness policy for daemon scheduling.
 *
 * Ensures that every continuously privileged machine eventually executes.
 */
typedef enum {
    FAIRNESS_NONE = 0,        /**< No fairness guarantee (adversarial) */
    FAIRNESS_WEAK = 1,        /**< Weak fairness: every continuously
                                   privileged machine eventually executes */
    FAIRNESS_STRONG = 2,      /**< Strong fairness: every infinitely often
                                   privileged machine executes infinitely often */
    FAIRNESS_ROUND_ROBIN = 3  /**< Round-robin: machines execute in order */
} fairness_policy_t;

/* ── L2: Scheduler State ───────────────────────────────────────────────── */

/**
 * @brief Scheduler instance — tracks execution state and fairness.
 *
 * The scheduler maintains a priority queue / execution log and
 * enforces the selected fairness policy.
 */
typedef struct {
    daemon_type_t    daemon_type;    /**< Daemon model */
    fairness_policy_t fairness;      /**< Fairness guarantee */
    int32_t          num_machines;   /**< Total machines in system */
    int32_t         *last_executed;  /**< Last step each machine executed */
    int32_t          current_step;   /**< Global step counter */
    bool            *privilege_mask; /**< Current privilege mask (size N) */
    int32_t          schedule_seed;  /**< Seed for reproducible scheduling */
} scheduler_t;

/**
 * @brief Initialize a scheduler.
 *
 * @param sched        Uninitialized scheduler
 * @param daemon_type  Daemon model to use
 * @param fairness     Fairness policy
 * @param num_machines Number of machines in the ring
 * @param seed         Seed for randomized scheduling decisions
 * @return             0 on success, -1 on error
 */
int32_t scheduler_init(scheduler_t *sched,
                       daemon_type_t daemon_type,
                       fairness_policy_t fairness,
                       int32_t num_machines,
                       uint32_t seed);

/**
 * @brief Destroy scheduler resources.
 */
void scheduler_destroy(scheduler_t *sched);

/* ── L2: Machine Selection ─────────────────────────────────────────────── */

/**
 * @brief Update the privilege mask from an external source.
 *
 * The scheduler needs to know which machines are currently privileged.
 * Call this before each scheduling decision.
 *
 * @param sched      Scheduler
 * @param privileges Array of booleans (size N), true = privileged
 */
void scheduler_update_privileges(scheduler_t *sched, const bool *privileges);

/**
 * @brief Select the next machine(s) to execute based on daemon type
 *        and fairness policy.
 *
 * For central daemon: returns exactly 1 machine ID in selected[0].
 * For distributed daemon: returns a nonempty subset.
 * For synchronous daemon: returns all privileged machines.
 *
 * @param sched      Scheduler
 * @param selected   Output array for selected machine IDs
 * @param capacity   Capacity of selected array
 * @return           Number of machines selected, 0 if none privileged, -1 error
 */
int32_t scheduler_select(scheduler_t *sched,
                         int32_t *selected,
                         int32_t capacity);

/**
 * @brief Record that a machine executed a step.
 *
 * Updates fairness bookkeeping.
 *
 * @param sched       Scheduler
 * @param machine_id  Machine that just executed
 * @return            0 on success, -1 on error
 */
int32_t scheduler_record_execution(scheduler_t *sched, int32_t machine_id);

/**
 * @brief Compute the starvation count — how many steps since each machine
 *        last executed. Useful for fairness analysis.
 *
 * @param sched    Scheduler
 * @param counts   Output array (size N), counts[i] = steps since last exec
 */
void scheduler_starvation_counts(const scheduler_t *sched, int32_t *counts);

/* ── L3: Probabilistic Scheduler ───────────────────────────────────────── */

/**
 * @brief Select machines using a probabilistic distributed daemon.
 *
 * Each privileged machine executes with independent probability p.
 * This models asynchronous message-passing systems with message loss.
 *
 * @param sched      Scheduler
 * @param privileges Privilege mask
 * @param p          Execution probability per privileged machine
 * @param selected   Output array
 * @param capacity   Capacity of selected array
 * @return           Number of machines selected
 */
int32_t scheduler_select_probabilistic(scheduler_t *sched,
                                        const bool *privileges,
                                        double p,
                                        int32_t *selected,
                                        int32_t capacity);

/* ── L4: Convergence Predictor ─────────────────────────────────────────── */

/**
 * @brief Estimate the worst-case convergence steps under the current
 *        daemon model for an N-machine K-state ring.
 *
 * Known bounds (Dijkstra, 1974; Dolev, 2000):
 *   Central daemon, 3-state:   O(N^2) worst case
 *   Central daemon, K-state:   O(N^2) worst case
 *   Distributed daemon, 3-state: O(N^2) worst case
 *   Synchronous daemon:        O(N) worst case
 *
 * @param n          Number of machines
 * @param k          Number of states
 * @param daemon     Daemon type
 * @return           Estimated maximum convergence steps (upper bound)
 */
int32_t scheduler_convergence_bound(int32_t n, int32_t k,
                                    daemon_type_t daemon);

/**
 * @brief Compute the expected convergence time for the probabilistic daemon.
 *
 * Uses the Markov chain analysis from Herman (1990) and Dolev (2000).
 *
 * @param n  Number of machines
 * @param k  Number of states
 * @param p  Execution probability
 * @return   Expected number of rounds (floating point)
 */
double scheduler_expected_convergence_rounds(int32_t n, int32_t k, double p);

#ifdef __cplusplus
}
#endif

#endif /* SCHEDULER_H */
