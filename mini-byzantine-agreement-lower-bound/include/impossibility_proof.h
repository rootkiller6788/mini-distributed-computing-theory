/**
 * @file impossibility_proof.h
 * @brief Formal structures for Byzantine agreement impossibility proofs.
 *
 * Covers L4 (Fundamental Laws): The lower bound f < N/3 for oral messages
 * and the FLP impossibility for asynchronous consensus are among the most
 * celebrated impossibility results in distributed computing.
 *
 * Proof techniques:
 *   1. Indistinguishability chain (LSP82): Construct scenario A, B, C
 *      such that A ~_1 B ~_2 C ~_3 A (where ~_p means indistinguishable
 *      to process p), creating a contradiction.
 *   2. Bivalence + hooking (FLP85): Show that there exists an initial
 *      bivalent configuration, and that from any bivalent configuration
 *      there exists a reachable bivalent configuration, implying an
 *      infinite non-deciding execution.
 *   3. Simulation argument: Extend impossibility from N=3 to general N.
 *
 * Reference: Lamport, Shostak, Pease (1982) "The Byzantine Generals Problem"
 *            Fischer, Lynch, Paterson (1985) "Impossibility of Distributed
 *              Consensus with One Faulty Process"
 *            Dolev, Strong (1983) "Authenticated Algorithms for Byzantine Agreement"
 */

#ifndef IMPOSSIBILITY_PROOF_H
#define IMPOSSIBILITY_PROOF_H

#include <stdint.h>
#include <stdbool.h>
#include "byzantine_agreement.h"
#include "msg_automaton.h"

/* ================================================================
 * L4: LSP82 Lower Bound (f < N/3 for oral messages)
 * ================================================================ */

/**
 * @brief The three-scenario indistinguishability construction.
 *
 * For N=3, f=1, let the three processes be G (general/commander),
 * L1, L2 (lieutenants). WLOG, one of L1 or L2 is faulty.
 *
 * Scenario A: G is correct, proposes 0. L1 is faulty.
 *   - G sends 0 to L1 and L2.
 *   - L1 (faulty) tells L2 that G told L1 value 1.
 *   - L2 sees: "G says 0; L1 says G said 1."
 *
 * Scenario B: G is correct, proposes 1. L2 is faulty.
 *   - G sends 1 to L1 and L2.
 *   - L2 (faulty) tells L1 that G told L2 value 0.
 *   - L1 sees: "G says 1; L2 says G said 0."
 *
 * Scenario C: G is faulty, sends 0 to L1 and 1 to L2.
 *   - L1 sees: "G says 0."
 *   - L2 sees: "G says 1."
 *
 * Then:
 *   A ~_L2 B (L2 cannot distinguish A from B: in both, L2 sees G says 0)
 *   A ~_L1 C (L1 cannot distinguish A from C: in both, L1 sees G says 0)
 *   B ~_L2 C (L2 cannot distinguish B from C: in both, L2 sees G says 1)
 *
 * By agreement and validity, this forces a contradiction.
 */

typedef enum {
    SCENARIO_A = 0,   /**< G correct(0), L1 faulty, G→L1,L2:0; L1→L2: G said 1 */
    SCENARIO_B = 1,   /**< G correct(1), L2 faulty, G→L1,L2:1; L2→L1: G said 0 */
    SCENARIO_C = 2,   /**< G faulty, G→L1:0, G→L2:1 */
    NUM_LSP82_SCENARIOS = 3
} lsp82_scenario_id_t;

/**
 * @brief Message level for the 3-process counterexample.
 *
 * In round 1 (r=1), the general sends to both lieutenants.
 * In round 2 (r=2), each lieutenant relays what the general told it.
 */
typedef struct {
    int32_t round;
    int32_t sender;
    int32_t receiver;
    int32_t reported_value;
    int32_t reported_sender; /**< Who originally said this (for relay msgs) */
} lsp82_message_t;

/**
 * @brief Complete message transcript for one scenario.
 */
