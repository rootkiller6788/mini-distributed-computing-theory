/**
 * @file msg_automaton.h
 * @brief Message automaton formal model for distributed agreement protocols.
 *
 * Covers L3 (Mathematical Structures): The message automaton is the
 * formal computational model for Byzantine agreement — it captures
 * the state transition rules of each process as a function of received
 * messages in each synchronous round.
 *
 * The automaton tuple: (Q, q0, Σ_in, Σ_out, δ, λ) where:
 *   Q    = local process states
 *   q0   = initial state (with initial value)
 *   Σ_in = multiset of received messages
 *   Σ_out= multiset of sent messages
 *   δ    = transition function: Q × Σ_in → Q
 *   λ    = output function: Q → Σ_out ∪ {⊥}
 *
 * Reference: Lynch (1996) "Distributed Algorithms" Ch. 5-6
 *            Lamport (1982) "The Byzantine Generals Problem"
 */

#ifndef MSG_AUTOMATON_H
#define MSG_AUTOMATON_H

#include <stdint.h>
#include <stdbool.h>
#include "byzantine_agreement.h"

/* ================================================================
 * L3: Message Automaton Formal Structure
 * ================================================================ */

/**
 * @brief The formal state space of a message automaton.
 *
 * The state includes: the process's current round, its estimate of the
 * decision value, a tree of received values (for EIG-style algorithms),
 * and whether a decision has been made.
 */
typedef struct {
    int32_t pid;                                   /**< Automaton ID */
    int32_t round;                                 /**< Current round ∈ [0, t+1] */
    int32_t estimate;                              /**< Current value estimate */
    bool    decided;                               /**< Has the automaton decided? */
    int32_t decision;                              /**< Decided value, valid iff decided */
    /* EIG-style value tree: tree[level][path_code] */
    int32_t **value_tree;                          /**< Recursively gathered values */
    int32_t   tree_depth;                          /**< Depth of the value tree (t+1) */
    int32_t   tree_width;                          /**< Branching factor = N */
    /* Message log */
    ba_message_t *sent;                            /**< Messages sent this round */
    int32_t       sent_count;
    ba_message_t *received;                        /**< Messages received this round */
    int32_t       received_count;
} automaton_state_t;

/**
 * @brief The transition function δ: Q × Σ_in → Q.
 *
 * Given the current state and a multiset of incoming messages,
 * computes the next state of the automaton.
 *
 * The transition encodes the protocol logic: how a process updates
 * its estimate based on what it heard from others.
 *
 * For the oral message model, δ implements recursive majority voting.
 */
typedef struct {
    /** The transition rule encoded as a function pointer */
    int32_t (*apply)(const automaton_state_t *q,
                     const ba_message_t *incoming, int32_t num_incoming,
                     automaton_state_t *q_next);
    /** Name of the transition rule (e.g., "EIG_oral", "PhaseKing") */
    const char *rule_name;
    /** The protocol family */
    enum { TRANSITION_ORAL, TRANSITION_SIGNED, TRANSITION_HYBRID } family;
} transition_function_t;

/**
 * @brief The output function λ: Q → Σ_out ∪ {⊥}.
 *
 * Given the current state, produces the set of messages to send
 * in the next round (or ⊥ if no messages are sent).
 */
typedef struct {
    int32_t (*generate)(const automaton_state_t *q,
                        ba_message_t *outgoing, int32_t max_outgoing);
    const char *output_name;
} output_function_t;

/**
 * @brief Full message automaton = (Q, q0, Σ_in, Σ_out, δ, λ).
 */
typedef struct {
    automaton_state_t       state;          /**< Current state q ∈ Q */
    automaton_state_t       initial_state;  /**< Initial state q0 */
    transition_function_t   delta;          /**< Transition function δ */
    output_function_t       lambda;         /**< Output function λ */
    ba_message_t           *input_alphabet; /**< Representative messages */
    int32_t                 input_alphabet_size;
    int32_t                 N;              /**< Number of automata in the system */
    int32_t                 f;              /**< Tolerance bound */
} msg_automaton_t;

/* ================================================================
 * L3: Automaton Algebra
 *
 * The message automaton model allows us to formalize crucial concepts:
 *
 * 1. Configuration: The vector of all automaton states.
 * 2. Event: A step from one configuration to the next.
 * 3. Execution: A sequence of configurations C0 → C1 → ... → Ck.
 * 4. Admissible execution: Every correct automaton takes infinitely
 *    many steps (synchronous case: one step per round).
 * 5. Bivalent configuration: A configuration from which both 0 and 1
 *    are still reachable. (Key concept in FLP impossibility.)
 */

/**
 * @brief A global configuration = vector of N automaton states.
 */
typedef struct {
    automaton_state_t states[BA_MAX_PROCESSES];
    int32_t           N;
    int32_t           config_id;
} automaton_config_t;

