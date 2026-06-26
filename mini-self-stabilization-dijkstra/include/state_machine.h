/**
 * @file state_machine.h
 * @brief Finite state machine abstraction for self-stabilizing systems
 *
 * Provides a general framework for modeling distributed state machines
 * with self-stabilization properties. This abstraction is used to
 * express Dijkstra's ring and other self-stabilizing algorithms uniformly.
 *
 * Reference:
 *   Dolev, S. "Self-Stabilization." MIT Press, 2000.
 *   Schneider, M. "Self-Stabilization." ACM Computing Surveys, 1993.
 *
 * Course Mappings:
 *   MIT 6.841 — State machine replication
 *   CMU 15-455 — Finite automata & distributed systems
 *   Oxford — Advanced complexity & stabilization
 *
 * Knowledge Levels: L1 (Definitions), L2 (Core Concepts), L3 (Math Structures)
 */

#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── L1: State Machine Definitions ─────────────────────────────────────── */

/**
 * @brief Guard condition — a predicate that enables a transition.
 *
 * A guard is a boolean function of the local state and a read-only
 * view of neighbor states. Only when the guard evaluates to true
 * may the machine execute its associated command.
 */
typedef bool (*guard_fn_t)(const int32_t *local_state,
                           const int32_t *neighbor_states,
                           int32_t num_neighbors,
                           void *context);

/**
 * @brief Command / action — the state transition to execute.
 *
 * When a guard is true, the command computes the new local state.
 * The transition is atomic: guard evaluation and state update are
 * indivisible at the level of a single machine.
 */
typedef int32_t (*command_fn_t)(const int32_t *local_state,
                                const int32_t *neighbor_states,
                                int32_t num_neighbors,
                                void *context);

/**
 * @brief A guarded command (rule) in Dijkstra's guarded command language.
 *
 * Each rule has the form:  guard → command
 *   IF guard(local, neighbors) THEN local := command(local, neighbors)
 *
 * This formalizes Dijkstra's "guarded commands" used throughout his
 * self-stabilization paper and later work on predicate transformers.
 */
typedef struct {
    guard_fn_t   guard;       /**< Guard predicate */
    command_fn_t command;     /**< State transition function */
    const char  *rule_name;   /**< Human-readable rule identifier */
} guarded_command_t;

/**
 * @brief Self-stabilizing state machine definition.
 *
 * Encapsulates a complete self-stabilizing machine:
 *   - A set of states S (local state space)
 *   - A set of guarded commands R (transition rules)
 *   - A topology descriptor (neighbors)
 *
 * The machine is self-stabilizing if from any initial state, execution
 * of the rules leads to a legitimate configuration in finite time,
 * and once legitimate, the configuration stays legitimate.
 */
typedef struct {
    int32_t          local_state;       /**< Current local state */
    int32_t          num_states;        /**< Size of state space |S| */
    int32_t          machine_id;        /**< Unique machine identifier */
    guarded_command_t *rules;           /**< Array of guarded commands */
    int32_t          num_rules;         /**< Number of rules |R| */
    int32_t          *neighbor_ids;     /**< IDs of neighbor machines */
    int32_t          num_neighbors;     /**< Degree of this machine */
    void             *context;          /**< Opaque context for callbacks */
} ss_state_machine_t;

/* ── L2: Core Concepts — Machine Operations ─────────────────────────────── */

/**
 * @brief Initialize a self-stabilizing state machine.
 *
 * @param machine       Pointer to uninitialized machine
 * @param machine_id    Unique ID for this machine
 * @param num_states    Number of states in the local state space
 * @param num_rules     Number of guarded commands
 * @param num_neighbors Maximum number of neighbors
 * @return              0 on success, -1 on error
 */
int32_t ss_machine_init(ss_state_machine_t *machine,
                        int32_t machine_id,
                        int32_t num_states,
                        int32_t num_rules,
                        int32_t num_neighbors);

/**
 * @brief Deallocate resources associated with a state machine.
 */
void ss_machine_destroy(ss_state_machine_t *machine);

/**
 * @brief Set a guarded command rule by index.
 *
 * @param machine   Target machine
 * @param rule_idx  Index of the rule to set
 * @param guard     Guard function
 * @param command   Command function
 * @param name      Rule name (for debugging)
 * @return          0 on success, -1 on bounds error
 */
int32_t ss_machine_set_rule(ss_state_machine_t *machine,
                            int32_t rule_idx,
                            guard_fn_t guard,
                            command_fn_t command,
                            const char *name);

/**
 * @brief Set a neighbor ID.
 * @return 0 on success, -1 on bounds error
 */
int32_t ss_machine_set_neighbor(ss_state_machine_t *machine,
                                int32_t neighbor_idx,
                                int32_t neighbor_id);

/**
 * @brief Evaluate all guards and return the index of the first enabled rule.
 * @return Rule index if a guard is true, -1 if no guard is enabled
 */
int32_t ss_machine_evaluate_guards(const ss_state_machine_t *machine,
                                   const int32_t *neighbor_states);

