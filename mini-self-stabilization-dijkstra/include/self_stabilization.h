/**
 * @file self_stabilization.h
 * @brief Core definitions and theorems of self-stabilization theory
 *
 * Provides the foundational definitions of self-stabilization as
 * introduced by Dijkstra (1974) and formalized by subsequent work.
 * This file defines the central concepts: legitimate states, closure,
 * convergence, and the self-stabilization property itself.
 *
 * Reference:
 *   Dijkstra, E.W. "Self-stabilizing Systems in Spite of Distributed Control."
 *     Communications of the ACM, Vol. 17, No. 11, 1974.
 *   Schneider, M. "Self-Stabilization." ACM Computing Surveys, Vol. 25, No. 1,
 *     March 1993, pp. 45-67.
 *   Dolev, S. "Self-Stabilization." MIT Press, 2000.
 *
 * Course Mappings:
 *   MIT 6.841 — Self-stabilization & fault tolerance
 *   Stanford CS254 — Distributed computing theory
 *   Berkeley CS278 — Stabilization & convergence
 *   CMU 15-855 — Advanced distributed algorithms
 *   Princeton COS 551 — Self-stabilizing systems
 *   Caltech CS 154 — Limits of distributed computation
 *   Cambridge Part III — Advanced distributed algorithms
 *   Oxford — Distributed computing & stabilization
 *   ETH 263-4650 — Self-stabilizing algorithms
 *
 * Knowledge Levels: L1-L4 (Definitions through Fundamental Laws)
 */

#ifndef SELF_STABILIZATION_H
#define SELF_STABILIZATION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── L1: Formal Definitions ────────────────────────────────────────────── */

/**
 * @brief Self-stabilization definition (Dijkstra, 1974).
 *
 * A distributed system S is self-stabilizing with respect to a
 * predicate LEGITIMATE (a subset of the global state space) iff:
 *
 *   1. CLOSURE:  LEGITIMATE is closed under all transitions of S.
 *      (∀c ∈ LEGITIMATE: c → c' ⇒ c' ∈ LEGITIMATE)
 *
 *   2. CONVERGENCE: Starting from any configuration, every maximal
 *      execution of S eventually reaches a configuration in LEGITIMATE.
 *      (∀ executions e: ∃ finite prefix p of e: last(p) ∈ LEGITIMATE)
 *
 * This struct represents a self-stabilizing system descriptor.
 */
typedef struct {
    const char *system_name;       /**< Human-readable system identifier */
    bool      (*legitimate_fn)(const void *config);  /**< LEGITIMATE predicate */
    int32_t   (*step_fn)(void *config, int32_t machine_id); /**< Transition fn */
    int32_t     num_machines;      /**< Number of machines N */
    int32_t     num_states;        /**< Number of states K per machine */
    void       *context;           /**< Opaque context for callbacks */
} ss_system_descriptor_t;

/* ── L2: Core Concepts ─────────────────────────────────────────────────── */

/**
 * @brief Check if a system satisfies the closure property.
 *
 * This is an executable check: for a given legitimate configuration,
 * verifies that all possible next configurations are also legitimate.
 *
 * @param desc   System descriptor
 * @param config A configuration known to be legitimate
 * @return       true if closure holds from this config
 */
bool ss_check_closure(const ss_system_descriptor_t *desc,
                      const void *config);

/**
 * @brief Check if a system converges from a given configuration.
 *
 * Simulates execution from the given configuration to verify that
 * a legitimate state is reached within max_steps.
 *
 * @param desc       System descriptor
 * @param config     Starting configuration
 * @param max_steps  Maximum steps to simulate
 * @return           Number of steps to converge, or -1 if timeout
 */
int32_t ss_check_convergence(ss_system_descriptor_t *desc,
                             void *config,
                             int32_t max_steps);

/* ── L3: Mathematical Structures ───────────────────────────────────────── */

/**
 * @brief Legitimate configuration set — formal characterization.
 *
 * For Dijkstra's token ring, the legitimate configurations are those
 * where exactly one machine has a privilege (token). This set has
 * cardinality N * K — for each possible token position and each
 * possible token state value.
 *
 * This structure enumerates and indexes all legitimate configurations
 * of a given self-stabilizing system.
 */
typedef struct {
    int32_t *config_ids;       /**< Indices of legitimate configurations */
    int32_t  num_legitimate;   /**< Number of legitimate configurations */
    int32_t  total_configs;    /**< Total number of configurations */
    double   density;          /**< Density = num_legitimate / total_configs */
} legitimate_set_t;

