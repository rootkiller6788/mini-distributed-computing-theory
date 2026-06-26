/**
 * @file lower_bounds.h
 * @brief Lower Bounds in the LOCAL Model
 *
 * This file covers the fundamental lower bound techniques for the LOCAL
 * model of distributed computing. Lower bounds tell us what cannot be
 * computed within T rounds, establishing the inherent limitations of
 * locality.
 *
 * Key lower bound theorems covered (L4):
 *  1. Linial's lower bound: 3-coloring a ring requires Ω(log* n) rounds.
 *  2. Naor's lower bound: MIS requires Ω(log* n) rounds deterministically.
 *  3. Kuhn-Moscibroda-Wattenhofer: MIS lower bound Ω(min{log Δ, √(log n)}).
 *  4. Lower bound via neighborhood indistinguishability.
 *  5. Round elimination technique.
 *  6. Speedup simulation: if a problem is solvable in T rounds, the
 *     "speedup" version is solvable in T-1 rounds (contrapositive for lower bounds).
 *  7. Ramsey-theoretic lower bounds for graph coloring.
 *  8. Lower bounds for sinkless orientation: Ω(log log n) randomized.
 *
 * References:
 *  - Linial (1992). "Locality in distributed graph algorithms." SIAM J. Comput.
 *  - Naor & Stockmeyer (1995). "What can be computed locally?" SIAM J. Comput.
 *  - Kuhn, Moscibroda, Wattenhofer (2004). "What cannot be computed locally!"
 *  - Brandt et al. (2016). "A lower bound for the distributed Lovasz local lemma."
 *  - Suomela (2013). "Survey of local algorithms."
 *
 * Level: L4 (Fundamental Laws/Theorems), L3 (Mathematical Structures)
 */

#ifndef LOWER_BOUNDS_H
#define LOWER_BOUNDS_H

#include "local_model.h"
#include "port_numbering.h"
#include <stdint.h>
#include <stdbool.h>

/* ================================================================
 * L4-1: Linial's Ω(log* n) Lower Bound for 3-Coloring a Ring
 * ================================================================ */

/**
 * @brief Verify Linial's lower bound construction.
 *
 * Linial proved that no deterministic LOCAL algorithm can 3-color a ring
 * in o(log* n) rounds. The proof proceeds by showing that any T-round
 * algorithm that produces a c-coloring can be converted into a (T-1)-round
 * algorithm that produces a f(c)-coloring (where f is a rapidly growing
 * function). Iterating this, if T is too small, we'd need the initial
 * round-0 view to already be a perfectly valid coloring with too many colors
 * — contradiction.
 *
 * This function checks the key inequality:
 *   c_{t-1} ≥ 2^{c_t - 2}
 * where c_t is the number of colors achievable in t rounds.
 *
 * @param rounds            Number of rounds T.
 * @param initial_colors    Number of distinct UID colors (n).
 * @return                  true iff the inequality chain shows impossibility
 *                          of 3-coloring in T rounds, meaning T < log* n - O(1).
 */
bool lb_linial_ring_impossible(uint32_t rounds, uint32_t initial_colors);

/**
 * @brief Compute the exact log* lower bound for c-coloring a ring.
 *
 * Given: c colors desired, n nodes (with n distinct UIDs as initial colors),
 * returns the minimum number of rounds required deterministically.
 *
 * @param n             Number of nodes.
 * @param target_colors Desired number of colors c.
 * @return              Minimum rounds required (lower bound).
 *
 * Corollary: For c = 3, the bound is Ω(log* n). For c = O(1), it remains Ω(log* n).
 */
uint32_t lb_linial_ring_min_rounds(uint32_t n, uint32_t target_colors);

/**
 * @brief Simulate Linial's round elimination:
 *        Given a T-round algorithm producing a c-coloring of a ring,
 *        produce the equivalent (T-1)-round neighborhood coloring function.
 *
 * This is a constructive version of the lower bound argument. It takes
 * the output of a hypothetical T-round algorithm and builds a labeling
 * of the (T-1)-neighborhoods that acts as a coloring.
 *
 * @param ctx               LOCAL context (must be a ring).
 * @param colors_after_T    Coloring produced after T rounds.
 * @param T                 Number of rounds used.
 * @param colors_after_Tm1  Output: "coloring" that a (T-1)-round algorithm
 *                          would need to produce.
 * @param pns               Port numberings.
 * @return                  Number of distinct labels in colors_after_Tm1.
 *
 * Complexity: O(n * d^T) to enumerate all T-neighborhood colorings.
 */
uint32_t lb_round_elimination_step(const local_context_t *ctx, const uint32_t *colors_after_T,
                                   uint32_t T, uint32_t *colors_after_Tm1,
                                   const port_numbering_t *pns);

