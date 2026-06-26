/**
 * @file flp_simulation.h
 * @brief Simulation driver for FLP impossibility experiments.
 *
 * This module orchestrates the full FLP analysis: create a protocol,
 * explore its configuration space, classify configurations, and
 * demonstrate that no deterministic consensus is possible.
 *
 * Knowledge Coverage:
 *   L2: Adversarial scheduling
 *   L4: FLP impossibility demonstration
 *   L5: Simulation and exploration algorithms
 *   L7: Application to practical consensus systems
 *
 * Course Alignment: MIT 6.852 Lec 7, CMU 15-712 Lec 10
 */

#ifndef FLP_SIMULATION_H
#define FLP_SIMULATION_H

#include "flp_common.h"
#include "flp_config.h"
#include "flp_protocol.h"
#include "flp_message.h"
#include "flp_search.h"
#include "flp_consensus.h"

/* ---------------------------------------------------------------------------
 * Simulation Configuration
 * ---------------------------------------------------------------------------
 */

/** Simulation parameters controlling the FLP experiment. */
typedef struct {
    int32_t             num_processes;      /**< N, number of processes */
    flp_protocol_type   protocol;           /**< Which protocol to test */
    int32_t             max_depth;          /**< Configuration graph depth bound */
    int32_t             max_steps;          /**< Max schedule steps to construct */
    flp_msg_strategy    msg_strategy;       /**< Message delivery strategy */
    bool                crash_one_process;  /**< Whether to simulate a crash */
    flp_process_id      crash_target;       /**< Which process crashes (-1=none) */
    int32_t             crash_after_steps;  /**< Crash after this many steps */
    bool                verbose;            /**< Print detailed output */
    int32_t             random_seed;        /**< Seed for randomized strategies */
} flp_simulation_params;

/**
 * Initialize simulation parameters with sensible defaults.
 * N=3, flood-set protocol, depth=50, no crash, verbose=false.
 */
void flp_sim_params_init(flp_simulation_params *params);

/* ---------------------------------------------------------------------------
 * Simulation Results
 * ---------------------------------------------------------------------------
 */

/** Simulation outcome for one run. */
typedef struct {
    flp_theorem_result  flp_result;         /**< FLP theorem analysis */
    int32_t             configs_explored;   /**< Total configurations visited */
    int32_t             bivalent_found;     /**< Bivalent configurations found */
    int32_t             schedules_tried;    /**< Schedules explored */
    int32_t             max_schedule_len;   /**< Longest non-deciding schedule */
    double              exploration_time_ms; /**< Wall-clock time for exploration */
    bool                consensus_possible; /**< Did the protocol achieve consensus? */
    flp_decision_value  final_decision;     /**< Final decision if any */
} flp_simulation_result;

/* ---------------------------------------------------------------------------
 * Simulation Execution
 * ---------------------------------------------------------------------------
 */

/**
 * Run a complete FLP impossibility analysis.
 *
 * 1. Initialize the protocol and configuration space.
 * 2. Search for initial bivalent configurations (Lemma 2).
 * 3. If found, construct the non-deciding run (Lemma 3).
 * 4. If not found, verify whether the protocol actually solves consensus.
 *
 * @param params  Simulation parameters.
 * @param result  Output: simulation results.
 */
void flp_sim_run(const flp_simulation_params *params,
                  flp_simulation_result *result);

/**
 * Run the FLP analysis for ALL supported protocols and compare results.
 * This provides a comprehensive view of which protocol properties
 * lead to FLP impossibility.
 *
 * @param n          Number of processes.
 * @param max_depth  Exploration depth.
 * @param results    Output array (size = FLP_PROTO_COUNT).
 */
void flp_sim_compare_all_protocols(int32_t n, int32_t max_depth,
                                    flp_simulation_result *results);

/**
 * Print a simulation result in a human-readable format.
 */
void flp_sim_result_print(const flp_simulation_result *result);

/**
 * Print a comparison table of all protocol results.
 */
void flp_sim_print_comparison(const flp_simulation_result *results,
                               int32_t count);

/* ---------------------------------------------------------------------------
 * Adversary Strategies
 * ---------------------------------------------------------------------------
 * The asynchronous adversary controls message delivery order.
 * Different strategies model different assumptions about the network.
 */

/**
 * Adversary: select which message to deliver next to maximize
 * the chance of keeping the system bivalent (i.e., preventing consensus).
 *
 * @param cfg       Current configuration.
 * @param receiver  Process to receive a message.
 * @param strategy  Selection strategy.
 * @param out_msg   Output: the chosen message.
 * @return          true if a message was selected.
 */
bool flp_adversary_select_message(const flp_configuration *cfg,
                                   flp_process_id receiver,
                                   flp_msg_strategy strategy,
                                   flp_message *out_msg);

/**
 * Adversary: select which process takes the next step.
 * The adversary can choose any correct process.
 *
 * @param cfg      Current configuration.
 * @param strategy Selection strategy.
 * @return         Process ID to take next step, or -1 if none available.
 */
flp_process_id flp_adversary_select_process(const flp_configuration *cfg,
                                              flp_msg_strategy strategy);

/**
 * The FLP adversary strategies for constructing non-deciding runs:
 *
 * FLP_ADV_BIVALENCE_PRESERVING:
 *   Always choose messages/processes that keep the system bivalent.
 *
 * FLP_ADV_DELAY_SUSPECTED:
 *   Delay messages from processes that seem slow (not yet crashed).
 *
 * FLP_ADV_ROUND_ROBIN:
 *   Cycle through processes fairly (models synchronous assumption).
 */
typedef enum {
    FLP_ADV_BIVALENCE_PRESERVING = 0,
    FLP_ADV_DELAY_SUSPECTED = 1,
    FLP_ADV_ROUND_ROBIN = 2,
    FLP_ADV_RANDOM = 3,
} flp_adversary_strategy;

/**
 * Run the FLP adversary: given a bivalent configuration and a
 * strategy, produce a sequence of steps that keeps the system
 * from deciding.
 *
 * @param start_cfg  Bivalent starting configuration.
 * @param desc       Protocol descriptor.
 * @param strategy   Adversary strategy.
 * @param max_steps  Maximum steps to produce.
 * @param out_sched  Output: the constructed schedule.
 * @return           Number of steps in the schedule.
 */
int32_t flp_adversary_run(const flp_configuration *start_cfg,
                           const flp_protocol_desc *desc,
                           flp_adversary_strategy strategy,
                           int32_t max_steps,
                           flp_schedule *out_sched);

#endif /* FLP_SIMULATION_H */
