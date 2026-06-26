/**
 * @file dijkstra_ring.h
 * @brief Dijkstra's self-stabilizing token ring — core data structures
 *
 * Implements the data structures and core API for Dijkstra's seminal
 * self-stabilizing mutual exclusion algorithm on a unidirectional ring
 * (Dijkstra, 1974, CACM).
 *
 * Reference:
 *   Dijkstra, E.W. "Self-stabilizing Systems in Spite of Distributed Control."
 *   Communications of the ACM, Vol. 17, No. 11, November 1974, pp. 643-644.
 *
 * Course Mappings:
 *   MIT 6.841 — Distributed computing & fault tolerance
 *   Stanford CS254 — Self-stabilization & convergence
 *   CMU 15-855 — Distributed algorithms & stabilization
 *   ETH 263-4650 — Advanced distributed computing
 *
 * Knowledge Levels: L1 (Definitions), L2 (Core Concepts), L3 (Math Structures)
 */

#ifndef DIJKSTRA_RING_H
#define DIJKSTRA_RING_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── L1: Core Definitions ──────────────────────────────────────────────── */

/**
 * @brief Machine state type for Dijkstra's ring.
 *
 * For the 3-state algorithm: s ∈ {0, 1, 2}
 * For the 3-state bottom-machine variant: s ∈ {0, 1, 2}
 * For the 4-state algorithm: s ∈ {0, 1, 2, 3}
 * For the K-state algorithm: s ∈ {0, 1, ..., K-1}
 */
typedef int32_t machine_state_t;

/**
 * @brief Token privilege — indicates whether a machine holds the token.
 *
 * In Dijkstra's formulation, a "privilege" is the condition that allows
 * a machine to change its state. Exactly one machine should have a
 * privilege in a legitimate configuration.
 */
typedef enum {
    PRIVILEGE_NONE = 0,      /**< Machine has no privilege (no token) */
    PRIVILEGE_BOTTOM = 1,    /**< Bottom machine privilege (token at bottom) */
    PRIVILEGE_NORMAL = 2,    /**< Non-bottom machine privilege (token passing) */
    PRIVILEGE_TOP = 3        /**< Top machine privilege (4-state algorithm) */
} privilege_t;

/**
 * @brief Algorithm variant for Dijkstra's ring.
 */
typedef enum {
    ALGORITHM_3STATE = 0,    /**< 3-state bottom-machine algorithm */
    ALGORITHM_4STATE = 1,    /**< 4-state algorithm with two special machines */
    ALGORITHM_KSTATE = 2     /**< K-state uniform algorithm (K > N) */
} algorithm_variant_t;

/**
 * @brief Single machine on Dijkstra's ring.
 *
 * Each machine has a local state and knows its position in the ring.
 * Machines are arranged unidirectionally: machine i reads the state of
 * machine i-1 (with wrap-around: machine 0 reads machine N-1).
 */
typedef struct {
    machine_state_t state;       /**< Current local state */
    int32_t         machine_id;  /**< Position in ring (0 = bottom) */
    bool            is_bottom;   /**< True if this is the bottom machine */
    bool            is_top;      /**< True if this is the top machine (4-state) */
    privilege_t     privilege;   /**< Current privilege status */
} ring_machine_t;

/**
 * @brief Complete ring configuration (global state).
 *
 * Captures the states of all N machines. In distributed computing theory,
 * this is the "configuration" — the tuple of all local states.
 */
typedef struct {
    ring_machine_t *machines;    /**< Array of N machines */
    int32_t         num_machines;/**< Number of machines N */
    int32_t         num_states;  /**< Number of states K */
    algorithm_variant_t variant; /**< Algorithm variant in use */
} ring_configuration_t;

/**
 * @brief A step record — for convergence analysis and history tracking.
 *
 * Each step records which machine moved, its old state, and its new state.
 */
typedef struct {
    int32_t         step_number;  /**< Step index (0-based) */
    int32_t         machine_id;   /**< Machine that executed the step */
    machine_state_t old_state;    /**< State before transition */
    machine_state_t new_state;    /**< State after transition */
    privilege_t     privilege;    /**< Privilege that enabled the move */
} ring_step_record_t;

