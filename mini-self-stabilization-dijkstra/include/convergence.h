/**
 * @file convergence.h
 * @brief Convergence analysis and metrics for self-stabilizing systems
 *
 * Provides tools for analyzing the convergence behavior of Dijkstra's
 * self-stabilizing ring and other self-stabilizing algorithms.
 * Includes convergence time measurement, attractor analysis,
 * and Lyapunov-like energy functions for distributed systems.
 *
 * Reference:
 *   Dijkstra, E.W. "Self-stabilizing Systems in Spite of Distributed Control."
 *     CACM, 1974.
 *   Dolev, S. "Self-Stabilization." MIT Press, 2000, Chapters 3-4.
 *   Beauquier, J. et al. "Space and Time Efficient Self-Stabilizing
 *     Algorithms." Distributed Computing, 2001.
 *
 * Course Mappings:
 *   MIT 6.841 — Convergence & closure proofs
 *   Princeton COS 551 — Lyapunov methods for distributed systems
 *   CMU 15-855 — Convergence complexity analysis
 *
 * Knowledge Levels: L2 (Core Concepts), L4 (Fundamental Laws)
 */

#ifndef CONVERGENCE_H
#define CONVERGENCE_H

#include <stdint.h>
#include <stdbool.h>
#include "dijkstra_ring.h"
#include "state_machine.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── L1: Convergence Metrics ───────────────────────────────────────────── */

/**
 * @brief Convergence result — the outcome of a convergence experiment.
 */
typedef struct {
    int32_t total_steps;          /**< Steps taken to reach legitimate state */
    int32_t num_moves_bottom;     /**< Moves by the bottom machine */
    int32_t num_moves_others;     /**< Moves by non-bottom machines */
    int32_t max_state;            /**< Maximum state value observed */
    int32_t min_state;            /**< Minimum state value observed */
    double  avg_states;           /**< Average state value during convergence */
    bool    converged;            /**< Whether convergence was achieved */
    int32_t final_num_privileges; /**< Number of privileges at end */
} convergence_result_t;

/**
 * @brief Run a convergence experiment on a random initial configuration.
 *
 * @param n          Number of machines
 * @param k          Number of states
 * @param variant    Algorithm variant
 * @param daemon_type Daemon model (0=central, 1=distributed, 2=synchronous)
 * @param max_steps  Maximum steps before aborting
 * @param seed       Random seed for initial state generation
 * @return           Convergence result (caller frees memory)
 */
convergence_result_t *convergence_run_experiment(int32_t n, int32_t k,
                                                  algorithm_variant_t variant,
                                                  int32_t daemon_type,
                                                  int32_t max_steps,
                                                  uint32_t seed);

/**
 * @brief Run multiple convergence experiments and compute statistics.
 *
 * @param n            Number of machines
 * @param k            Number of states
 * @param variant      Algorithm variant
 * @param num_trials   Number of independent trials
 * @param max_steps    Maximum steps per trial
 * @param seed         Random seed
 * @return             Array of num_trials convergence results
 */
convergence_result_t *convergence_run_trials(int32_t n, int32_t k,
                                              algorithm_variant_t variant,
                                              int32_t num_trials,
                                              int32_t max_steps,
                                              uint32_t seed);

/**
 * @brief Compute summary statistics from trial results.
 */
typedef struct {
    double  mean_steps;       /**< Mean steps to converge */
    double  median_steps;     /**< Median steps to converge */
    double  stddev_steps;     /**< Standard deviation */
    int32_t min_steps;        /**< Minimum steps observed */
    int32_t max_steps;        /**< Maximum steps observed */
    int32_t num_converged;    /**< Number of trials that converged */
    int32_t num_timed_out;    /**< Number of trials that timed out */
    double  convergence_rate; /**< Fraction that converged */
} convergence_stats_t;

/**
 * @brief Compute summary statistics from trial results.
 *
 * @param results    Array of convergence results
 * @param num_trials Number of trials
 * @param stats      Output statistics
 */
void convergence_compute_stats(const convergence_result_t *results,
                               int32_t num_trials,
                               convergence_stats_t *stats);

/* ── L2: Attractor / Basin Analysis ────────────────────────────────────── */

