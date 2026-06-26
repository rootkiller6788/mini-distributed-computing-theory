/**
 * @file algorithms.h
 * @brief Core Algorithms for the LOCAL Model
 *
 * This file declares the fundamental distributed algorithms that operate
 * in the LOCAL model. Each algorithm is characterized by:
 *  - Problem solved
 *  - Round complexity (deterministic and/or randomized)
 *  - Key technique employed
 *
 * Algorithms covered (L5):
 *  - Cole-Vishkin coloring reduction (O(log* n) for coloring)
 *  - Linial's n-coloring reduction (iterated log)
 *  - Luby's randomized MIS (O(log n) rounds)
 *  - Barenboim-Elkin deterministic MIS (2^O(√(log n)) rounds)
 *  - Naor-Stockmeyer weak coloring (O(log* n) rounds)
 *  - Panconesi-Rizzi edge coloring (O(log n) rounds)
 *  - Sinkless orientation algorithms
 *  - Tree-based convergecast/broadcast (O(diameter) rounds)
 *  - Flooding / BFS tree construction (O(diameter) rounds)
 *  - Leader election (O(diameter) deterministic, O(log n) randomized)
 *  - Triangle detection / listing
 *
 * References:
 *  - Cole & Vishkin (1986). "Deterministic coin tossing with applications to
 *    optimal parallel list ranking."
 *  - Linial (1992). "Locality in distributed graph algorithms."
 *  - Luby (1986). "A simple parallel algorithm for the maximal independent set
 *    problem."
 *  - Barenboim & Elkin (2013). "Distributed Graph Coloring."
 *  - Suomela (2013). "Survey of local algorithms."
 *
 * Level: L5 (Algorithms/Methods)
 */

#ifndef ALGORITHMS_H
#define ALGORITHMS_H

#include "local_model.h"
#include "port_numbering.h"
#include "graph_problems.h"
#include <stdint.h>
#include <stdbool.h>

/* ================================================================
 * L5-1: Cole-Vishkin Coloring Reduction
 * ================================================================ */

/**
 * @brief Compute the iterated logarithm of n: log* n.
 *
 * log* n is the number of times the logarithm function must be applied
 * before the result is ≤ 1. This function appears in the round complexity
 * of many LOCAL algorithms.
 *
 * @param n     Positive integer.
 * @return      log* n (minimum t s.t. log^{(t)}(n) ≤ 1, using base 2).
 *
 * Theorem: In the LOCAL model, log* n is the threshold separating
 * constant-time and non-constant-time determinism for several problems.
 */
uint32_t iterated_log2(uint64_t n);

/**
 * @brief The Cole-Vishkin coloring reduction round.
 *
 * Given a k-coloring (each node has a color in [0, k-1]), produce a
 * k'-coloring with k' = O(log k). This is the core iterative step that
 * reduces the number of colors from n to O(1) in O(log* n) rounds.
 *
 * Each node looks at its own color c and its successor's color d (in the
 * ring orientation) and computes a new color:
 *   - Find the least significant bit where c and d differ: index i
 *   - Set new_color = 2*i + bit_i(c)
 *
 * @param ctx           LOCAL context.
 * @param old_colors    Input coloring (size n).
 * @param new_colors    Output coloring (size n), smaller range.
 * @param pns           Port numberings (must define successor).
 * @param successor_port Port number pointing to the successor.
 * @return              Number of bits needed to represent new_colors.
 *
 * Complexity: 1 round per reduction step.
 */
uint32_t cv_reduce_colors(const local_context_t *ctx, const uint32_t *old_colors,
                          uint32_t *new_colors, const port_numbering_t *pns,
                          uint32_t successor_port);

/**
 * @brief Full Cole-Vishkin: reduce from initial n-coloring to 6-coloring
 *        in O(log* n) rounds.
 *
 * This is a pedagogical decomposition of the algorithm: it applies
 * cv_reduce_colors() iteratively until reaching ≤ 6 colors.
 *
 * @param ctx           LOCAL context (must be a cycle/oriented path).
 * @param colors        Input: UIDs as initial n-coloring.
 *                      Output: 6-coloring.
 * @param pns           Port numberings.
 * @param successor_port Which port points to the successor.
 * @return              Number of rounds executed (= O(log* n)).
 *
 * Reference: Cole & Vishkin (1986); Linial (1992, Section 3.2).
 */
