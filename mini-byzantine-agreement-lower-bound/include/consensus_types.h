/**
 * @file consensus_types.h
 * @brief Distributed consensus type taxonomy and reductions.
 *
 * Covers L2 (Core Concepts): Classifies consensus variants and their
 * inter-reducibility. Byzantine agreement is the strongest form;
 * crash-fault tolerant consensus (Paxos/Raft) is a special case.
 *
 * Taxonomy:
 *   Crash Fault Tolerance (CFT): f < N/2 (Paxos, Raft)
 *   Byzantine Fault Tolerance (BFT) oral: f < N/3 (PBFT, EIG)
 *   BFT signed: f < N/2 (Dolev-Strong)
 *   BFT async: impossible (FLP) → use randomization (Ben-Or)
 *
 * Reference: Cachin, Guerraoui, Rodrigues (2011) "Introduction to
 *            Reliable and Secure Distributed Programming"
 *            Castro & Liskov (1999) "Practical Byzantine Fault Tolerance"
 */

#ifndef CONSENSUS_TYPES_H
#define CONSENSUS_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include "byzantine_agreement.h"

/* ================================================================
 * L2: Consensus Type Classification
 * ================================================================ */

/**
 * @brief Consensus protocol family.
 */
typedef enum {
    CONSENSUS_CRASH_STOP = 0,       /**< Processes crash, no restart (classic Paxos) */
    CONSENSUS_CRASH_RECOVERY = 1,   /**< Processes crash and recover (Raft) */
    CONSENSUS_BYZANTINE_ORAL = 2,   /**< Byzantine faults, oral messages */
    CONSENSUS_BYZANTINE_SIGNED = 3, /**< Byzantine faults, signed messages */
    CONSENSUS_RANDOMIZED = 4,       /**< Randomized consensus (Ben-Or, Rabin) */
    CONSENSUS_QUANTUM = 5           /**< Quantum Byzantine agreement */
} consensus_family_t;

/**
 * @brief Fault tolerance threshold for each consensus type.
 */
typedef struct {
    consensus_family_t family;
    int32_t             N;                /**< Total processes */
    int32_t             max_faulty;       /**< Maximum tolerable faulty */
    double              fault_ratio;      /**< f/N upper bound */
    const char         *bound_theorem;    /**< Reference theorem */
    bool                synchronous;      /**< Requires synchrony? */
    int32_t             min_rounds;       /**< Minimum rounds to decide */
} consensus_threshold_t;

/**
 * @brief Reduction: A protocol for type A can implement type B.
 *
 * Many reductions exist between consensus types. For example:
 *   Signed BFT → Oral BFT (by adding signatures)
 *   Crash consensus ← BFT (BFT is stronger, can simulate crash)
 */
typedef struct {
    consensus_family_t from_family;
    consensus_family_t to_family;
    int32_t             overhead_rounds;  /**< Extra rounds needed */
    int32_t             overhead_msgs;    /**< Extra messages needed */
    const char         *reduction_name;
    bool                is_tight;         /**< Is this reduction optimal? */
} consensus_reduction_t;

/* ================================================================
 * L2: Known Consensus Protocols
 * ================================================================ */

/**
 * @brief Paxos protocol parameters.
 *
 * Paxos (Lamport 1998) solves consensus in the crash-stop model
 * with f < N/2. It uses a leader-based two-phase protocol.
 */
typedef struct {
    int32_t  N;                   /**< Acceptors */
    int32_t  f;                   /**< Max crash faults */
    int32_t  quorum_size;         /**< ⌊N/2⌋ + 1 */
    int32_t  leader_id;           /**< Current leader (proposer) */
    int64_t  proposal_number;     /**< Monotonic proposal number */
    int32_t  proposed_value;
    int32_t  accepted_value[BA_MAX_PROCESSES];
    int64_t  accepted_proposal[BA_MAX_PROCESSES];
    bool     has_decided;
    int32_t  decided_value;
} paxos_state_t;

/**
 * @brief PBFT (Practical Byzantine Fault Tolerance) parameters.
 *
 * Castro & Liskov (1999). Requires f < N/3. Uses three-phase protocol:
 * pre-prepare, prepare, commit. Designed for efficiency.
 */
typedef struct {
    int32_t  N;
    int32_t  f;
    int32_t  quorum_size;         /**< 2f+1 */
    int32_t  view;                /**< Current view number */
    int32_t  primary_id;          /**< Primary for current view */
    int32_t  sequence_number;
    /** Message log for the three phases */
    int32_t  pre_prepare_values[BA_MAX_PROCESSES];
    int32_t  prepare_count;
    int32_t  commit_count;
    bool     prepared;
    bool     committed;
} pbft_state_t;

/* ================================================================
 * L2: Distributed System Models
 * ================================================================ */

/**
 * @brief Timing assumptions for distributed computing models.
 */
typedef enum {
    TIMING_SYNC_KNOWN = 0,       /**< Δ known, messages delivered within Δ */
    TIMING_SYNC_UNKNOWN = 1,     /**< Δ exists but unknown */
    TIMING_PARTIAL_SYNC_GST = 2, /**< Global Stabilization Time (GST) exists but unknown */
    TIMING_ASYNC = 3             /**< No timing guarantees at all */
} timing_model_t;

/**
 * @brief Failure detector abstraction (Chandra & Toueg 1996).
 *
 * Failure detectors provide (possibly unreliable) information about
 * which processes have failed. They are characterized by two properties:
 *   Completeness: eventually every crashed process is suspected
 *   Accuracy:    no correct process is ever suspected (strong)
 *
 * ◇S (eventually strong) is the weakest failure detector that
 * solves consensus in the asynchronous model with f < N/2.
 */
