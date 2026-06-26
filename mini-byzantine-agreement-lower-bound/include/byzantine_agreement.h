/**
 * @file byzantine_agreement.h
 * @brief Byzantine Agreement core definitions and lower bound results.
 *
 * Covers L1 (Definitions): Byzantine failure, agreement/validity/termination,
 * f-resilient, synchronous round model, oral messages vs signed messages.
 *
 * Reference: Lamport, Shostak, Pease (1982) "The Byzantine Generals Problem"
 *            Fischer, Lynch, Paterson (1985) "Impossibility of Distributed
 *            Consensus with One Faulty Process"
 *
 * Course mapping: MIT 6.852, Princeton COS 551, ETH 263-4650
 */

#ifndef BYZANTINE_AGREEMENT_H
#define BYZANTINE_AGREEMENT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ================================================================
 * L1: Core Definitions
 * ================================================================ */

/** Maximum reasonable number of processes for enumeration-based proofs */
#define BA_MAX_PROCESSES    16
/** Maximum value domain size */
#define BA_MAX_VALUES       16

/**
 * @brief Message types in the Byzantine agreement model.
 *
 * ORAL: Messages cannot be forged, but a faulty sender can lie about what
 *        it received from others. This is the standard unsigned model.
 * SIGNED: Messages carry unforgeable signatures. A faulty process cannot
 *         forge a message claiming to come from a correct process.
 */
typedef enum {
    BA_MSG_ORAL = 0,       /**< Oral (unsigned) message model */
    BA_MSG_SIGNED = 1,     /**< Signed (authenticated) message model */
    BA_MSG_HYBRID = 2      /**< Hybrid (some channels authenticated) */
} ba_message_model_t;

/**
 * @brief System synchrony model for Byzantine agreement.
 */
typedef enum {
    BA_SYNC = 0,           /**< Synchronous: known message delay bound Δ */
    BA_ASYNC = 1,          /**< Asynchronous: no timing guarantees */
    BA_PARTIAL_SYNC = 2    /**< Partial synchrony: eventually bounded delay */
} ba_sync_model_t;

/**
 * @brief Process fault type.
 */
typedef enum {
    BA_CORRECT = 0,        /**< Follows protocol correctly */
    BA_CRASH_FAULT = 1,    /**< Stops prematurely (non-Byzantine) */
    BA_BYZANTINE = 2       /**< Arbitrary (Byzantine) behavior */
} ba_fault_type_t;

/**
 * @brief A single message in the Byzantine agreement protocol.
 *
 * The message carries the value being agreed upon and a path tracking
 * which processes have relayed this message. The path is essential for
 * the oral message model: "A told me that B told him that C said v."
 */
typedef struct {
    int32_t  value;                          /**< Proposed/relayed value */
    int32_t  sender;                         /**< Direct sender of this message */
    int32_t  path[BA_MAX_PROCESSES];        /**< Relay chain (excluding sender) */
    int32_t  path_len;                       /**< Number of hops in path */
    int32_t  round;                          /**< Round number when sent */
    uint64_t signature;                      /**< For signed model (simplified) */
} ba_message_t;

/**
 * @brief Local state of a single process in the Byzantine agreement protocol.
 *
 * Each process maintains its own view of the system. In the oral message
 * model, a process recursively collects values through relay chains.
 */
typedef struct {
    int32_t           pid;                   /**< Process identifier (0..N-1) */
    int32_t           initial_value;         /**< Its own initial proposed value */
    int32_t           decided_value;         /**< Final decided value, or -1 if undecided */
    bool              has_decided;           /**< Whether decision has been made */
    ba_fault_type_t   fault_type;            /**< Whether this process is faulty */
    /* Received messages indexed by round and relay path */
    ba_message_t      inbox[BA_MAX_PROCESSES * BA_MAX_PROCESSES];
    int32_t           inbox_count;
    int32_t           round_number;          /**< Current round */
} ba_process_t;

/**
 * @brief Full system configuration for a Byzantine agreement instance.
 */
typedef struct {
    int32_t             num_processes;       /**< N, total number of processes */
    int32_t             max_faulty;          /**< f, maximum number of faulty processes */
    ba_message_model_t  msg_model;           /**< Oral or signed message model */
    ba_sync_model_t     sync_model;          /**< Synchrony model */
    ba_process_t        processes[BA_MAX_PROCESSES];
    int32_t             total_rounds;        /**< Total rounds executed */
    ba_message_t        global_log[BA_MAX_PROCESSES * BA_MAX_PROCESSES * BA_MAX_PROCESSES];
    int32_t             global_log_count;
} ba_system_t;

/**
 * @brief Result of a Byzantine agreement execution.
 */
typedef struct {
    bool    agreement_holds;     /**< All correct processes decided same value */
    bool    validity_holds;      /**< If all correct proposed v, then v is decided */
    bool    termination_holds;   /**< All correct processes eventually decided */
    int32_t decided_value;       /**< The agreed value (if agreement holds) */
    int32_t num_rounds;          /**< Rounds taken to decide */
} ba_result_t;

/**
 * @brief The three fundamental properties of Byzantine agreement.
 *
 * Lamport et al. (1982):
 *   Agreement:  All correct processes decide the same value.
 *   Validity:   If all correct processes propose the same value v,
 *               then every correct process decides v.
 *   Termination: Every correct process eventually decides.
 */