uint32_t cv_full_reduction(local_context_t *ctx, uint32_t *colors,
                           const port_numbering_t *pns, uint32_t successor_port);

/* ================================================================
 * L5-2: Linial's Coloring Algorithm
 * ================================================================ */

/**
 * @brief Linial's single-round color reduction.
 *
 * Given a graph with maximum degree Δ, and nodes initially colored with
 * a palette of size C, produce a new coloring with palette size
 * f(C, Δ) which is substantially smaller.
 *
 * Theorem (Linial, 1992): In a single round of the LOCAL model, one can
 * reduce the number of colors from C to O(Δ^2 log C).
 *
 * @param ctx           LOCAL context.
 * @param old_colors    Input coloring.
 * @param new_colors    Output coloring (reduced palette).
 * @param delta         Maximum degree.
 * @return              New palette size.
 */
uint32_t linial_color_reduce(const local_context_t *ctx, const uint32_t *old_colors,
                             uint32_t *new_colors, uint32_t delta);

/**
 * @brief Linial's (Δ+1)-coloring algorithm.
 *
 * Iteratively applies color reduction until reaching a (Δ+1)-coloring.
 * The total number of rounds is O(Δ^2 + log* n).
 *
 * @param ctx       LOCAL context.
 * @param colors    Output: (Δ+1)-coloring.
 * @param pns       Port numberings.
 * @return          Number of rounds executed.
 */
uint32_t linial_delta_plus_one_coloring(local_context_t *ctx, uint32_t *colors,
                                        const port_numbering_t *pns);

/* ================================================================
 * L5-3: Luby's MIS — Randomized
 * ================================================================ */

/**
 * @brief Execute the full Luby MIS algorithm.
 *
 * Repeats mis_luby_round() until all nodes are decided. Expected
 * O(log n) rounds.
 *
 * @param ctx       LOCAL context.
 * @param states    MIS node states (will be filled with result).
 * @param seed      Random seed.
 * @return          Number of rounds executed.
 */
uint32_t luby_mis_full(local_context_t *ctx, mis_node_state_t *states, uint64_t seed);

/**
 * @brief A single "MIS via random permutation" round.
 *
 * Alternative to Luby: assign random priorities to nodes, then nodes
 * with locally maximum priority join the MIS.
 *
 * @param ctx       LOCAL context.
 * @param states    MIS node states.
 * @param seed      Random seed for this round.
 * @return          Number of new decisions.
 */
uint32_t mis_random_priority_round(local_context_t *ctx, mis_node_state_t *states,
                                   uint64_t seed);

/* ================================================================
 * L5-4: BFS Tree Construction (Flooding)
 * ================================================================ */

/**
 * @brief Build a BFS tree rooted at a given node.
 *
 * In the LOCAL model, BFS tree construction takes O(diameter) rounds.
 * Each node learns its distance from the root and its parent port.
 *
 * @param ctx       LOCAL context.
 * @param root      Root node index.
 * @param parent    Output: parent[i] = parent of node i (parent[root] = root).
 * @param distance  Output: distance[i] = hop distance from root.
 * @return          Number of rounds executed (= diameter).
 */
uint32_t bfs_tree_build(local_context_t *ctx, uint32_t root,
                        uint32_t *parent, uint32_t *distance);

/**
 * @brief Broadcast a value from root to all nodes via the BFS tree.
 *
 * Each round, nodes that have received the value forward it to all children.
 *
 * @param ctx       LOCAL context.
 * @param parent    Parent array from bfs_tree_build.
 * @param value     Value to broadcast.
 * @param values    Output: values[i] = received value.
 * @return          Number of rounds (= tree height).
 */
uint32_t bfs_broadcast(local_context_t *ctx, const uint32_t *parent,
                       uint32_t value, uint32_t *values);

/**
 * @brief Convergecast: aggregate values from leaves to root via the BFS tree.
 *
 * Each node sends the sum (or other associative op) of its subtree to its parent.
 *
 * @param ctx       LOCAL context.
 * @param parent    Parent array.
 * @param values    Input: per-node values. Output: aggregated at root.
 * @param agg_fn    Aggregation function (e.g., sum, max, min).
 * @return          Number of rounds (= tree height).
 */
typedef uint32_t (*agg_fn_t)(uint32_t a, uint32_t b);
uint32_t bfs_convergecast(local_context_t *ctx, const uint32_t *parent,
                          uint32_t *values, agg_fn_t agg_fn);

/* ================================================================
 * L5-5: Leader Election
 * ================================================================ */