/* ── L2: Core Concepts — Ring Operations ───────────────────────────────── */

/**
 * @brief Initialize a ring configuration with N machines and K states.
 *
 * @param config    Pointer to uninitialized ring configuration
 * @param n         Number of machines (N ≥ 2)
 * @param k         Number of states (K depends on algorithm variant)
 * @param variant   Algorithm variant to use
 * @return          Number of machines initialized, or -1 on error
 */
int32_t ring_init(ring_configuration_t *config, int32_t n, int32_t k,
                  algorithm_variant_t variant);

/**
 * @brief Free memory associated with a ring configuration.
 */
void ring_destroy(ring_configuration_t *config);

/**
 * @brief Set the state of a specific machine.
 * @return 0 on success, -1 if machine_id out of range
 */
int32_t ring_set_state(ring_configuration_t *config, int32_t machine_id,
                       machine_state_t state);

/**
 * @brief Get the state of a specific machine.
 * @return Machine state, or -1 if machine_id out of range
 */
machine_state_t ring_get_state(const ring_configuration_t *config,
                               int32_t machine_id);

/**
 * @brief Randomize all machine states (for fault injection / initial testing).
 *
 * Each machine gets a uniformly random state in [0, K-1].
 */
void ring_randomize_states(ring_configuration_t *config, uint32_t seed);

/**
 * @brief Set all machines to the same state (a known non-legitimate config).
 */
void ring_set_uniform_state(ring_configuration_t *config, machine_state_t s);

/* ── L2: Token / Privilege Detection ───────────────────────────────────── */

/**
 * @brief Compute the privilege for a single machine under current ring state.
 *
 * For the 3-state algorithm:
 *   - Bottom machine (i=0): privilege iff S[0] == S[N-1]
 *   - Other machines (i>0): privilege iff S[i] != S[i-1]
 *
 * @param config    Ring configuration
 * @param machine_id Machine to check
 * @return          Privilege type (PRIVILEGE_NONE if no privilege)
 */
privilege_t ring_compute_privilege(const ring_configuration_t *config,
                                   int32_t machine_id);

/**
 * @brief Count the number of privileged machines in the current configuration.
 *
 * In a legitimate state, exactly 1 machine should be privileged.
 * This function is essential for convergence detection.
 *
 * @param config Ring configuration
 * @return       Number of privileged machines
 */
int32_t ring_count_privileges(const ring_configuration_t *config);

/**
 * @brief Check if the current configuration is legitimate.
 *
 * A configuration is legitimate iff exactly one machine has a privilege.
 * This is the "closure" property — once legitimate, always legitimate
 * (assuming legal moves only).
 *
 * @param config Ring configuration
 * @return       true if exactly one privilege exists
 */
bool ring_is_legitimate(const ring_configuration_t *config);

/* ── L3: Mathematical Structures — Transition Functions ─────────────────── */

/**
 * @brief Execute one step for a specific machine (3-state algorithm).
 *
 * Bottom machine (i=0):
 *   if S[0] == S[N-1] then S[0] := (S[N-1] + 1) mod K
 *
 * Non-bottom machine (i>0):
 *   if S[i] != S[i-1] then S[i] := S[i-1]
 *
 * This function performs the state transition atomically.
 *
 * @param config    Ring configuration (modified in-place)
 * @param machine_id Machine to execute
 * @return          0 if machine moved, 1 if no move allowed, -1 on error
 */
int32_t ring_step_3state(ring_configuration_t *config, int32_t machine_id);