/**
 * @brief An event = transition of one automaton in one round.
 */
typedef struct {
    int32_t from_config_id;
    int32_t to_config_id;
    int32_t pid;                    /**< Automaton that moved */
    int32_t round;
} automaton_event_t;

/**
 * @brief An execution = sequence of configurations.
 */
typedef struct {
    automaton_config_t *configs;
    int32_t              length;
    int32_t              capacity;
} automaton_execution_t;

/**
 * @brief Bivalence classification of a configuration.
 *
 * From FLP (1985): A configuration C is k-valent (k ∈ {0, 1}) if
 * all admissible executions from C lead to decision k.
 * C is bivalent if both 0 and 1 are reachable from C.
 */
typedef enum {
    AUTOMATON_0_VALENT = 0,   /**< Only 0 is reachable */
    AUTOMATON_1_VALENT = 1,   /**< Only 1 is reachable */
    AUTOMATON_BIVALENT = 2    /**< Both 0 and 1 reachable */
} automaton_valence_t;

/* ================================================================
 * L3: API
 * ================================================================ */

/**
 * @brief Initialize a message automaton with a given protocol rule.
 * @param ma        Automaton to initialize.
 * @param N         Number of processes.
 * @param f         Fault tolerance.
 * @param pid       Process ID for this automaton.
 * @param init_val  Initial proposed value.
 * @param delta     Transition function.
 * @param lambda    Output function.
 * @return 0 on success, -1 on error.
 *
 * Complexity: O(N^t) for EIG tree allocation, where t = f+1
 * Reference: Lynch (1996) §6.2.2
 */
int  automaton_init(msg_automaton_t *ma, int32_t N, int32_t f,
                    int32_t pid, int32_t init_val,
                    transition_function_t delta, output_function_t lambda);

/**
 * @brief Apply one round of the automaton: receive messages, compute
 *        next state, generate outgoing messages.
 * @param ma        Automaton to step.
 * @param incoming  Messages received this round.
 * @param num_in    Number of incoming messages.
 * @param outgoing  Output buffer for generated messages.
 * @param max_out   Capacity of outgoing buffer.
 * @param num_out   Actual number of generated messages (output).
 * @return 0 on success, -1 on error.
 *
 * Complexity: O(num_in * N^t) for oral model tree updates
 */
int  automaton_step(msg_automaton_t *ma,
                    const ba_message_t *incoming, int32_t num_in,
                    ba_message_t *outgoing, int32_t max_out,
                    int32_t *num_out);

/**
 * @brief Determine the valence of a configuration by exploring
 *        all possible next steps (one for each process + message schedule).
 * @param cfg       Configuration to analyze.
 * @param N         Number of processes.
 * @param fault_mask Which processes are faulty (bitmask).
 * @return The valence classification.
 *
 * Complexity: O(N * |Σ_in|^(N)) — exhaustive search for small N
 * Theorem: FLP85 Lemma 2 (existence of bivalent initial configuration)
 */
automaton_valence_t automaton_classify_valence(const automaton_config_t *cfg,
                                                int32_t N, uint16_t fault_mask);

/**
 * @brief Execute the full protocol from a given initial configuration
 *        for a specified number of rounds, recording the execution trace.
 * @param exec       Output execution trace.
 * @param init_cfg   Initial configuration.
 * @param N          Number of processes.
 * @param f          Faulty count.
 * @param rounds     Number of rounds to execute.
 * @param fault_mask Faulty process bitmask.
 * @return 0 on success, -1 on error.
 */
int  automaton_execute(automaton_execution_t *exec,
                       const automaton_config_t *init_cfg,
                       int32_t N, int32_t f, int32_t rounds,
                       uint16_t fault_mask);

/**
 * @brief Free resources associated with an automaton.
 * @param ma Automaton to destroy.
 */
void automaton_destroy(msg_automaton_t *ma);

/**
 * @brief Free resources associated with an execution trace.
 * @param exec Execution to destroy.
 */
void automaton_execution_destroy(automaton_execution_t *exec);

/**
 * @brief Check whether a configuration satisfies the agreement property.
 * @param cfg Configuration to check.
 * @param N   Number of processes.
 * @param fault_mask Faulty process bitmask.
 * @return true if all correct processes have the same decision.
 */
bool automaton_config_has_agreement(const automaton_config_t *cfg,
                                     int32_t N, uint16_t fault_mask);

/**
 * @brief Check whether a configuration is terminal (all correct decided).
 * @param cfg Configuration to check.
 * @param N   Number of processes.
 * @param fault_mask Faulty process bitmask.
 * @return true if all correct processes have decided.
 */
bool automaton_config_is_terminal(const automaton_config_t *cfg,
                                   int32_t N, uint16_t fault_mask);

#endif /* MSG_AUTOMATON_H */
