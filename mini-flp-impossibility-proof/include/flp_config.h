/**
 * @file flp_config.h
 * @brief Configuration management for the FLP impossibility proof.
 *
 * A configuration C = (s[0],...,s[N-1], M) is the global system state.
 * This module provides creation, cloning, comparison, serialization,
 * and valence classification of configurations.
 *
 * Knowledge Coverage:
 *   L1: Configuration definition (Def 1, FLP 1985)
 *   L2: Reachability relation between configurations
 *   L3: Configuration graph as a state transition system
 *   L4: Initial bivalence lemma (Lemma 2)
 *
 * Course Alignment: MIT 6.852 Lec 5-7, CMU 15-712 Lec 8
 */

#ifndef FLP_CONFIG_H
#define FLP_CONFIG_H

#include "flp_common.h"

/* ---------------------------------------------------------------------------
 * L1: Configuration Lifecycle
 * ---------------------------------------------------------------------------
 * A configuration represents an instantaneous snapshot of the entire
 * distributed system. The key operations are creation (initial configs),
 * transition (apply event), and analysis (valence, decisions).
 */

/**
 * Initialize an empty configuration descriptor with N processes.
 * All processes start with FLP_UNDECIDED status.
 * @param cfg       Pointer to configuration to initialize.
 * @param n         Number of processes (1..FLP_MAX_PROCESSES).
 * @param config_id Unique numeric identifier for this configuration.
 */
void flp_config_init(flp_configuration *cfg, int32_t n, int32_t config_id);

/**
 * Initialize a configuration with specific input values for each process.
 * This creates an initial configuration of the FLP model.
 * @param cfg    Configuration to initialize.
 * @param n      Number of processes.
 * @param inputs Array of n input values, each 0 or 1.
 * @param cid    Configuration ID.
 */
void flp_config_init_with_inputs(flp_configuration *cfg, int32_t n,
                                  const int32_t *inputs, int32_t cid);

/**
 * Deep-copy a configuration. All process states and the message
 * buffer multiset are duplicated.
 * @param dst  Destination configuration.
 * @param src  Source configuration.
 */
void flp_config_clone(flp_configuration *dst, const flp_configuration *src);

/**
 * Compare two configurations for exact equality.
 * Two configurations are equal iff: same N, identical process states
 * for all processes, and identical message buffer multisets.
 * @param a  First configuration.
 * @param b  Second configuration.
 * @return   true if configurations are identical.
 */
bool flp_config_equal(const flp_configuration *a, const flp_configuration *b);

/**
 * Compute a hash value for a configuration, useful for visited-set
 * in configuration graph exploration.
 * @param cfg  Configuration.
 * @return     32-bit hash of the configuration.
 */
uint32_t flp_config_hash(const flp_configuration *cfg);

/**
 * Reset the valence cache of a configuration to FLP_VALENCE_UNKNOWN.
 * Call this after applying an event that may change the valence.
 */
void flp_config_invalidate_valence(flp_configuration *cfg);

/* ---------------------------------------------------------------------------
 * L2: Reachability
 * ---------------------------------------------------------------------------
 * Configuration C2 is reachable from C1 if there exists a finite
 * schedule sigma such that sigma(C1) = C2.
 */

/**
 * Check if a configuration is an initial configuration:
 * empty message buffer and all processes in initial state.
 * @param cfg  Configuration.
 * @return     true if this is an initial configuration.
 */
bool flp_config_is_initial(const flp_configuration *cfg);

/**
 * Check if a configuration is a deciding configuration:
 * at least one correct process has output a decision value.
 * Note: in a correct consensus protocol, once any process decides,
 * all correct processes eventually decide the same value.
 * @param cfg  Configuration.
 * @return     true if some correct process has decided.
 */
bool flp_config_is_decided(const flp_configuration *cfg);

/**
 * Get the common decision value from a decided configuration.
 * Only valid if flp_config_is_decided() returns true.
 * @param cfg  Configuration.
 * @return     The decision value (0 or 1).
 */
flp_decision_value flp_config_decision(const flp_configuration *cfg);

/**
 * Count the number of distinct reachable configurations within
 * a given depth bound via BFS enumeration.
 * @param start      Starting configuration.
 * @param max_depth  Maximum exploration depth.
 * @return           Number of configurations reachable within max_depth.
 */
int32_t flp_config_reachable_count(const flp_configuration *start,
                                    int32_t max_depth);

/* ---------------------------------------------------------------------------
 * L4: Valence Analysis
 * ---------------------------------------------------------------------------
 * Valence is the central concept of the FLP proof.
 * A configuration is:
 *   - 0-valent if ALL deciding runs from it decide 0
 *   - 1-valent if ALL deciding runs from it decide 1
 *   - bivalent if BOTH 0 and 1 are reachable
 */

/**
 * Classify the valence of a configuration by exploring all reachable
 * deciding configurations up to max_depth.
 * @param cfg        Configuration to analyze.
 * @param max_depth  Bound on exploration depth.
 * @return           FLP_VALENCE_0, FLP_VALENCE_1, or FLP_VALENCE_BIVALENT.
 */
flp_valence flp_config_classify_valence(flp_configuration *cfg,
                                         int32_t max_depth);

/**
 * Check whether a 0-deciding configuration is reachable from cfg.
 * @return true if some reachable configuration decides 0.
 */
bool flp_config_can_decide_0(flp_configuration *cfg, int32_t max_depth);

/**
 * Check whether a 1-deciding configuration is reachable from cfg.
 * @return true if some reachable configuration decides 1.
 */
bool flp_config_can_decide_1(flp_configuration *cfg, int32_t max_depth);

/**
 * Find a concrete schedule that leads from cfg to a 0-decision.
 * Returns schedule length, or -1 if unreachable within bound.
 */
int32_t flp_config_schedule_to_0(const flp_configuration *cfg,
                                  flp_schedule *out, int32_t max_depth);

/**
 * Find a concrete schedule that leads from cfg to a 1-decision.
 * Returns schedule length, or -1 if unreachable within bound.
 */
int32_t flp_config_schedule_to_1(const flp_configuration *cfg,
                                  flp_schedule *out, int32_t max_depth);

/* ---------------------------------------------------------------------------
 * Serialization and Debugging
 * ---------------------------------------------------------------------------
 * Human-readable output for debugging and demonstration.
 */

/**
 * Print a human-readable representation of a configuration to stdout.
 */
void flp_config_print(const flp_configuration *cfg);

/**
 * Format a configuration as a string into a user-provided buffer.
 * @return Number of bytes written (excluding null terminator).
 */
int32_t flp_config_to_string(const flp_configuration *cfg,
                              char *buf, int32_t bufsize);

#endif /* FLP_CONFIG_H */
