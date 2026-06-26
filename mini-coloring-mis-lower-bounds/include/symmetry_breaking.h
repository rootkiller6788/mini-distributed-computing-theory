/*
 * symmetry_breaking.h -- Symmetry Breaking in Distributed Computing
 *
 * L1 Definitions:
 *   - Symmetry: nodes in same local state make same decisions
 *   - Symmetry breaking: mechanism to differentiate otherwise identical nodes
 *   - Unique IDs: provided by system, break symmetry deterministically
 *   - Random bits: nodes generate random values to break symmetry
 *
 * L2 Core Concepts:
 *   - ID-based: compare IDs, smallest/largest wins
 *   - Random: each node generates random value, largest/smallest wins
 *   - Bitwise: exchange ID bits, find first differing bit
 *   - Tournament: multi-round elimination based on random coin flips
 *
 * L3 Mathematical Structures:
 *   - Ordering: total order on IDs induces a priority scheme
 *   - Random ranking: random values define a random permutation
 *   - Cole-Vishkin bit-graph: IDs as bitstrings, compare bit positions
 *
 * L4 Fundamental Laws:
 *   - No deterministic symmetry breaking without IDs (impossible on rings)
 *   - Random symmetry breaking succeeds w.h.p. in O(log n) rounds
 *   - Linial's log* n bound: symmetry breaking needs log* n even with IDs
 *
 * L5 Methods:
 *   - Cole-Vishkin coin tossing: deterministic using ID bits
 *   - Randomized symmetry breaking: pick random rank
 *   - Tournament tree: hierarchical elimination
 *
 * Reference: Cole & Vishkin (1986), Linial (1992)
 * Courses: MIT 6.852, ETH 263-4650
 */

#ifndef SYMMETRY_BREAKING_H
#define SYMMETRY_BREAKING_H

#include "graph.h"
#include <stdint.h>
#include <stdbool.h>

/* === Cole-Vishkin Coin Tossing === */

/* Compute the first differing bit position between two IDs.
 * Returns (position, my_bit) encoded as position << 1 | my_bit.
 * This is the core of Cole-Vishkin: in each round, reduce the ID space. */
int cv_first_differing_bit(int my_id, int left_id, int right_id);

/* Run one round of Cole-Vishkin reduction on a ring.
 * Each node computes (first_differing_bit_pos, my_bit_at_that_pos).
 * This reduces the label space from O(L) to O(log L). */
void cv_ring_one_round(const int* ids, int n, int* new_labels);

/* Run O(log* n) rounds of Cole-Vishkin to get O(1) labels.
 * On a ring of n nodes, produces 6 labels in O(log* n) rounds. */
int* cv_ring_full(int n, const int* ids, int* out_rounds);

/* === Randomized Symmetry Breaking === */

/* Each node generates a random rank, finds if it is the
 * local maximum among active neighbors. */
bool random_local_max(double my_rand, const double* neighbor_rands,
                      int n_neighbors);

/* Randomized tournament: each round, nodes flip a coin.
 * Nodes that flip heads and whose neighbors all flip tails
 * survive to the next round. */
bool tournament_round_survives(double my_coin, const double* neighbor_coins,
                                int n_neighbors);

/* === ID-Based Symmetry Breaking === */

/* Simple: node with smallest/largest ID among neighbors wins */
int min_id_among(int my_id, const int* neighbor_ids, int n_neighbors);
int max_id_among(int my_id, const int* neighbor_ids, int n_neighbors);

/* Check if this node is the local minimum (smallest ID in closed neighborhood) */
bool is_local_min(int my_id, const int* neighbor_ids, int n_neighbors);

/* Check if this node is the local maximum */
bool is_local_max(int my_id, const int* neighbor_ids, int n_neighbors);

/* === Multi-Bit Symmetry Breaking === */

/* Exchange k bits of ID per round. After ceil(log_{k+1} n) rounds,
 * nodes learn relative ordering of IDs in their neighborhood. */
int symmetry_exchange_bits(int my_id, int round, int bits_per_round);

/* Compute a coloring from ID bits: use first ceil(log(Delta+1)) bits
 * as an initial color (not necessarily proper). */
int id_to_initial_color(int id, int max_colors);

/* === Combined Symmetry Breaking (ID + Random) === */

/* If IDs are distinct, use IDs. Otherwise, fall back to random.
 * Returns the winning value (for comparison with neighbors). */
double symmetry_break_combined(int my_id, uint64_t* seed,
                                bool ids_distinct);

/* === Analysis Functions === */

/* Probability that random symmetry breaking produces a
 * maximal independent set in one round (for d-regular graphs). */
double symmetry_prob_one_round_mis(int degree);

/* Expected number of rounds until all nodes have decided
 * under random symmetry breaking (simplified analysis). */
double symmetry_expected_rounds(int n, int degree);

#endif /* SYMMETRY_BREAKING_H */
