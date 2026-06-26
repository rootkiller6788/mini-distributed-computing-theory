/**
 * @file flp_protocol.h
 * @brief Protocol abstraction for FLP impossibility analysis.
 *
 * A protocol in the FLP model is a deterministic function mapping
 * (process state, received message) -> (new state, outgoing messages).
 * This module provides the protocol interface and several concrete
 * consensus protocols to test the impossibility theorem against.
 *
 * Knowledge Coverage:
 *   L1: Deterministic protocol definition
 *   L2: Protocol as a transition function delta
 *   L3: State machine representation
 *   L5: Concrete consensus protocols (flood-set, phase-king, etc.)
 *   L6: Consensus problem as a protocol specification
 *
 * Course Alignment: MIT 6.852 Lec 5, Stanford CS244E, Cambridge Part II
 */

#ifndef FLP_PROTOCOL_H
#define FLP_PROTOCOL_H

#include "flp_common.h"
#include "flp_message.h"

/** Maximum outgoing messages per step (finite, as required by FLP). */
#define FLP_MAX_OUTGOING_MSG 16

/** Result of a single protocol step. */
typedef struct {
    flp_process_state   new_state;
    flp_message         outgoing[FLP_MAX_OUTGOING_MSG];
    int32_t             num_outgoing;
    bool                decided;
    flp_decision_value  decision;
} flp_step_result;

/** Protocol type identifier for dispatch. */
typedef enum {
    FLP_PROTO_FLOOD_SET = 0,
    FLP_PROTO_PHASE_KING = 1,
    FLP_PROTO_ECHO = 2,
    FLP_PROTO_ROTATING_LEADER = 3,
    FLP_PROTO_MAJORITY_VOTE = 4,
    FLP_PROTO_TWO_PHASE = 5,
    FLP_PROTO_COUNT = 6
} flp_protocol_type;

/** Protocol descriptor: bundles type with configuration. */
typedef struct {
    flp_protocol_type   type;
    int32_t             num_processes;
    int32_t             fault_tolerance;
    flp_round_t         max_rounds;
    bool                uses_timeout;
    bool                is_randomized;
    const char          *name;
} flp_protocol_desc;

/* ---------------------------------------------------------------------------
 * Core Protocol Functions
 * ---------------------------------------------------------------------------
 */

/**
 * Initialize a protocol descriptor.
 */
void flp_protocol_desc_init(flp_protocol_desc *desc, flp_protocol_type type,
                             int32_t n, int32_t f);

/**
 * Execute a single step of the specified protocol.
 * This is the core transition function delta of the FLP model.
 *
 * Reference: FLP (1985) Section 2, "The Model"
 */
void flp_protocol_step(const flp_protocol_desc *desc,
                        const flp_process_state *state,
                        const flp_message *msg,
                        flp_step_result *result);

/* ---------------------------------------------------------------------------
 * Individual Protocol Step Functions (exposed for testing)
 * ---------------------------------------------------------------------------
 * Each implements a distinct consensus algorithm.
 * These real protocols fail the FLP conditions in different ways,
 * demonstrating the sharpness of the impossibility result.
 */

/**
 * Flood-Set Protocol (Lynch 1996, Section 6.2.1):
 * Broadcast knowledge set; decide when enough values known.
 * Fails in async model: cannot distinguish slow process from crash.
 */
void flp_proto_flood_set_step(const flp_protocol_desc *desc,
                               const flp_process_state *state,
                               const flp_message *msg,
                               flp_step_result *result);

/**
 * Phase-King Protocol (Berman-Garay 1989):
 * Phases with designated king; works sync with f < n/2.
 * Fails async: king may be slow, not crashed.
 */
void flp_proto_phase_king_step(const flp_protocol_desc *desc,
                                const flp_process_state *state,
                                const flp_message *msg,
                                flp_step_result *result);

/**
 * Echo-Based Consensus:
 * Leader proposes, followers echo, decide on majority.
 */
void flp_proto_echo_step(const flp_protocol_desc *desc,
                          const flp_process_state *state,
                          const flp_message *msg,
                          flp_step_result *result);

/**
 * Rotating Leader Protocol:
 * Simplified Paxos-style round-robin leadership.
 */
void flp_proto_rotating_leader_step(const flp_protocol_desc *desc,
                                     const flp_process_state *state,
                                     const flp_message *msg,
                                     flp_step_result *result);

/**
 * Majority Vote Protocol:
 * Simple voting without retry. Unsafe under faults.
 */
void flp_proto_majority_vote_step(const flp_protocol_desc *desc,
                                   const flp_process_state *state,
                                   const flp_message *msg,
                                   flp_step_result *result);

/**
 * Two-Phase Protocol:
 * Phase 1 collect, Phase 2 commit. Similar to 2PC.
 */
void flp_proto_two_phase_step(const flp_protocol_desc *desc,
                               const flp_process_state *state,
                               const flp_message *msg,
                               flp_step_result *result);

/* ---------------------------------------------------------------------------
 * Protocol Analysis Functions
 * ---------------------------------------------------------------------------
 */

/**
 * Check protocol validity: all-same-input => only that value decided.
 * @param desc       Protocol descriptor.
 * @param n          Number of processes.
 * @param max_depth  Exploration bound.
 * @return           true if validity holds within bound.
 */
bool flp_protocol_check_validity(const flp_protocol_desc *desc,
                                  int32_t n, int32_t max_depth);

/**
 * Check protocol agreement: no two correct processes decide differently.
 * @return true if no conflicting decisions found within bound.
 */
bool flp_protocol_check_agreement(const flp_protocol_desc *desc,
                                   int32_t n, int32_t max_depth);

/**
 * Check protocol termination: all correct processes eventually decide.
 * @return true if termination holds within bound.
 */
bool flp_protocol_check_termination(const flp_protocol_desc *desc,
                                     int32_t n, int32_t max_depth);

/**
 * Get human-readable name for a protocol type.
 */
const char *flp_protocol_type_name(flp_protocol_type type);

#endif /* FLP_PROTOCOL_H */
