/**
 * @file phase_protocols.h
 * @brief Phase King, Ben-Or randomized consensus, and Dolev-Strong
 *        authenticated algorithm implementations.
 *
 * Covers L5 (Algorithms/Methods): Multiple Byzantine agreement protocols
 * beyond EIG, each illustrating a different approach to fault tolerance.
 *
 * Protocols:
 *   Phase King (Berman, Garay 1993): f+1 phases, each with a designated
 *       "king" whose value is adopted if no majority exists. Requires
 *       f < N/4 in the original, later improved.
 *   Ben-Or (1983): Randomized consensus for asynchronous systems. Uses
 *       coin flips to break symmetry when no majority exists.
 *       Tolerates f < N/5 Byzantine or f < N/2 crash failures.
 *   Dolev-Strong (1983): Authenticated Byzantine agreement with
 *       unforgeable signatures. Uses a relay-set mechanism.
 *
 * Course mapping: MIT 6.852 Lectures 7-8, Princeton COS 551,
 *                 CMU 15-855, ETH 263-4650
 */

#ifndef PHASE_PROTOCOLS_H
#define PHASE_PROTOCOLS_H

#include <stdint.h>
#include <stdbool.h>
#include "byzantine_agreement.h"

/* ================================================================
 * L5: Phase King Algorithm
 *
 * The Phase King algorithm runs in (f+1) phases. Each phase has two
 * rounds:
 *   Round 1: All processes broadcast their current estimate.
 *            Each process counts occurrences of each value.
 *   Round 2: The designated "king" for this phase broadcasts its
 *            estimate. Each process updates:
 *              if count[majority] > N/2 + f:
 *                 estimate = majority
 *              else:
 *                 estimate = king's value
 *
 * By the end of phase f+1, all correct processes agree.
 * ================================================================ */

typedef struct {
    int32_t pid;
    int32_t estimate;            /**< Current value estimate */
    int32_t phase;               /**< Current phase (0..f) */
    int32_t round_in_phase;      /**< 0 or 1 within a phase */
    /** Vote counts for round 1 */
    int32_t votes[BA_MAX_VALUES];
    int32_t num_votes;
    /** King's value from round 2 */
    int32_t king_value;
    bool    has_decided;
    int32_t decision;
} phase_king_process_t;

typedef struct {
    int32_t               N;
    int32_t               f;
    int32_t               current_phase;
    phase_king_process_t  processes[BA_MAX_PROCESSES];
    int32_t               king_schedule[BA_MAX_PROCESSES]; /**< King for each phase */
    bool                  agreement_reached;
} phase_king_system_t;

/**
 * @brief Initialize Phase King system.
 * @param pk        System to initialize.
 * @param N         Number of processes.
 * @param f         Fault tolerance.
 * @param values    Initial estimates.
 * @param faults    Fault types.
 * @return 0 on success, -1 on error.
 *
 * Requirement: f < N/4 for original Phase King.
 * Complexity: O(N)
 */
int  phase_king_init(phase_king_system_t *pk, int32_t N, int32_t f,
                     const int32_t *values, const ba_fault_type_t *faults);

/**
 * @brief Execute one phase of the Phase King protocol.
 * @param pk Phase King system.
 * @return 0 if protocol continues, 1 if agreement reached.
 */
int  phase_king_step(phase_king_system_t *pk);

/**
 * @brief Run complete Phase King protocol.
 * @param pk     System to run.
 * @param result Output agreement result.
 * @return 0 on success.
 */
int  phase_king_run(phase_king_system_t *pk, ba_result_t *result);

/* ================================================================
 * L5: Ben-Or Randomized Consensus
 *
 * Ben-Or's algorithm operates in asynchronous rounds. Each round:
 *   1. Broadcast estimate to all.
 *   2. Wait for N-f messages.
 *      If any value has > N/2 votes, adopt it; else adopt ?.
 *   3. Broadcast adopted value.
 *   4. Wait for N-f adopt messages.
 *      If any value v has at least f+1 votes:
 *        decide v, but continue relaying.
 *        estimate = v for next round.
 *      If all non-? votes are same v and count(v) > 0:
 *        estimate = v
 *      Else:
 *        estimate = coin_flip()  ← randomization
 * ================================================================ */

typedef struct {
    int32_t pid;
    int32_t estimate;            /**< Current estimate (0 or 1 in classic Ben-Or) */
    int32_t round;
    /** Phase 1: received estimates */
    int32_t recv_estimates[BA_MAX_PROCESSES];
    int32_t num_recv_estimates;
    /** Phase 2: received adoptions */
    int32_t recv_adoptions[BA_MAX_PROCESSES];
    int32_t num_recv_adoptions;
    int32_t adopted_value;       /**< -1 for ? */
    bool    has_decided;
    int32_t decision;
    /** For faulty behavior simulation */
    ba_fault_type_t fault_type;
} benor_process_t;