/**
 * @brief Attractor — a set of configurations closed under transitions.
 *
 * In self-stabilization, the set of legitimate configurations L
 * is an attractor: once the system enters L, it never leaves.
 * This structure represents an attractor and its basin.
 */
typedef struct {
    int32_t *config_ids;      /**< Configurations in the attractor */
    int32_t  num_configs;     /**< Size of the attractor */
    int32_t  basin_size;      /**< Number of configs that converge to this */
    bool     is_minimal;       /**< True if no proper subset is an attractor */
} attractor_t;

/**
 * @brief Find all attractors in a transition system.
 *
 * Uses Kosaraju-like SCC decomposition to find bottom SCCs
 * (attractors may contain cycles but no outgoing edges).
 *
 * @param ts         Transition system
 * @param attractors Output array of attractors (caller frees)
 * @param capacity   Capacity of output array
 * @return           Number of attractors found
 */
int32_t convergence_find_attractors(const ss_transition_system_t *ts,
                                     attractor_t *attractors,
                                     int32_t capacity);

/* ── L3: Energy / Potential Functions ──────────────────────────────────── */

/**
 * @brief Compute a Lyapunov-like energy function for a ring configuration.
 *
 * For Dijkstra's 3-state ring, a natural energy function is the total
 * number of "inversions" or the count of adjacent state differences.
 * This function decreases monotonically (in expectation) as the ring
 * converges to a legitimate state.
 *
 * Energy E(config) = Σ_i δ(S[i], S[i-1]) where δ(x,y) = 0 if x=y else 1
 *
 * In a legitimate state, E = N — exactly one adjacent pair differs.
 *
 * @param config Ring configuration
 * @return       Energy value (lower is closer to legitimate)
 */
int32_t convergence_energy_difference_count(const ring_configuration_t *config);

/**
 * @brief Alternative energy function: sum of squared state values.
 *
 * Useful for K-state algorithms where state magnitude correlates
 * with distance from legitimate configuration.
 *
 * E_sq(config) = Σ_i S[i]^2
 *
 * @param config Ring configuration
 * @return       Squared energy value
 */
int64_t convergence_energy_squared_sum(const ring_configuration_t *config);

/**
 * @brief Third energy function: token misplacement metric.
 *
 * Counts the number of state values that differ from their
 * "correct" value in a perfectly rotating token pattern.
 *
 * @param config Ring configuration
 * @return       Misplacement count
 */
int32_t convergence_energy_token_misplacement(const ring_configuration_t *config);

/**
 * @brief Compute the energy delta for a potential state transition.
 *
 * ΔE = E(config_after) - E(config_before)
 *
 * For Dijkstra's ring with the difference-count energy function,
 * ΔE ≤ 0 for all legal moves (the energy never increases).
 *
 * @param config     Current configuration
 * @param machine_id Machine that would execute
 * @return           ΔE (negative = energy decrease)
 */
int32_t convergence_energy_delta(const ring_configuration_t *config,
                                  int32_t machine_id);

/* ── L4: Theorem Verification ─────────────────────────────────────────── */

/**
 * @brief Verify the monotonicity property for non-bottom machines:
 *        no non-bottom legal step increases the energy function.
 *
 * For Dijkstra's 3-state algorithm, non-bottom moves never increase
 * the difference-count energy. Bottom machine moves can temporarily
 * increase it (creating a new token value), which is necessary for
 * progress.
 *
 * Exhaustively checks all possible configurations (for small N, K).
 *
 * @param n Number of machines
 * @param k Number of states
 * @return  true if non-bottom moves are non-increasing for all configs
 */
bool convergence_verify_monotonicity(int32_t n, int32_t k);

/**
 * @brief Verify the energy function correctly identifies legitimate states.
 *
 * Checks that all legitimate configurations have energy ≤ 1 (using the
 * difference-count energy function), confirming that the energy function
 * correctly separates legitimate from non-legitimate states.
 *
 * @param n Number of machines
 * @param k Number of states
 * @return  true if legitimate configs have energy ≤ 1
 */
bool convergence_verify_energy_property(int32_t n, int32_t k);

