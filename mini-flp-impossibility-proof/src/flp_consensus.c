/**
 * @file flp_consensus.c
 * @brief Consensus problem formalization and FLP theorem analysis.
 *
 * The consensus problem: N processes with inputs {0,1} must agree on
 * exactly one output that equals some input.
 *
 * Properties:
 *   Agreement:   All correct processes decide the same value.
 *   Validity:    Decided value equals some process's input.
 *   Termination: Every correct process eventually decides.
 *
 * The FLP theorem (1985) states: in an asynchronous system with
 * f >= 1, no deterministic protocol can satisfy all three.
 *
 * Reference: FLP (1985) JACM 32(2):374-382
 */

#include "flp_consensus.h"
#include "flp_config.h"
#include "flp_protocol.h"
#include "flp_message.h"
#include "flp_search.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ===== Consensus Property Checking ===== */

bool flp_consensus_agreement_holds(flp_decision_value d1,
                                    flp_decision_value d2)
{
    /* Agreement holds if either is undecided, or both are the same */
    if (d1 == FLP_UNDECIDED || d2 == FLP_UNDECIDED) return true;
    return d1 == d2;
}

bool flp_consensus_validity_holds(const flp_configuration *cfg,
                                   flp_decision_value decision)
{
    if (cfg == NULL) return false;
    if (decision == FLP_UNDECIDED) return true;

    int32_t expected = (decision == FLP_DECIDE_0) ? 0 : 1;
    for (int32_t i = 0; i < cfg->num_processes; i++) {
        if (cfg->processes[i].input_value == expected) return true;
    }
    return false;
}

bool flp_consensus_config_is_consistent(const flp_configuration *cfg)
{
    if (cfg == NULL) return false;

    flp_decision_value seen_decision = FLP_UNDECIDED;
    for (int32_t i = 0; i < cfg->num_processes; i++) {
        if (!flp_process_is_correct(&cfg->processes[i])) continue;
        if (!cfg->processes[i].has_decided) continue;

        flp_decision_value d = cfg->processes[i].decision;
        if (seen_decision == FLP_UNDECIDED) {
            seen_decision = d;
        } else if (d != seen_decision) {
            /* Agreement violation: two different decisions */
            return false;
        }

        /* Check validity */
        if (!flp_consensus_validity_holds(cfg, d)) return false;
    }
    return true;
}

/* ===== Consensus Variants ===== */

const char *flp_consensus_variant_name(flp_consensus_variant v)
{
    switch (v) {
        case FLP_CONSENSUS_DETERMINISTIC:    return "Deterministic (FLP)";
        case FLP_CONSENSUS_RANDOMIZED:       return "Randomized";
        case FLP_CONSENSUS_FAILURE_DETECTOR: return "Failure Detector";
        case FLP_CONSENSUS_SYNCHRONOUS:      return "Synchronous";
        case FLP_CONSENSUS_K_SET:            return "k-Set Agreement";
        default:                             return "Unknown";
    }
}

/* ===== FLP Theorem Analysis ===== */

/**
 * Internal: check if a protocol has a bivalent initial configuration
 * by exploring the 2^N possible input vectors.
 */
static bool has_bivalent_initial_internal(const flp_protocol_desc *desc,
                                           int32_t n, int32_t max_depth,
                                           flp_configuration *witness)
{
    if (desc == NULL || n < 2 || n > 6) return false;
    /* Limit to n <= 6 to keep 2^N manageable (max 64 configs) */

    int32_t num_configs = 1 << n;
    for (int32_t mask = 0; mask < num_configs; mask++) {
        int32_t inputs[FLP_MAX_PROCESSES];
        for (int32_t i = 0; i < n; i++) {
            inputs[i] = (mask >> i) & 1;
        }

        flp_configuration cfg;
        flp_config_init_with_inputs(&cfg, n, inputs, mask);

        bool can_0 = flp_config_can_decide_0(&cfg, max_depth);
        bool can_1 = flp_config_can_decide_1(&cfg, max_depth);

        if (can_0 && can_1) {
            if (witness != NULL) {
                flp_config_clone(witness, &cfg);
            }
            return true;
        }
    }
    return false;
}

void flp_analyze_protocol(const flp_protocol_desc *desc, int32_t n,
                           int32_t max_depth, flp_theorem_result *result)
{
    if (desc == NULL || result == NULL) return;
    memset(result, 0, sizeof(*result));

    result->explanation = "Analysis not performed";
    result->impossible = false;

    /* Step 1: Check for bivalent initial configuration (Lemma 2) */
    flp_configuration witness;
    memset(&witness, 0, sizeof(witness));
    result->has_bivalent_init = has_bivalent_initial_internal(
        desc, n, max_depth, &witness);

    if (result->has_bivalent_init) {
        result->witness_config = witness;
        result->bivalent_count = 1;

        /* Step 2: Try to construct non-deciding run (Lemma 3) */
        flp_schedule sched;
        flp_schedule_init(&sched);
        int32_t steps = flp_construct_infinite_run(desc, n, 20, max_depth,
                                                    &sched);
        if (steps > 0) {
            result->explanation = "FLP impossibility applies: found bivalent "
                "initial configuration and constructed non-deciding run.";
            result->impossible = true;
            result->infinite_prefix = sched;
            result->max_nondeciding = steps;
        } else {
            result->explanation = "Bivalent initial config found but could "
                "not construct non-deciding run within depth bound.";
            result->impossible = false;
        }
    } else {
        result->explanation = "No bivalent initial configuration found. "
            "Protocol might solve consensus (or search depth insufficient).";
        result->impossible = false;
    }
}