typedef struct {
    int32_t           N;
    int32_t           f;
    benor_process_t   processes[BA_MAX_PROCESSES];
    int32_t           max_rounds;    /**< Safety bound for simulation */
    int32_t           current_round;
    bool              agreement_reached;
    /** Shared coin (for deterministic simulation, we use a seeded PRNG) */
    uint64_t          coin_seed;
} benor_system_t;

/**
 * @brief Initialize Ben-Or system.
 * @param bo       System to initialize.
 * @param N        Number of processes.
 * @param f        Fault tolerance.
 * @param values   Initial estimates (only 0 or 1 in classic Ben-Or).
 * @param faults   Fault types.
 * @param seed     PRNG seed for coin flips.
 * @return 0 on success, -1 on error.
 *
 * Complexity: O(N)
 */
int  benor_init(benor_system_t *bo, int32_t N, int32_t f,
                const int32_t *values, const ba_fault_type_t *faults,
                uint64_t seed);

/**
 * @brief Execute one round of Ben-Or.
 * @param bo Ben-Or system.
 * @return 0 if protocol continues, 1 if all correct decided.
 */
int  benor_step(benor_system_t *bo);

/**
 * @brief Run Ben-Or to completion (or max_rounds).
 * @param bo     System to run.
 * @param result Output result.
 * @return 0 on success.
 */
int  benor_run(benor_system_t *bo, ba_result_t *result);

/**
 * @brief Compute the expected round complexity for Ben-Or.
 * @param N Number of processes.
 * @param f Faulty count.
 * @return Expected number of rounds (constant in expectation).
 *
 * Complexity: O(1)
 * Theorem: Ben-Or (1983) — expected O(1) rounds with fair coin.
 */
double benor_expected_rounds(int32_t N, int32_t f);

/* ================================================================
 * L5: Dolev-Strong Authenticated Algorithm
 *
 * In the signed message model, each process collects a set of
 * (value, proof_chain) pairs. A process decides on a value v if
 * it has seen v authenticated by at least one correct process
 * and has not seen any conflicting authenticated value.
 *
 * The key data structure is the "extracted set":
 *   extract(p, r) = { (v, chain) | process p has received v
 *                     with a valid chain of r distinct signatures }
 *
 * Decision rule at round f+1:
 *   If |values in extract(p, f+1)| == 1, decide that value.
 *   Otherwise, decide a default value.
 * ================================================================ */

/**
 * @brief A signed message chain entry.
 */
typedef struct {
    int32_t  value;
    int32_t  signers[BA_MAX_PROCESSES]; /**< Ordered list of signers */
    int32_t  num_signers;
    uint64_t chain_hash;                /**< Simplified signature = hash of chain */
    bool     verified;                  /**< Has the chain been verified? */
} ds_chain_entry_t;

/**
 * @brief Extract set for one process in Dolev-Strong.
 */
typedef struct {
    int32_t          pid;
    ds_chain_entry_t entries[BA_MAX_PROCESSES * BA_MAX_PROCESSES];
    int32_t          num_entries;
    int32_t          round;
} ds_extract_t;

/**
 * @brief Dolev-Strong system state.
 */
typedef struct {
    int32_t        N;
    int32_t        f;
    ds_extract_t   extracts[BA_MAX_PROCESSES];
    ba_fault_type_t fault_types[BA_MAX_PROCESSES];
    int32_t        current_round;
} ds_system_t;

/**
 * @brief Initialize Dolev-Strong system.
 * @param ds       System to initialize.
 * @param N        Number of processes.
 * @param f        Fault tolerance.
 * @param values   Initial values.
 * @param faults   Fault types.
 * @return 0 on success, -1 on error.
 */
int  ds_init(ds_system_t *ds, int32_t N, int32_t f,
             const int32_t *values, const ba_fault_type_t *faults);

/**
 * @brief Execute one round of Dolev-Strong.
 * @param ds Dolev-Strong system.
 * @return 0 if protocol continues, 1 if done.
 */
int  ds_step(ds_system_t *ds);

/**
 * @brief Run Dolev-Strong to completion (f+1 rounds).
 * @param ds     System to run.
 * @param result Output result.
 * @return 0 on success.
 */
int  ds_run(ds_system_t *ds, ba_result_t *result);

/**
 * @brief Verify a signature chain.
 *
 * In the simplified model, a hash chain is valid if:
 *   - The first signer is the original sender of the value.
 *   - No signer appears twice (no forging).
 *   - The chain length does not exceed N.
 *
 * @param chain Chain to verify.
 * @return true if chain is valid.
 *
 * Complexity: O(num_signers)
 */
bool ds_verify_chain(const ds_chain_entry_t *chain);

/**
 * @brief Generate a signature for a chain extension.
 * @param chain     Existing chain.
 * @param new_signer PID of the new signer.
 * @param new_hash  Output: new chain hash.
 * @return 0 on success.
 *
 * Complexity: O(chain length)
 */
int  ds_sign_chain(const ds_chain_entry_t *chain, int32_t new_signer,
                   uint64_t *new_hash);

#endif /* PHASE_PROTOCOLS_H */