/* ================================================================
 * L4-2: Neighborhood Indistinguishability Lower Bound
 * ================================================================ */

/**
 * @brief Construct the T-neighborhood graph of each node (as a labeled tree).
 *
 * After T rounds in the LOCAL model, each node knows its T-neighborhood
 * (the subgraph induced by nodes within distance T, including UIDs and
 * port numbers). If two nodes have isomorphic T-neighborhoods, they must
 * produce the same output in any deterministic T-round algorithm.
 *
 * This is the fundamental indistinguishability argument that underlies
 * almost all LOCAL lower bounds.
 *
 * @param ctx           LOCAL context.
 * @param T             Number of rounds.
 * @param pns           Port numberings.
 * @param labels        Output: equivalence class label for each node
 *                      (two nodes share a label iff they have isomorphic
 *                      T-neighborhoods).
 * @param num_classes   Output: number of distinct equivalence classes.
 * @return              0 on success.
 */
int lb_neighborhood_classes(const local_context_t *ctx, uint32_t T,
                            const port_numbering_t *pns,
                            uint32_t *labels, uint32_t *num_classes);

/**
 * @brief Determine if a given problem is impossible in T rounds
 *        using neighborhood indistinguishability.
 *
 * If two nodes with isomorphic T-neighborhoods must produce different outputs
 * (according to the problem specification), then the problem cannot be solved
 * in T rounds deterministically.
 *
 * @param ctx           LOCAL context.
 * @param T             Rounds.
 * @param pns           Port numberings.
 * @param required_fn   Called on each pair (u,v) of nodes with identical
 *                      T-neighborhoods. If required_fn returns false (meaning
 *                      they must differ), the problem is impossible in T rounds.
 * @return              true if the problem specification requires two nodes
 *                      with identical T-neighborhoods to differ, i.e.,
 *                      the problem is IMPOSSIBLE in T rounds.
 */
typedef bool (*lb_required_equal_fn_t)(uint32_t u, uint32_t v, void *context);
bool lb_indistinguishability_test(const local_context_t *ctx, uint32_t T,
                                  const port_numbering_t *pns,
                                  lb_required_equal_fn_t required_fn, void *fn_context);

/* ================================================================
 * L4-3: Kuhn-Moscibroda-Wattenhofer MIS Lower Bound
 * ================================================================ */

/**
 * @brief Construct a graph family that forces a large MIS lower bound.
 *
 * Kuhn, Moscibroda, Wattenhofer (2004, 2016) proved that any deterministic
 * LOCAL algorithm for MIS requires Ω(min{log Δ, √(log n)}) rounds.
 *
 * This function builds the "cluster graph" that is the core of the lower
 * bound construction: it's a tree of clusters where the root is a large
 * cluster and deeper levels have smaller clusters.
 *
 * @param ctx           LOCAL context (will be filled).
 * @param delta         Desired maximum degree Δ.
 * @param num_nodes     Desired number of nodes (n = poly(Δ)).
 * @return              0 on success.
 */
int lb_kmw_hard_graph(local_context_t *ctx, uint32_t delta, uint32_t num_nodes);

/**
 * @brief For the KMW hard graph, compute the minimum rounds needed for MIS.
 *
 * @param delta     Maximum degree.
 * @param n         Number of nodes.
 * @return          Lower bound on MIS rounds for this graph family
 *                  (= Ω(min{log Δ, √(log n)})).
 */
uint32_t lb_kmw_mis_lower_bound(uint32_t delta, uint32_t n);

/* ================================================================
 * L4-4: Speedup Simulation
 * ================================================================ */

/**
 * @brief Speedup lemma: if problem P is solvable in T rounds, then
 *        its "one-round-left" version is solvable in T-1 rounds.
 *
 * The speedup simulation is a meta-technique used to derive lower bounds
 * via contradiction: assume problem P is solvable in T rounds; construct
 * a related problem P' solvable in T-1 rounds; if P' is known to be
 * impossible in T-1 rounds, then P must be impossible in T rounds.
 *
 * This function implements the simulation: given a T-round LOCAL algorithm
 * for P (represented as a function mapping T-neighborhoods to outputs),
 * construct a (T-1)-round algorithm for P'.
 *
 * @param ctx               LOCAL context.
 * @param T                 Number of rounds.
 * @param pns               Port numberings.
 * @param output_original   Output of the T-round algorithm.
 * @param output_speedup    Output: what a (T-1)-round algorithm can compute.
 * @return                  0 on success.
 */
int lb_speedup_simulation(const local_context_t *ctx, uint32_t T,
                          const port_numbering_t *pns,
                          const uint32_t *output_original, uint32_t *output_speedup);