typedef struct {
    lsp82_scenario_id_t id;
    lsp82_message_t     messages[16];  /**< All messages sent */
    int32_t             num_messages;
    int32_t             g_value;       /**< What general actually proposes */
    bool                g_correct;     /**< Is general correct? */
    bool                l1_correct;     /**< Is lieutenant 1 correct? */
    bool                l2_correct;     /**< Is lieutenant 2 correct? */
} lsp82_transcript_t;

/**
 * @brief The full LSP82 impossibility proof structure.
 */
typedef struct {
    lsp82_transcript_t scenarios[NUM_LSP82_SCENARIOS];
    /** Indistinguishability pairs: (scenario_i, scenario_j, process_p) */
    struct {
        lsp82_scenario_id_t a;
        lsp82_scenario_id_t b;
        int32_t             pid;
    } indist_pairs[6];
    int32_t num_indist_pairs;
    /** The contradiction chain */
    char contradiction_chain[512];
    bool proof_valid;
} lsp82_proof_t;

/* ================================================================
 * L4: FLP85 Impossibility (Asynchronous consensus with 1 crash)
 * ================================================================ */

/**
 * @brief FLP bivalence graph node.
 *
 * The FLP proof constructs a potentially infinite graph of configurations
 * connected by transitions. A bivalent configuration has both 0-decided
 * and 1-decided configurations reachable from it.
 */
typedef struct {
    automaton_config_t config;
    automaton_valence_t valence;
    int32_t             distance_from_initial; /**< Steps from initial config */
    bool                visited;
} flp_config_node_t;

/**
 * @brief An edge in the FLP configuration graph.
 *
 * Represents one process taking a step (receiving a message and transitioning).
 * In the asynchronous model, the adversary (scheduler) chooses which process
 * takes the next step.
 */
typedef struct {
    int32_t from_node;
    int32_t to_node;
    int32_t pid;           /**< Which process moves */
    int32_t message_sender; /**< Which process's message is delivered */
} flp_transition_t;

/**
 * @brief The FLP impossibility proof structure.
 *
 * Contains the initial bivalent configuration and the proof that
 * from any bivalent configuration, a bivalent successor is reachable,
 * allowing indefinite postponement of decision.
 */
typedef struct {
    flp_config_node_t *configs;
    int32_t             num_configs;
    int32_t             config_capacity;
    flp_transition_t   *transitions;
    int32_t             num_transitions;
    int32_t             trans_capacity;
    /** Whether the initial configuration is bivalent (it always is) */
    bool                initial_bivalent;
    /** Proof that a bivalent config has a reachable bivalent successor */
    bool                bivalence_preserved;
} flp_proof_t;

/* ================================================================
 * L4: Dolev-Strong bound (f < N/2 for signed messages)
 * ================================================================ */

/**
 * @brief Authenticated (signed) message relay chain.
 *
 * In the signed model, each process appends its unforgeable signature.
 * A correct process can verify the entire chain of signatures.
 */
typedef struct {
    int32_t  chain[BA_MAX_PROCESSES]; /**< Ordered list of signers */
    int32_t  chain_len;
    int32_t  value;
    uint64_t signatures[BA_MAX_PROCESSES]; /**< One signature per signer */
} ds_signed_message_t;

/**
 * @brief Dolev-Strong authenticated Byzantine agreement proof parameters.
 *
 * Theorem (DS83): With unforgeable signatures, Byzantine agreement
 * is achievable iff f < N (i.e., any number of faults), but with
 * authentication the bound is f < N/2 for the standard model.
 *
 * Actually the exact bound depends on the model. In the full
 * information model with signatures, agreement requires f < N/2
 * if we want both agreement and validity. If we only need agreement,
 * f < N is sufficient with signatures.
 */
typedef struct {
    int32_t N;
    int32_t f;
    bool    agreement_with_signatures;  /**< f < N → yes for agreement-only */
    bool    strong_validity;            /**< f < N/2 needed for strong validity */
} ds_bound_result_t;