typedef enum {
    FD_PERFECT = 0,        /**< P: strong completeness + strong accuracy */
    FD_STRONG = 1,         /**< S: strong completeness + weak accuracy */
    FD_EVENTUALLY_STRONG = 2, /**< ◇S: strong completeness + eventual weak accuracy */
    FD_EVENTUALLY_WEAK = 3  /**< ◇W: weak completeness + eventual weak accuracy */
} failure_detector_class_t;

/**
 * @brief Failure detector state.
 */
typedef struct {
    failure_detector_class_t class;
    bool  suspected[BA_MAX_PROCESSES]; /**< Currently suspected processes */
    int32_t N;
    int32_t false_positives;           /**< Count of incorrect suspicions */
    int32_t false_negatives;           /**< Count of missed crashes */
} failure_detector_t;

/* ================================================================
 * L7: Applications — CAP Theorem connection
 * ================================================================ */

/**
 * @brief CAP Theorem: Consistency-Availability-Partition Tolerance.
 *
 * Brewer's CAP Theorem states that a distributed system can provide
 * at most two of: Consistency, Availability, Partition Tolerance.
 *
 * Connection to Byzantine agreement: In the presence of network
 * partitions (faults), consensus requires either sacrificing
 * availability (wait for quorum) or consistency (allow divergence).
 */
typedef struct {
    bool provides_consistency;      /**< All nodes see same data at same time */
    bool provides_availability;     /**< Every request receives a response */
    bool provides_partition_tolerance; /**< System works despite network partitions */
    /** Which two are achievable together */
    enum { CAP_CP, CAP_AP, CAP_CA } achievable_pair;
} cap_system_t;

/* ================================================================
 * L2: API
 * ================================================================ */

/**
 * @brief Get the fault tolerance threshold for a given consensus family.
 *
 * @param family   Consensus protocol family.
 * @param N        Number of processes.
 * @param threshold Output threshold structure.
 * @return 0 on success, -1 on invalid family.
 *
 * Complexity: O(1)
 */
int  consensus_get_threshold(consensus_family_t family, int32_t N,
                              consensus_threshold_t *threshold);

/**
 * @brief Check if a reduction from one consensus family to another exists.
 *
 * @param from      Source protocol family.
 * @param to        Target protocol family.
 * @param reduction Output reduction info if one exists.
 * @return true if a reduction exists.
 *
 * Complexity: O(1)
 */
bool consensus_has_reduction(consensus_family_t from, consensus_family_t to,
                              consensus_reduction_t *reduction);

/**
 * @brief Initialize a Paxos state for N acceptors with f crash tolerance.
 * @param paxos Paxos state to initialize.
 * @param N     Number of acceptors.
 * @param f     Maximum crash faults.
 * @return 0 on success, -1 if f >= N/2.
 */
int  paxos_init(paxos_state_t *paxos, int32_t N, int32_t f);

/**
 * @brief Paxos Phase 1: Prepare (leader proposes).
 * @param paxos    Paxos state.
 * @param proposal Proposal number.
 * @return 0 if quorum promises, 1 if rejected.
 */
int  paxos_phase1_prepare(paxos_state_t *paxos, int64_t proposal);

/**
 * @brief Paxos Phase 2: Accept (leader proposes value).
 * @param paxos  Paxos state.
 * @param value  Proposed value.
 * @return 0 if accepted by quorum, 1 if rejected.
 */
int  paxos_phase2_accept(paxos_state_t *paxos, int32_t value);

/**
 * @brief Initialize a PBFT state.
 * @param pbft PBFT state.
 * @param N    Number of replicas.
 * @param f    Maximum Byzantine faults.
 * @return 0 on success, -1 if f >= N/3.
 */
int  pbft_init(pbft_state_t *pbft, int32_t N, int32_t f);

/**
 * @brief PBFT pre-prepare phase: primary sends request.
 * @param pbft PBFT state.
 * @param value Proposed value.
 * @return 0 on success.
 */
int  pbft_pre_prepare(pbft_state_t *pbft, int32_t value);

/**
 * @brief PBFT prepare phase: replicas acknowledge.
 * @param pbft PBFT state.
 * @return 0 if prepared, 1 if not yet.
 */
int  pbft_prepare(pbft_state_t *pbft);

/**
 * @brief PBFT commit phase: replicas commit.
 * @param pbft PBFT state.
 * @return 0 if committed, 1 if not yet.
 */
int  pbft_commit(pbft_state_t *pbft);

/**
 * @brief Initialize a failure detector.
 * @param fd    Failure detector to initialize.
 * @param class Desired failure detector class.
 * @param N     Number of monitored processes.
 * @return 0 on success.
 */
int  failure_detector_init(failure_detector_t *fd,
                            failure_detector_class_t class, int32_t N);

/**
 * @brief Update failure detector with a heartbeat observation.
 * @param fd      Failure detector.
 * @param pid     Process observed.
 * @param alive   true if process responded to heartbeat.
 */
void failure_detector_heartbeat(failure_detector_t *fd, int32_t pid, bool alive);

/**
 * @brief Query suspicion status of a process.
 * @param fd  Failure detector.
 * @param pid Process to query.
 * @return true if suspected faulty.
 */
bool failure_detector_is_suspected(const failure_detector_t *fd, int32_t pid);

/**
 * @brief Evaluate CAP properties for a given system configuration.
 * @param N            Number of nodes.
 * @param f            Number of tolerable faults.
 * @param msg_model    Message model.
 * @param sync_model   Synchrony model.
 * @param cap_result   Output CAP analysis.
 * @return 0 on success.
 */
int  cap_evaluate(int32_t N, int32_t f, ba_message_model_t msg_model,
                   ba_sync_model_t sync_model, cap_system_t *cap_result);

#endif /* CONSENSUS_TYPES_H */