/**
 * @brief Compute the worst-case convergence distance for exhaustive
 *        enumeration of all configurations.
 *
 * For small N and K (N ≤ 5, K ≤ 4), exhaustively simulates all
 * initial configurations and computes the maximum number of steps
 * needed (under central daemon, worst-case scheduling).
 *
 * @param n       Number of machines
 * @param k       Number of states
 * @param variant Algorithm variant
 * @param worst_config_state Output: the worst initial configuration states
 * @return                 Maximum steps to converge
 */
int32_t convergence_worst_case_distance(int32_t n, int32_t k,
                                         algorithm_variant_t variant,
                                         int32_t *worst_config_state);

/* ── L8: Time-Adaptive Self-Stabilization ─────────────────────────────────── */

/**
 * @brief Time-adaptive convergence bound.
 *
 * In time-adaptive self-stabilization (Kutten & Patt-Shamir, 1997),
 * the convergence time adapts to the actual severity of the fault:
 *   - Minor faults (single state corruption): O(1) recovery
 *   - Moderate faults (few corruptions): O(d) where d = fault distance
 *   - Catastrophic faults (arbitrary initial state): O(N²) worst case
 *
 * This function computes the adaptive bound for Dijkstra's 3-state ring
 * given the distance of the initial configuration from legitimacy.
 *
 * Knowledge: Time-adaptive stabilization — the system stabilizes
 *   faster when faults are localized. This is practically important
 *   because most real-world faults affect few machines. The adaptive
 *   bound is proportional to the fault diameter, not the whole ring.
 *
 * Time: O(1), Space: O(1)
 */
int32_t convergence_time_adaptive_bound(int32_t n, int32_t fault_distance);

/**
 * @brief Compute the fault distance — minimum number of state changes
 *        needed to reach a legitimate configuration.
 *
 * For Dijkstra's 3-state ring, this is bounded by the number of
 * machines whose state differs from the "correct" token value.
 *
 * Time: O(N), Space: O(1)
 */
int32_t convergence_fault_distance(const ring_configuration_t *config);

/* ── L8: Snap-Stabilization ──────────────────────────────────────────── */

/**
 * @brief Snap-stabilization fault classification.
 *
 * Snap-stabilization (Bui et al., 1999) guarantees that the system
 * satisfies its specification immediately after the last fault,
 * even if the fault occurs during recovery from a previous fault.
 *
 * Fault classes:
 *   - SNAP_IMMEDIATE: 0-step recovery (fault did not violate invariant)
 *   - SNAP_FAST: O(1) step recovery (fault affected ≤ 1 token position)
 *   - SNAP_STANDARD: O(N) step recovery (fault requires token propagation)
 */
typedef enum {
    SNAP_IMMEDIATE = 0,  /**< Zero-step recovery: invariant preserved */
    SNAP_FAST     = 1,   /**< Fast recovery: O(1) steps */
    SNAP_STANDARD = 2    /**< Standard recovery: O(N) steps */
} snap_fault_class_t;

/**
 * @brief Classify a fault by its expected recovery complexity.
 *
 * Analyzes the post-fault configuration and determines which
 * snap-stabilization class applies to it for Dijkstra's 3-state ring.
 *
 * Knowledge: Snap-stabilization classification — by analyzing the
 *   post-fault configuration, we can predict recovery time. If the
 *   fault only affects machines near the token, recovery is fast.
 *   If the fault creates multiple tokens or destroys the token,
 *   recovery takes longer but is still bounded.
 *
 * Time: O(N), Space: O(1)
 */
snap_fault_class_t convergence_classify_snap_fault(
    const ring_configuration_t *config);

/**
 * @brief Snap-stabilization guarantee: after a classified fault,
 *        verify that the system satisfies specification immediately.
 *
 * For SNAP_IMMEDIATE class faults, the system should already be
 * in a legitimate configuration (or one step away).
 *
 * @param config Post-fault configuration
 * @return       true if the specification already holds
 */
bool convergence_snap_guarantee_holds(const ring_configuration_t *config);

/**
 * @brief Print a convergence result summary to stdout.
 */
void convergence_result_print(const convergence_result_t *result);

/**
 * @brief Print convergence statistics to stdout.
 */
void convergence_stats_print(const convergence_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif /* CONVERGENCE_H */