/**
 * @brief Execute the command associated with an enabled rule.
 *
 * The rule_idx should come from ss_machine_evaluate_guards.
 *
 * @param machine       Machine to update (modified in-place)
 * @param rule_idx      Index of the enabled rule
 * @param neighbor_states Array of neighbor states
 * @return              New local state, or -1 on error
 */
int32_t ss_machine_execute_command(ss_state_machine_t *machine,
                                   int32_t rule_idx,
                                   const int32_t *neighbor_states);

/* ── L3: Mathematical Structures — Transition System ───────────────────── */

/**
 * @brief Global configuration of a distributed state machine system.
 *
 * A configuration is the tuple (s_0, s_1, ..., s_{N-1}) of all
 * local states. The set of all configurations C = S^N is the global
 * state space.
 */
typedef struct {
    ss_state_machine_t *machines;  /**< Array of N machines */
    int32_t             num_machines; /**< System size N */
    int32_t            *state_buffer; /**< Snapshot of all local states */
} ss_global_config_t;

/**
 * @brief Initialize a global configuration from an array of machines.
 *
 * @param config    Uninitialized global config
 * @param machines  Array of initialized machines
 * @param n         Number of machines
 * @return          0 on success
 */
int32_t ss_global_config_init(ss_global_config_t *config,
                              ss_state_machine_t *machines,
                              int32_t n);

/**
 * @brief Refresh the state buffer from the machines array.
 *
 * Call this after any machine state change to keep the snapshot
 * consistent for neighbor reads.
 */
void ss_global_config_sync(ss_global_config_t *config);

/**
 * @brief Destroy global configuration resources.
 */
void ss_global_config_destroy(ss_global_config_t *config);

/* ── L3: Transition Graph Representation ────────────────────────────────── */

/**
 * @brief A transition in the global state space.
 *
 * Represents one atomic step: (config_old) → (config_new) by machine i
 * executing rule r.
 */
typedef struct {
    int32_t from_config_id;   /**< Source configuration identifier */
    int32_t to_config_id;     /**< Destination configuration identifier */
    int32_t machine_id;       /**< Machine that executed */
    int32_t rule_idx;         /**< Rule that was executed */
} ss_transition_t;

/**
 * @brief Transition system / state space graph.
 *
 * The transition system T = (C, →) where:
 *   - C is the set of all global configurations
 *   - → ⊆ C × C is the transition relation
 *
 * Self-stabilization requires:
 *   1. (Convergence) From any c ∈ C, every maximal execution reaches L
 *   2. (Closure)    From any l ∈ L, every transition stays in L
 *
 * where L ⊆ C is the set of legitimate configurations.
 */
typedef struct {
    int32_t          *states;         /**< Flat array of N*|C| states */
    int32_t           num_configs;    /**< |C| = K^N (total configurations) */
    int32_t           num_machines;   /**< N */
    int32_t           num_machine_states; /**< K (states per machine) */
    ss_transition_t  *transitions;    /**< Dynamic array of transitions */
    int32_t           num_transitions;/**< Number of transitions recorded */
    int32_t           trans_capacity; /**< Capacity of transitions array */
} ss_transition_system_t;

/**
 * @brief Build the complete transition system for a ring of N machines with
 *        K states using Dijkstra's rules.
 *
 * This enumerates all K^N configurations and all possible transitions.
 *
 * @param ts       Uninitialized transition system
 * @param n        Number of machines
 * @param k        States per machine
 * @param variant  Algorithm variant
 * @return         0 on success, -1 on failure (too large or memory error)
 */
int32_t ss_build_transition_system(ss_transition_system_t *ts,
                                   int32_t n, int32_t k,
                                   int32_t variant);

/**
 * @brief Destroy a transition system and free all memory.
 */
void ss_transition_system_destroy(ss_transition_system_t *ts);

/**
 * @brief Check if a configuration (by index) is legitimate.
 *
 * For Dijkstra's ring: exactly one privilege exists.
 *
 * @param ts         Transition system
 * @param config_id  Configuration index
 * @return           true if legitimate
 */
bool ss_config_is_legitimate(const ss_transition_system_t *ts,
                             int32_t config_id);

/**
 * @brief Verify the closure property exhaustively on a transition system.
 *
 * Checks: for every legitimate config, all outgoing transitions go to
 * legitimate configs.
 *
 * @param ts Transition system
 * @return  true if closure holds
 */
bool ss_verify_closure_exhaustive(const ss_transition_system_t *ts);

/**
 * @brief Verify the convergence property on a transition system.
 *
 * Uses BFS/DFS from all configurations to check that every path
 * eventually reaches a legitimate configuration (no cycles in
 * non-legitimate region).
 *
 * @param ts Transition system
 * @return  Number of non-legitimate configs that converge, -1 if any diverge
 */
int32_t ss_verify_convergence_exhaustive(const ss_transition_system_t *ts);

/**
 * @brief Compute the worst-case convergence distance from any non-legitimate
 *        configuration to a legitimate one.
 *
 * @param ts Transition system
 * @return  Maximum number of steps needed to converge
 */
int32_t ss_max_convergence_steps(const ss_transition_system_t *ts);

#ifdef __cplusplus
}
#endif

#endif /* STATE_MACHINE_H */
