/*
 * coloring.h -- Distributed Graph Coloring Algorithms
 *
 * L1 Definitions:
 *   - Proper vertex coloring: c: V -> {1,...,k} such that (u,v) in E => c(u) != c(v)
 *   - Chromatic number chi(G): min k for a proper k-coloring
 *   - (Delta+1)-coloring: coloring using at most Delta+1 colors
 *   - Greedy coloring: sequential coloring using smallest available color
 *
 * L2 Core Concepts:
 *   - Randomized coloring: each node picks random color from palette
 *   - Color reduction: reducing number of colors round by round
 *   - Palette reduction: each node maintains set of available colors
 *   - Conflict resolution: handling color collisions with neighbors
 *
 * L4 Fundamental Laws:
 *   - Linial lower bound: Omega(log* n) rounds to 3-color an n-cycle
 *   - Naor & Stockmeyer: locality of weak coloring
 *
 * L5 Algorithms/Methods:
 *   - Cole-Vishkin: deterministic (Delta+1)-coloring in O(Delta + log* n)
 *   - Linial O(Delta^2)-coloring in O(log* n) rounds
 *   - Randomized (Delta+1)-coloring in O(log n) rounds
 *   - Greedy sequential coloring (centralized baseline)
 *
 * L6 Canonical Problems:
 *   - 3-coloring of a ring (cycle graph)
 *   - (Delta+1)-coloring of general graphs
 *
 * Reference: Barenboim & Elkin (2013) Distributed Graph Coloring
 *            Linial (1992) Locality in Distributed Graph Algorithms
 *            Cole & Vishkin (1986) Deterministic Coin Tossing
 * Courses: ETH 263-4650, MIT 6.852, CMU 15-859, Princeton COS 522
 */

#ifndef COLORING_H
#define COLORING_H

#include "graph.h"
#include "local_model.h"

/* === Greedy Sequential Coloring (Centralized Baseline) === */

/* Greedy coloring: process vertices in given order, assign each vertex
 * the smallest color not used by already-colored neighbors.
 * Always uses <= Delta+1 colors.
 * Complexity: O(n + m) time on RAM, not a distributed algorithm. */
Coloring* coloring_greedy_sequential(const Graph* g, const int* order);

/* Smallest-Last ordering: repeatedly remove min-degree vertex,
 * then reverse the removal order. Good color count on many graphs. */
int* coloring_smallest_last_order(const Graph* g);

/* Largest-First ordering: sort by degree descending.
 * Simple heuristic that often uses fewer colors. */
int* coloring_largest_first_order(const Graph* g);

/* === Linial Algorithm: O(Delta^2)-coloring in O(log* n) === */

/* Linial one-round color reduction: given coloring with K colors,
 * produce new coloring with at most f(K) colors in 1 round,
 * where f(K) = smallest t such that t * C(t, Delta) >= K.
 * Key insight: each node exchanges palette, shrinks it using
 * a graph coloring of the palette graph. */
Coloring* linial_one_round_reduction(const Graph* g, const Coloring* input,
                                      int max_degree);

/* Run Linial full algorithm: reduce from n colors to O(Delta^2)
 * in O(log* n) rounds. Returns intermediate trace. */
ExecutionTrace* linial_full_algorithm(const Graph* g);

/* === Cole-Vishkin Algorithm: (Delta+1)-coloring in O(Delta+log*n) === */

/* Cole-Vishkin extends Linial: first reduce to O(Delta^2) colors
 * via O(log* n) rounds, then O(Delta) rounds to Delta+1.
 * Phase 1: O(log* n) rounds -> O(Delta^2) coloring.
 * Phase 2: O(Delta) rounds -> Delta+1, eliminating one color per round. */
Coloring* cole_vishkin_reduce(const Graph* g, const Coloring* input,
                               int target_colors);

/* Full Cole-Vishkin (Delta+1)-coloring */
Coloring* cole_vishkin_coloring(const Graph* g);

/* === Randomized Coloring Algorithms === */

/* Randomized (Delta+1)-coloring: each uncolored node picks random
 * color from {1,...,Delta+1}. If no neighbor picks same color,
 * node keeps it permanently. Expected O(log n) rounds. */
Coloring* randomized_delta_plus_one_coloring(const Graph* g, uint64_t seed);

/* One round of random color trial. Returns count of newly colored vertices. */
int randomized_color_trial(const Graph* g, Coloring* current,
                            uint64_t* seed, bool* newly_colored);

/* Luby-style randomized coloring via independent sets */
Coloring* luby_style_coloring(const Graph* g, uint64_t seed);

/* === Color Reduction Operations === */

/* Reduce K-coloring to (K-1)-coloring by eliminating one color class
 * and recoloring those vertices. Used iteratively: O(Delta^2) -> Delta+1. */
bool coloring_reduce_by_one(const Graph* g, Coloring* c, int color_to_remove);

/* Palette-based coloring: each node maintains set of allowed colors.
 * Each round, nodes exchange palettes and eliminate colors neighbors chose. */
void coloring_palette_init(LocalConfig* cfg, int initial_colors);
void coloring_palette_update(const Graph* g, LocalNodeState** nodes,
                              int n, int round);

/* === 3-Coloring of a Ring (Canonical) === */

/* Cole-Vishkin 3-coloring of a cycle: O(log* n) rounds.
 * Encode each node ID as bitstring. Each round: compare own bitstring
 * with neighbors, find first differing bit, output (bit_index, own_bit).
 * Reduces color space from n to 2*log n. Iterate to 6 colors, then finalize. */
Coloring* ring_3coloring_cole_vishkin(int n, const int* ids);

/* Ring 3-coloring lower bound: construct worst-case ID assignment
 * forcing >= t rounds when n > 2^t (demonstrates Linial Omega(log* n)). */
int* ring_3coloring_lower_bound_ids(int n, int target_rounds);

/* === Distributed Greedy Coloring === */

/* Each node waits until neighbors with smaller IDs are colored,
 * then picks min available color. O(Delta + log* n) rounds. */
Coloring* distributed_greedy_coloring(const Graph* g, const LocalConfig* cfg);

/* === Planar Graph 6-Coloring === */

/* Every planar graph has vertex of degree <= 5.
 * Distributed version: O(log n) rounds via network decomposition. */
Coloring* planar_6coloring(const Graph* g);

/* Simple planarity test (Euler formula + small-graph checks) */
bool graph_is_planar(const Graph* g);

/* === Color Statistics === */

int coloring_count_conflicts(const Graph* g, const Coloring* c);
void coloring_histogram(const Coloring* c, int* hist, int max_colors);
int coloring_max_conflict_vertex(const Graph* g, const Coloring* c);

/* === Lower Bound Support for Coloring === */

/* Neighborhood graph for Linial lower bound: H where
 * nodes = t-neighborhood views, edges connect adjacent views.
 * A proper coloring of H corresponds to t-round coloring for G. */
NeighborhoodGraph* coloring_neighborhood_graph(const Graph* g, int t);

/* Lower bound witness graph: any t-round k-coloring algorithm fails. */
Graph* coloring_lower_bound_graph(int n, int k, int t);

#endif /* COLORING_H */