/* ================================================================
 * L4: APIs for impossibility proofs
 * ================================================================ */

/**
 * @brief Construct the LSP82 three-scenario impossibility proof.
 *
 * Creates scenarios A, B, C as described above and computes the
 * indistinguishability relations. Verifies that the proof is valid
 * (contradiction is reached).
 *
 * @param proof Output proof structure.
 * @return 0 on success, -1 on error.
 *
 * Complexity: O(1)
 * Theorem: LSP82 Theorem 1
 */
int  lsp82_construct_proof(lsp82_proof_t *proof);

/**
 * @brief Verify that the LSP82 proof is sound.
 *
 * Checks: (i) each indistinguishability pair is correct,
 * (ii) the chain of implications yields a contradiction,
 * (iii) validity and agreement cannot simultaneously hold.
 *
 * @param proof The proof to verify.
 * @return true if the proof is valid.
 *
 * Complexity: O(1)
 */
bool lsp82_verify_proof(const lsp82_proof_t *proof);

/**
 * @brief Extend the 3-process impossibility to N processes, f >= N/3,
 *        using a simulation argument.
 *
 * Given any N and f with f >= N/3, partition the N processes into
 * 3 groups simulating the 3-process case. If a protocol existed for
 * (N, f), it could be transformed into a protocol for (3, 1),
 * contradicting the impossibility.
 *
 * @param N Number of processes.
 * @param f Number of faulty processes.
 * @return true if the simulation argument shows impossibility.
 *
 * Complexity: O(1)
 * Theorem: LSP82 Corollary 1
 */
bool lsp82_simulation_argument(int32_t N, int32_t f);

/**
 * @brief Construct the FLP impossibility proof for N processes, 1 crash.
 *
 * Builds the initial configuration, shows it is bivalent, and constructs
 * the proof that bivalence is preserved under all possible single steps.
 *
 * @param proof Output proof structure.
 * @param N     Number of processes (typically 2).
 * @return 0 on success, -1 on error.
 *
 * Complexity: O(|configs| * |transitions|) — exhaustive for small N
 * Theorem: FLP85 Theorem 1
 */
int  flp_construct_proof(flp_proof_t *proof, int32_t N);

/**
 * @brief Verify the FLP proof.
 *
 * Checks: (i) the initial configuration is bivalent,
 * (ii) every bivalent configuration has at least one applicable
 *      transition to another bivalent configuration,
 * (iii) therefore, an infinite non-deciding execution exists.
 *
 * @param proof The FLP proof.
 * @return true if the proof is valid.
 */
bool flp_verify_proof(const flp_proof_t *proof);

/**
 * @brief Compute the Dolev-Strong bound for signed messages.
 *
 * @param N      Number of processes.
 * @param result Output bound result.
 * @return 0 on success.
 *
 * Complexity: O(1)
 * Theorem: DS83
 */
int  ds_compute_bound(int32_t N, ds_bound_result_t *result);

/**
 * @brief General impossibility checker: given (N, f, msg_model, sync_model),
 *        determine if Byzantine agreement is possible and provide the
 *        relevant theorem reference.
 *
 * @param N           Number of processes.
 * @param f           Number of faulty processes.
 * @param msg_model   Oral / Signed.
 * @param sync_model  Sync / Async.
 * @param achievable  Output: true if agreement is possible.
 * @param theorem_ref Output: human-readable theorem reference.
 * @return 0 on success.
 *
 * Complexity: O(1)
 */
int  impossibility_check(int32_t N, int32_t f,
                          ba_message_model_t msg_model,
                          ba_sync_model_t sync_model,
                          bool *achievable, const char **theorem_ref);

/**
 * @brief Free resources associated with an LSP82 proof.
 * @param proof Proof to destroy.
 */
void lsp82_proof_destroy(lsp82_proof_t *proof);

/**
 * @brief Free resources associated with an FLP proof.
 * @param proof Proof to destroy.
 */
void flp_proof_destroy(flp_proof_t *proof);

#endif /* IMPOSSIBILITY_PROOF_H */
