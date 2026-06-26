/**
 * @file flp_consensus.h
 * @brief Consensus problem formalization for the FLP proof.
 *
 * The consensus problem: N processes each start with an input from {0,1}.
 * A correct protocol must satisfy:
 *   1. Agreement:    All correct processes decide the same value.
 *   2. Validity:     The decided value was some input.
 *   3. Termination:  Every correct process eventually decides.
 *
 * The FLP theorem: in an async system with f >= 1,
 * NO deterministic protocol satisfies all three simultaneously.
 *
 * Knowledge Coverage:
 *   L1: Consensus problem definition
 *   L2: Safety vs Liveness
 *   L3: Formal specification as predicate on runs
 *   L4: FLP impossibility theorem statement
 *   L6: Consensus as canonical distributed computing problem
 *
 * Course Alignment: MIT 6.852 Lec 5-7, Stanford CS244E, CMU 15-712,
 *   Cambridge Part II, ETH 263-4650
 */

#ifndef FLP_CONSENSUS_H
#define FLP_CONSENSUS_H

#include "flp_common.h"
#include "flp_protocol.h"

bool flp_consensus_agreement_holds(flp_decision_value d1,
                                    flp_decision_value d2);

bool flp_consensus_validity_holds(const flp_configuration *cfg,
                                   flp_decision_value decision);

bool flp_consensus_config_is_consistent(const flp_configuration *cfg);

typedef enum {
    FLP_CONSENSUS_DETERMINISTIC = 0,
    FLP_CONSENSUS_RANDOMIZED = 1,
    FLP_CONSENSUS_FAILURE_DETECTOR = 2,
    FLP_CONSENSUS_SYNCHRONOUS = 3,
    FLP_CONSENSUS_K_SET = 4,
} flp_consensus_variant;

const char *flp_consensus_variant_name(flp_consensus_variant v);

/**
 * FLP Theorem (Fischer-Lynch-Paterson 1985):
 *   In an asynchronous system with N >= 2 processes and one
 *   possible crash fault, no deterministic protocol solves
 *   binary consensus.
 *
 * Proof via bivalence:
 *   Lemma 1: Disjoint events commute.
 *   Lemma 2: A bivalent initial configuration exists.
 *   Lemma 3: From a bivalent config, you can reach another
 *            bivalent config after applying any pending event.
 *   Adversary constructs infinite non-deciding run.
 */
typedef struct {
    bool                impossible;
    bool                has_bivalent_init;
    int32_t             bivalent_count;
    int32_t             max_nondeciding;
    flp_configuration   witness_config;
    flp_schedule        infinite_prefix;
    const char          *explanation;
} flp_theorem_result;

void flp_analyze_protocol(const flp_protocol_desc *desc, int32_t n,
                           int32_t max_depth, flp_theorem_result *result);

void flp_theorem_result_print(const flp_theorem_result *result);

void flp_consensus_all_zero_config(flp_configuration *cfg, int32_t n);
void flp_consensus_all_one_config(flp_configuration *cfg, int32_t n);
void flp_consensus_split_config(flp_configuration *cfg, int32_t n);
void flp_consensus_custom_config(flp_configuration *cfg, int32_t n,
                                  const int32_t *inputs);

int32_t flp_consensus_count_bivalent_initial(const flp_protocol_desc *desc,
                                              int32_t n, int32_t max_depth);

void flp_consensus_classify_initial_configs(const flp_protocol_desc *desc,
                                             int32_t n, int32_t max_depth,
                                             int32_t *count_0,
                                             int32_t *count_1,
                                             int32_t *count_biv);

#endif /* FLP_CONSENSUS_H */