/* ================================================================
 * L4-5: Lower Bounds via Graph Fibration Theory
 * ================================================================ */

/**
 * @brief Compute whether graph G covers graph H (covering map / fibration).
 *
 * In distributed computing, if G is a covering graph of H, then any LOCAL
 * algorithm run on H can be "lifted" to G. This means that lower bounds
 * on H imply lower bounds on G (and the entire family of graphs that H covers).
 *
 * @param ctx_G     Graph G (candidate cover).
 * @param ctx_H     Graph H (candidate base).
 * @param mapping   Output: mapping[i] = vertex in H that vertex i in G is mapped to.
 * @return          true iff G → H is a covering map.
 */
bool lb_is_covering_graph(const local_context_t *ctx_G, const local_context_t *ctx_H,
                          uint32_t *mapping);

/**
 * @brief Given a base graph H, construct its universal cover (infinite tree).
 *
 * If H has no cycles, its universal cover is itself. Otherwise it's an
 * infinite tree. This function constructs a finite truncation of depth K.
 *
 * @param ctx_H         Base graph H.
 * @param ctx_cover     Output: truncated universal cover (depth K).
 * @param K             Truncation depth.
 * @return              0 on success.
 */
int lb_universal_cover_truncation(const local_context_t *ctx_H,
                                  local_context_t *ctx_cover, uint32_t K);

/* ================================================================
 * L4-6: Lower Bounds from Communication Complexity
 * ================================================================ */

/**
 * @brief Reduce a 2-party communication complexity problem to a LOCAL
 *        problem, establishing a lower bound.
 *
 * The classic reduction: partition the graph into two halves connected by
 * a cut of k edges; simulate the LOCAL algorithm in 2-party communication;
 * the number of rounds T translates to O(T * k * log n) bits of communication.
 * If the communication problem requires Ω(C) bits, then T = Ω(C / (k log n)).
 *
 * @param n                 Number of nodes.
 * @param cut_edges         Number of edges crossing the cut.
 * @param comm_lower_bound  Known communication complexity lower bound (in bits).
 * @return                  Lower bound on LOCAL rounds.
 */
uint32_t lb_from_communication_complexity(uint32_t n, uint32_t cut_edges,
                                          uint64_t comm_lower_bound);

/* ================================================================
 * L4-7: Lower Bounds via Ramsey Theory
 * ================================================================ */

/**
 * @brief Apply the Ramsey-theoretic method to LOCAL lower bounds.
 *
 * Theorem (Naor & Stockmeyer, 1995): If a graph family is closed under
 * taking induced subgraphs and contains arbitrarily large complete graphs,
 * then the "what can be computed locally" is limited by Ramsey numbers.
 *
 * This function computes the Ramsey threshold: for graph family F with
 * the above properties, if a T-round algorithm solves problem Π on F,
 * then the Ramsey number R(k, k) must be ≤ n for k related to the output
 * size.
 *
 * @param output_colors     Number of distinct outputs each node can produce.
 * @param rounds            Number of rounds T.
 * @return                  Minimum graph size n needed for the problem to
 *                          be solvable in T rounds (i.e., lower bound on n
 *                          for given T; equivalently lower bound on T for given n).
 */
uint32_t lb_ramsey_threshold(uint32_t output_colors, uint32_t rounds);

/* ================================================================
 * L4-8: Deterministic vs Randomized Separation
 * ================================================================ */

/**
 * @brief Compare deterministic vs randomized round complexity for a problem.
 *
 * For many LOCAL problems, randomization provides an exponential speedup.
 * This function computes the separation gap:
 *   gap = T_det / T_rand
 *
 * For MIS on a path: T_det = Ω(log* n), T_rand = O(1) → infinite gap.
 * For MIS on general graphs: T_det = 2^O(√(log n)), T_rand = O(log n) → exponential gap.
 *
 * @param T_det     Deterministic round complexity (lower bound).
 * @param T_rand    Randomized round complexity (upper bound).
 * @return          Separation gap (T_det / T_rand, as fixed-point scaled by 1000).
 *                  Returns UINT32_MAX if T_rand = 0 (constant-time randomized).
 */
uint32_t lb_det_rand_gap(uint32_t T_det, uint32_t T_rand);

/* ================================================================
 * L4-9: Lower Bound Table for Canonical Problems
 * ================================================================ */

/**
 * @brief Get the known deterministic lower bound for a canonical problem.
 *
 * @return Round lower bound (or 0 if unknown / O(1)).
 */
uint32_t lb_for_problem(const char *problem_name, uint32_t n, uint32_t delta);

/**
 * @brief Print a summary table of known LOCAL model lower bounds.
 */
void lb_print_summary(void);

#endif /* LOWER_BOUNDS_H */