typedef struct {
    bool agreement;
    bool validity;
    bool termination;
} ba_properties_t;

/* ================================================================
 * L4: Fundamental Theorems (Structures for lower bound proofs)
 * ================================================================ */

/**
 * @brief A scenario (configuration + fault assignment) used in
 *        impossibility proofs.
 *
 * The classical lower bound proof (Lamport 1982) constructs three
 * scenarios A, B, C that are pairwise indistinguishable to some
 * correct process, leading to a contradiction if f >= N/3.
 */
typedef struct {
    int32_t  scenario_id;
    int32_t  num_processes;
    int32_t  faulty_processes[BA_MAX_PROCESSES];
    int32_t  num_faulty;
    int32_t  initial_values[BA_MAX_PROCESSES];
    int32_t  **message_history;    /**< 2D: [round][process] → received value */
    int32_t  decided_values[BA_MAX_PROCESSES];
} ba_scenario_t;

/**
 * @brief The N/3 lower bound theorem parameters.
 *
 * Theorem (Lamport, Shostak, Pease 1982):
 *   In a synchronous system with oral messages and N processes,
 *   Byzantine agreement is achievable iff f < N/3.
 *
 * The necessity proof (lower bound) shows impossibility when f >= N/3
 * by constructing an indistinguishability chain for N=3, f=1, then
 * extending via simulation argument to larger N.
 */
typedef struct {
    int32_t  N;          /**< Number of processes */
    int32_t  f;          /**< Number of faulty processes */
    bool     achievable; /**< f < N/3 ? */
    /** Three-process counterexample when N=3, f=1 */
    ba_scenario_t counterexample_scenarios[3];
} ba_lower_bound_theorem_t;

/* ================================================================
 * L1: Core API
 * ================================================================ */

/**
 * @brief Initialize a Byzantine agreement system.
 * @param sys       Pointer to system to initialize.
 * @param N         Number of processes.
 * @param f         Maximum faulty processes tolerated.
 * @param msg_model Oral or signed message model.
 * @param sync_model Synchrony model.
 * @return 0 on success, -1 on invalid parameters.
 *
 * Complexity: O(N)
 * Theorem mapping: LSP82 §2 - System Model
 */
int  ba_system_init(ba_system_t *sys, int32_t N, int32_t f,
                    ba_message_model_t msg_model, ba_sync_model_t sync_model);

/**
 * @brief Assign initial values and fault types to processes.
 * @param sys      System to configure.
 * @param values   Array of N initial values.
 * @param faults   Array of N fault types.
 * @return 0 on success, -1 on error.
 *
 * Complexity: O(N)
 */
int  ba_configure_processes(ba_system_t *sys, const int32_t *values,
                             const ba_fault_type_t *faults);

/**
 * @brief Check whether Byzantine agreement is theoretically possible.
 *
 * Applies the f < N/3 bound for oral messages, f < N/2 for signed,
 * and FLP impossibility for asynchronous systems.
 *
 * @param sys  System to check.
 * @return true if agreement is theoretically achievable.
 *
 * Complexity: O(1)
 * Theorem: LSP82, FLP85, DS83
 */
bool ba_is_achievable(const ba_system_t *sys);

/**
 * @brief Get the tight lower bound on faulty processes.
 * @param N          Number of processes.
 * @param msg_model  Message model.
 * @param sync_model Synchrony model.
 * @return Maximum f for which agreement is possible; -1 if impossible.
 *
 * Complexity: O(1)
 */
int32_t ba_max_tolerable_faults(int32_t N, ba_message_model_t msg_model,
                                 ba_sync_model_t sync_model);

/**
 * @brief The classical three-process impossibility proof.
 *
 * For N=3, f=1 with oral messages, no protocol can achieve Byzantine
 * agreement. This function enumerates all possible protocols and returns
 * the impossibility argument structure.
 *
 * @param sys  A 3-process system to analyze.
 * @param result Output parameter with the proof structure.
 * @return 0 if impossibility is verified, 1 if a protocol exists.
 *
 * Complexity: O(2^(3*3)) enumerating message patterns
 * Theorem: LSP82 Theorem 1
 */
int  ba_prove_n3_f1_impossible(ba_system_t *sys, ba_lower_bound_theorem_t *result);

/**
 * @brief Compute the indistinguishability relation between two scenarios.
 *
 * Two scenarios are indistinguishable to process p if p's message history
 * is identical in both scenarios. This is the core combinatorial tool
 * in all Byzantine agreement lower bounds.
 *
 * @param a  First scenario.
 * @param b  Second scenario.
 * @param pid Process whose perspective is checked.
 * @return true if scenarios are indistinguishable to pid.
 *
 * Complexity: O(R * N) where R = number of rounds
 */
bool ba_indistinguishable_to(const ba_scenario_t *a, const ba_scenario_t *b,
                              int32_t pid);

/**
 * @brief Verify all three Byzantine agreement properties on a completed run.
 * @param sys    Executed system.
 * @param result Output verification result.
 * @return 0 if verification completes, -1 on error.
 *
 * Complexity: O(N^2)
 */
int  ba_verify_properties(const ba_system_t *sys, ba_result_t *result);

/**
 * @brief Clean up and free resources in a system.
 * @param sys System to destroy.
 */
void ba_system_destroy(ba_system_t *sys);

#endif /* BYZANTINE_AGREEMENT_H */