/**
 * @brief Deterministic leader election via flooding and comparison.
 *
 * In the LOCAL model with unique UIDs, leader election can be done in
 * O(diameter) rounds: flood all UIDs, the node with the smallest UID
 * becomes the leader.
 *
 * @param ctx       LOCAL context.
 * @param leader    Output: leader UID, or 0 on error.
 * @return          Number of rounds executed.
 */
uint32_t leader_election_deterministic(local_context_t *ctx, uint32_t *leader);

/**
 * @brief Randomized leader election.
 *
 * Based on the algorithm of Kutten et al. (2015): O(log n) rounds
 * with high probability, using randomized exponential backoff.
 *
 * @param ctx       LOCAL context.
 * @param leader    Output: leader UID.
 * @param seed      Random seed.
 * @return          Number of rounds executed.
 */
uint32_t leader_election_randomized(local_context_t *ctx, uint32_t *leader, uint64_t seed);

/* ================================================================
 * L5-6: Triangle Detection
 * ================================================================ */

/**
 * @brief Detect whether the graph contains a triangle (3-cycle).
 *
 * In the LOCAL model with bandwidth constraint, triangle detection
 * requires Ω(n^{1/3}) rounds. Without bandwidth constraint (classic LOCAL),
 * one round suffices: each node sends its neighbor list to all neighbors.
 *
 * @param ctx           LOCAL context.
 * @param has_triangle  Output: true iff at least one triangle exists.
 * @return              Number of rounds.
 */
uint32_t triangle_detect(local_context_t *ctx, bool *has_triangle);

/**
 * @brief Count all triangles in the graph.
 *
 * @param ctx   LOCAL context.
 * @return      Number of triangles.
 */
uint32_t triangle_count(const local_context_t *ctx);

/* ================================================================
 * L5-7: Distributed Sorting (on a ring)
 * ================================================================ */

/**
 * @brief Sort n values on a ring network.
 *
 * In the LOCAL model, sorting on a ring takes O(n) rounds (worst-case)
 * because each value may need to travel half the ring.
 *
 * @param ctx       LOCAL context (must be a ring).
 * @param values    Input: per-node values. Output: sorted values
 *                  (node 0 has smallest, node 1 second smallest, etc.).
 * @param pns       Port numberings (ring orientation).
 * @return          Number of rounds.
 */
uint32_t ring_sort(local_context_t *ctx, uint32_t *values, const port_numbering_t *pns);

/* ================================================================
 * L5-8: Distributed Sum / Count
 * ================================================================ */

/**
 * @brief Compute the sum of all node values via pipelined BFS convergecast.
 *
 * Optimized version for sums: O(diameter) rounds, O(1) messages per edge
 * per round.
 *
 * @param ctx       LOCAL context.
 * @param values    Per-node values.
 * @param sum       Output: total sum.
 * @return          Number of rounds.
 */
uint32_t distributed_sum(local_context_t *ctx, const uint32_t *values, uint64_t *sum);

/**
 * @brief Compute the maximum value across all nodes.
 *
 * @param ctx       LOCAL context.
 * @param values    Per-node values.
 * @param max_val   Output: maximum value.
 * @return          Number of rounds.
 */
uint32_t distributed_max(local_context_t *ctx, const uint32_t *values, uint32_t *max_val);

/* ================================================================
 * L5-9: Distance-k Coloring
 * ================================================================ */

/**
 * @brief Compute a distance-2 coloring.
 *
 * A distance-2 coloring assigns colors such that no two nodes at distance
 * ≤ 2 share the same color. This is equivalent to coloring the square graph
 * G^2. The palette size needed is at most Δ^2 + 1.
 *
 * @param ctx       LOCAL context.
 * @param colors    Output: distance-2 coloring.
 * @return          Number of rounds.
 */
uint32_t distance2_coloring(local_context_t *ctx, uint32_t *colors);

/* ================================================================
 * L5-10: Maximal Matching via Random Priorities
 * ================================================================ */

/**
 * @brief Full maximal matching via repeated random priority rounds.
 *
 * @param ctx       LOCAL context.
 * @param states    Matching node states.
 * @param pns       Port numberings.
 * @param seed      Random seed.
 * @return          Number of rounds.
 */
uint32_t mm_full_random(local_context_t *ctx, mm_node_state_t *states,
                        const port_numbering_t *pns, uint64_t seed);

#endif /* ALGORITHMS_H */