/**
 * @brief Enumerate all legitimate configurations of a ring system.
 *
 * For Dijkstra's ring with N machines and K states:
 *   Total configurations: K^N
 *   Legitimate configurations: all configs with exactly 1 privilege
 *   The count depends on N and K combinatorially (not simply N*K)
 *
 * @param n          Number of machines
 * @param k          Number of states
 * @param variant    Algorithm variant
 * @param lset       Output legitimate set (caller-freed)
 * @return           0 on success
 */
int32_t ss_enumerate_legitimate_configs(int32_t n, int32_t k,
                                         int32_t variant,
                                         legitimate_set_t *lset);

/* ── L4: Fundamental Laws / Theorems ──────────────────────────────────── */

/**
 * @brief Dijkstra's Theorem 1 (3-state ring convergence).
 *
 * For a ring of N machines with the 3-state bottom-machine algorithm:
 *   - The system is self-stabilizing (satisfies both closure and convergence)
 *   - The worst-case convergence is O(N^2) steps
 *   - K = 3 states are sufficient, independent of N
 *
 * This function verifies the theorem exhaustively for small N.
 *
 * @param n  Number of machines (1 ≤ n ≤ 6 for exhaustive check)
 * @return    true if the theorem holds for this N
 */
bool ss_theorem_dijkstra_3state(int32_t n);

/**
 * @brief Dijkstra's Theorem 2 (K-state ring convergence).
 *
 * For a ring of N identical machines with K > N states:
 *   - The system is self-stabilizing
 *   - The worst-case convergence is O(N^2) steps
 *
 * Note: K must exceed N for the uniform algorithm to be self-stabilizing.
 * When K ≤ N, there exist configurations that never converge.
 *
 * @param n  Number of machines
 * @param k  Number of states (must be > n)
 * @return    true if the theorem holds for these parameters
 */
bool ss_theorem_dijkstra_kstate(int32_t n, int32_t k);

/**
 * @brief Lower bound on the number of states.
 *
 * Theorem (Dijkstra, 1974): No uniform self-stabilizing ring
 * (where all machines execute identical programs) can exist with
 * fewer than N+1 states. A distinguished (bottom) machine is
 * required to break the symmetry with K=3 states.
 *
 * This function returns the minimum K needed for a given N
 * with uniform machines.
 *
 * @param n Number of machines
 * @return  Minimum number of states required (N+1 for uniform case)
 */
int32_t ss_theorem_min_states_uniform(int32_t n);

/**
 * @brief The "No Symmetry" lemma.
 *
 * Lemma: In a ring of identical, deterministic state machines with
 * a uniform transition function, if all machines start in the same
 * state, they will all remain in the same state forever — convergence
 * to asymmetry (a single token) is impossible.
 *
 * This function checks the symmetry-breaking property of a system.
 *
 * @param n Number of machines
 * @param k Number of states
 * @return  true if the system can break symmetry from a uniform start
 */
bool ss_lemma_symmetry_breaking(int32_t n, int32_t k);

/* ── L5: Self-Stabilization Composition ────────────────────────────────── */

/**
 * @brief Fair composition of self-stabilizing systems.
 *
 * Theorem (Dolev, 2000): If systems A and B are both self-stabilizing
 * and they do not interfere (disjoint variables), then their fair
 * composition is also self-stabilizing.
 *
 * This structure represents a composed system.
 */
typedef struct {
    ss_system_descriptor_t *subsystem_a;
    ss_system_descriptor_t *subsystem_b;
    bool (*interference_check)(const void *config); /**< True if no interference */
} ss_composed_system_t;

/**
 * @brief Verify that a composed system is self-stabilizing.
 *
 * @param composed The composed system descriptor
 * @return         true if the composition preserves self-stabilization
 */
bool ss_verify_composition(const ss_composed_system_t *composed);

/* ── L8: Advanced — Probabilistic Self-Stabilization ───────────────────── */

/**
 * @brief Probabilistic self-stabilization descriptor.
 *
 * A system is probabilistically self-stabilizing if:
 *   1. Closure holds with probability 1
 *   2. From any configuration, the probability of reaching a
 *      legitimate configuration approaches 1 as t → ∞
 *
 * This relaxes the deterministic convergence requirement.
 */
typedef struct {
    ss_system_descriptor_t base;
    double convergence_probability; /**< Per-step prob of moving toward legit */
    int32_t expected_convergence_steps; /**< Expected steps to converge */
} ss_probabilistic_system_t;

/**
 * @brief Compute the expected convergence time for a probabilistic
 *        self-stabilizing token circulation algorithm.
 *
 * Uses the absorbing Markov chain model (Herman, 1990).
 *
 * @param n  Number of machines
 * @param k  Number of states
 * @return    Expected number of steps
 */
double ss_probabilistic_expected_steps(int32_t n, int32_t k);

#ifdef __cplusplus
}
#endif

#endif /* SELF_STABILIZATION_H */