/**
 * @brief Execute one step under the 4-state algorithm.
 *
 * Machine 0 (bottom): if S[0]+1 mod 4 == S[1] then S[0] := S[1]
 * Machine N-1 (top):  if S[N-2]+1 mod 4 == S[N-1] and S[0]+1 mod 4 != S[N-1]
 *                      then S[N-1] := (S[N-1]+1) mod 4
 * Machine i (middle):  if S[i-1]+1 mod 4 == S[i] then S[i] := (S[i]+1) mod 4
 *   else if S[i-1]+1 mod 4 != S[i] then S[i] := S[i-1]+1 mod 4... actually:
 *
 * Simplified 4-state rule:
 *   Top machine: if left+1 mod 4 = right and bottom+1 mod 4 ≠ self,
 *                then self := (self+1) mod 4
 *   Bottom machine: if left+1 mod 4 = self then self := left
 *   Other machines: if left+1 mod 4 ≠ self then self := (left+1) mod 4
 *
 * @return 0 if machine moved, 1 if no move allowed, -1 on error
 */
int32_t ring_step_4state(ring_configuration_t *config, int32_t machine_id);

/**
 * @brief Execute one step under the K-state algorithm (K > N).
 *
 * Machine 0: if S[0] == S[N-1] then S[0] := (S[0]+1) mod K
 * Machine i:  if S[i] != S[i-1] then S[i] := S[i-1]
 *
 * This is a generalization of the 3-state algorithm.
 *
 * @return 0 if machine moved, 1 if no move allowed, -1 on error
 */
int32_t ring_step_kstate(ring_configuration_t *config, int32_t machine_id);

/**
 * @brief Execute one step using the appropriate algorithm for this ring.
 *
 * Dispatch function that selects the right step implementation based
 * on the algorithm variant stored in the configuration.
 *
 * @return 0 if machine moved, 1 if no move allowed, -1 on error
 */
int32_t ring_step(ring_configuration_t *config, int32_t machine_id);

/* ── L2: Convergence Tracking ──────────────────────────────────────────── */

/**
 * @brief Execute a full round of steps (one per machine, in order).
 *
 * This simulates a "synchronous round" where every machine is evaluated
 * once. Returns the number of machines that actually moved.
 */
int32_t ring_synchronous_round(ring_configuration_t *config);

/**
 * @brief Run the ring until convergence to a legitimate state.
 *
 * Uses a central daemon (arbitrary but fair scheduling).
 * Returns the number of steps taken, or -1 if it exceeds max_steps.
 *
 * @param config     Ring configuration
 * @param max_steps  Maximum steps before giving up
 * @param history    Optional output array for step records (can be NULL)
 * @param hist_cap   Capacity of history array (ignored if history is NULL)
 * @return           Number of steps to converge, or -1 if timed out
 */
int32_t ring_converge_to_legitimate(ring_configuration_t *config,
                                     int32_t max_steps,
                                     ring_step_record_t *history,
                                     int32_t hist_cap);

/**
 * @brief Clone a ring configuration (deep copy).
 * @return Pointer to new configuration (caller must ring_destroy), or NULL
 */
ring_configuration_t *ring_clone(const ring_configuration_t *src);

/**
 * @brief Compare two ring configurations for equality.
 * @return true if all machine states match
 */
bool ring_equals(const ring_configuration_t *a, const ring_configuration_t *b);

/**
 * @brief Print the current ring configuration to stdout (debug/demo).
 */
void ring_print(const ring_configuration_t *config);

/* ── L4: Theorem Verification — Closure Property ───────────────────────── */

/**
 * @brief Verify the closure property: if the ring is in a legitimate state,
 *        executing any legal step leaves it in a legitimate state.
 *
 * This is an executable verification of Dijkstra's closure theorem.
 * It exhaustively checks all possible legal moves from the current state.
 *
 * @param config  Ring configuration (must be legitimate)
 * @return        true if closure holds for all possible next states
 */
bool ring_verify_closure(const ring_configuration_t *config);

/**
 * @brief Verify that every non-legitimate configuration eventually converges.
 *
 * For small N, this performs exhaustive search from all non-legitimate
 * configurations reachable from the given starting point.
 *
 * @param config     Starting configuration
 * @param max_depth  Maximum search depth
 * @return          Number of configurations verified to converge, -1 on fail
 */
int32_t ring_verify_convergence(const ring_configuration_t *config,
                                 int32_t max_depth);

#ifdef __cplusplus
}
#endif

#endif /* DIJKSTRA_RING_H */