void flp_theorem_result_print(const flp_theorem_result *result)
{
    if (result == NULL) {
        printf("FLP Theorem Result: NULL\n");
        return;
    }

    printf("========================================\n");
    printf("FLP Impossibility Theorem Analysis\n");
    printf("========================================\n");
    printf("Consensus impossible: %s\n",
           result->impossible ? "YES (FLP applies)" : "NO (or undetermined)");
    printf("Bivalent initial config: %s\n",
           result->has_bivalent_init ? "FOUND" : "NOT FOUND");
    printf("Bivalent configs found: %d\n", result->bivalent_count);
    printf("Max non-deciding steps: %d\n", result->max_nondeciding);
    printf("Explanation: %s\n", result->explanation);

    if (result->has_bivalent_init) {
        printf("\nWitness bivalent configuration:\n");
        flp_config_print(&result->witness_config);
    }

    if (result->max_nondeciding > 0) {
        printf("\nNon-deciding schedule prefix (%d steps):\n",
               result->infinite_prefix.length);
        for (int32_t i = 0; i < result->infinite_prefix.length; i++) {
            printf("  Step %d: process P%d\n",
                   i, result->infinite_prefix.schedule[i]);
        }
    }
    printf("========================================\n");
}

/* ===== Canonical Consensus Instances ===== */

void flp_consensus_all_zero_config(flp_configuration *cfg, int32_t n)
{
    int32_t inputs[FLP_MAX_PROCESSES];
    for (int32_t i = 0; i < n && i < FLP_MAX_PROCESSES; i++) inputs[i] = 0;
    flp_config_init_with_inputs(cfg, n, inputs, 0);
}

void flp_consensus_all_one_config(flp_configuration *cfg, int32_t n)
{
    int32_t inputs[FLP_MAX_PROCESSES];
    for (int32_t i = 0; i < n && i < FLP_MAX_PROCESSES; i++) inputs[i] = 1;
    flp_config_init_with_inputs(cfg, n, inputs, 1);
}

void flp_consensus_split_config(flp_configuration *cfg, int32_t n)
{
    int32_t inputs[FLP_MAX_PROCESSES];
    for (int32_t i = 0; i < n && i < FLP_MAX_PROCESSES; i++) inputs[i] = i % 2;
    flp_config_init_with_inputs(cfg, n, inputs, 0);
}

void flp_consensus_custom_config(flp_configuration *cfg, int32_t n,
                                  const int32_t *inputs)
{
    flp_config_init_with_inputs(cfg, n, inputs, 99);
}

int32_t flp_consensus_count_bivalent_initial(const flp_protocol_desc *desc,
                                              int32_t n, int32_t max_depth)
{
    if (desc == NULL || n < 2 || n > 6) return 0;
    int32_t count = 0;
    int32_t num_configs = 1 << n;

    for (int32_t mask = 0; mask < num_configs; mask++) {
        int32_t inputs[FLP_MAX_PROCESSES];
        for (int32_t i = 0; i < n; i++) {
            inputs[i] = (mask >> i) & 1;
        }

        flp_configuration cfg;
        flp_config_init_with_inputs(&cfg, n, inputs, mask);

        bool can_0 = flp_config_can_decide_0(&cfg, max_depth);
        bool can_1 = flp_config_can_decide_1(&cfg, max_depth);
        if (can_0 && can_1) count++;
    }
    return count;
}

void flp_consensus_classify_initial_configs(const flp_protocol_desc *desc,
                                             int32_t n, int32_t max_depth,
                                             int32_t *count_0,
                                             int32_t *count_1,
                                             int32_t *count_biv)
{
    if (count_0) *count_0 = 0;
    if (count_1) *count_1 = 0;
    if (count_biv) *count_biv = 0;
    if (desc == NULL || n < 2 || n > 6) return;

    int32_t num_configs = 1 << n;
    for (int32_t mask = 0; mask < num_configs; mask++) {
        int32_t inputs[FLP_MAX_PROCESSES];
        for (int32_t i = 0; i < n; i++) {
            inputs[i] = (mask >> i) & 1;
        }

        flp_configuration cfg;
        flp_config_init_with_inputs(&cfg, n, inputs, mask);

        bool can_0 = flp_config_can_decide_0(&cfg, max_depth);
        bool can_1 = flp_config_can_decide_1(&cfg, max_depth);

        if (can_0 && can_1) {
            if (count_biv) (*count_biv)++;
        } else if (can_0) {
            if (count_0) (*count_0)++;
        } else if (can_1) {
            if (count_1) (*count_1)++;
        }
    }
}