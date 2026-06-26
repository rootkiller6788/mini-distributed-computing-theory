/**
 * @file graph_problems.h
 * @brief Canonical Graph Problems in the LOCAL Model
 *
 * This file defines the canonical distributed graph problems studied
 * in the LOCAL model. Each problem is defined by:
 *  - Input: what each node knows initially (UID, degree, ports, possible input label)
 *  - Output: what each node must compute (e.g., a color, a set membership bit)
 *  - Constraints: global properties the output must satisfy
 *  - Round complexity: the LOCAL model's round complexity for the problem
 *
 * Canonical problems covered:
 *  L6-1: (Δ+1)-Vertex Coloring
 *  L6-2: Maximal Independent Set (MIS)
 *  L6-3: Maximal Matching (MM)
 *  L6-4: 3-Coloring a Ring (Linial's problem)
 *  L6-5: 2-Coloring a Bipartite Graph (with orientation)
 *  L6-6: Edge Coloring
 *  L6-7: Vertex Cover approximation
 *  L6-8: Dominating Set
 *  L6-9: Orientation / Sinkless Orientation
 *
 * References:
 *  - Barenboim & Elkin (2013). "Distributed Graph Coloring."
 *  - Linial (1992). "Locality in distributed graph algorithms."
 *  - Luby (1986). "A simple parallel algorithm for the maximal independent set problem."
 *  - Alon, Babai, Itai (1986). "A fast and simple randomized parallel algorithm for the MIS."
 *  - Panconesi & Rizzi (2001). "Some simple distributed algorithms for sparse networks."
 *
 * Level: L6 (Canonical Problems), L1 (Definitions)
 */

#ifndef GRAPH_PROBLEMS_H
#define GRAPH_PROBLEMS_H

#include "local_model.h"
#include "port_numbering.h"
#include <stdint.h>
#include <stdbool.h>

/* ================================================================
 * L6-1: (Δ+1)-Vertex Coloring
 * ================================================================ */

/**
 * @brief State for (Δ+1)-vertex coloring.
 *
 * Each node chooses a color c ∈ {0, ..., Δ} such that for every edge {u,v},
 * c(u) ≠ c(v). Δ is the maximum degree of the graph.
 *
 * Key facts:
 *  - (Δ+1) colors always suffice (greedy coloring).
 *  - Deterministic LOCAL complexity: O(Δ log Δ + log* n) (Barenboim-Elkin).
 *  - Randomized LOCAL complexity: O(log n) (Johansson).
 */
typedef struct {
    uint32_t color;             /* Current color (output when terminated) */
    uint32_t delta;             /* Local view of max degree (Δ) */
    uint32_t palette_size;      /* Δ+1 */
    uint32_t *neighbor_colors;  /* Colors reported by neighbors last round */
    bool     conflict;          /* Does this node have a color conflict? */
    uint32_t trial_round;       /* Round counter within the algorithm */
} vertex_coloring_state_t;

/**
 * @brief Initialize coloring state for a node.
 *
 * @param state         State to initialize.
 * @param delta         Maximum degree known to the node.
 * @return              0 on success.
 */
int vcol_init(vertex_coloring_state_t *state, uint32_t delta);

/**
 * @brief Free resources held by a coloring state.
 */
void vcol_destroy(vertex_coloring_state_t *state);

/**
 * @brief The "reduce" step: each node picks a trial color, exchanges with
 *        neighbors, and if conflict-free, becomes permanently colored.
 *        Otherwise retries next round.
 *
 * This is the core subroutine of the Linial-Cole-Vishkin-type coloring
 * algorithms where colors are encoded in log* n rounds.
 *
 * @param ctx       LOCAL context.
 * @param states    Array of coloring states (one per node).
 * @param round_fn  Per-round callback (can be the Linial reduction).
 * @return          Number of nodes still uncolored.
 */
uint32_t vcol_reduce_round(local_context_t *ctx, vertex_coloring_state_t *states);

/**
 * @brief Check if a vertex coloring is a proper coloring.
 *
 * A proper coloring has c(u) ≠ c(v) for all edges {u,v}.
 *
 * @param ctx       LOCAL context.
 * @param colors    Array of n colors.
 * @return          true iff proper.
 */
bool vcol_is_proper(const local_context_t *ctx, const uint32_t *colors);

/**
 * @brief Count the number of distinct colors used.
 *
 * This measures the quality of a coloring algorithm.
 *
 * @param colors    Array of n colors.
 * @param n         Number of nodes.
 * @return          Number of distinct colors.
 */
uint32_t vcol_count_colors(const uint32_t *colors, uint32_t n);

/* ================================================================
 * L6-2: Maximal Independent Set (MIS)
 * ================================================================ */

/**
 * @brief State for Maximal Independent Set computation.
 *
 * An independent set I ⊆ V satisfies: for all {u,v} ∈ E, not (u ∈ I and v ∈ I).
 * It is maximal if no vertex can be added to I without breaking independence.
 *
 * Key facts:
 *  - Luby's randomized algorithm: O(log n) rounds.
 *  - Deterministic LOCAL: 2^O(√(log n)) rounds (Panconesi-Srinivasan).
 *  - Lower bound: Ω(min{log Δ, √(log n)}) for deterministic (Kuhn et al.).
 */
typedef enum {
    MIS_UNDECIDED = 0,  /* Node has not yet decided */
    MIS_IN_SET    = 1,  /* Node is in the independent set */
    MIS_OUT_SET   = 2   /* Node is definitely not in the set */
} mis_state_t;

typedef struct {
    mis_state_t status;         /* MIS membership status */
    uint32_t    round_joined;   /* Round when status was decided */
    uint32_t   *neighbor_status;/* Statuses of neighbors from last round */
    bool        active;         /* Is this node still participating? */
} mis_node_state_t;

/**
 * @brief Initialize MIS state.
 */
int mis_init(mis_node_state_t *state);

/**
 * @brief Free MIS state resources.
 */
void mis_destroy(mis_node_state_t *state);

/**
 * @brief Execute one round of Luby's MIS algorithm.
 *
 * In Luby's algorithm, each active node picks a random number.
 * A node joins MIS if its number is strictly greater than all active neighbors'.
 * Neighbors of newly joined nodes become OUT.
 *
 * @param ctx       LOCAL context.
 * @param states    Array of MIS states.
 * @param seed      Per-round random seed (for reproducibility).
 * @return          Number of nodes that changed status this round.
 */
uint32_t mis_luby_round(local_context_t *ctx, mis_node_state_t *states, uint64_t seed);

/**
 * @brief Verify that the computed set is a valid MIS.
 *
 * Checks:
 *  1. Independence: no two adjacent nodes are both IN_SET.
 *  2. Maximality: every OUT_SET node has at least one IN_SET neighbor.
 *
 * @param ctx       LOCAL context.
 * @param states    Array of MIS states.
 * @return          true iff valid MIS.
 */
bool mis_is_valid(const local_context_t *ctx, const mis_node_state_t *states);

/* ================================================================
 * L6-3: Maximal Matching (MM)
 * ================================================================ */

/**
 * @brief State for Maximal Matching computation.
 *
 * A matching M ⊆ E satisfies: no two edges in M share an endpoint.
 * It is maximal if no edge can be added to M without breaking this property.
 *
 * Key facts:
 *  - Deterministic LOCAL: 2^O(√(log n)) rounds (same as MIS via reduction).
 *  - Randomized LOCAL: O(log n) rounds.
 *  - A maximal matching is a 2-approximation to the maximum matching.
 */
typedef enum {
    MM_UNMATCHED  = 0,   /* Node is not matched */
    MM_MATCHED    = 1,   /* Node is matched to a specific neighbor */
    MM_PROPOSING  = 2    /* Node has proposed to a neighbor, awaiting response */
} mm_state_t;

typedef struct {
    mm_state_t  status;         /* Matching status */
    uint32_t    partner;        /* If matched, UID of partner (0 if unmatched) */
    uint32_t    partner_port;   /* Port number to partner */
    uint32_t    awaiting_port;  /* Port being proposed to */
    bool        proposal_sent;  /* Have we sent a proposal this round? */
} mm_node_state_t;

/**
 * @brief Initialize matching state.
 */
int mm_init(mm_node_state_t *state);

/**
 * @brief Execute one round of a maximal matching algorithm.
 *
 * Unmatched nodes propose to an unmatched neighbor (if any).
 * If two nodes propose to each other, they become matched.
 *
 * @param ctx       LOCAL context.
 * @param states    Array of matching states.
 * @param pns       Port numberings.
 * @param seed      Random seed (for randomized tie-breaking).
 * @return          Number of new matches formed this round.
 */
uint32_t mm_matching_round(local_context_t *ctx, mm_node_state_t *states,
                           const port_numbering_t *pns, uint64_t seed);

/**
 * @brief Verify that the computed matching is a valid maximal matching.
 *
 * @param ctx       LOCAL context.
 * @param states    Array of matching states.
 * @return          true iff valid maximal matching.
 */
bool mm_is_valid(const local_context_t *ctx, const mm_node_state_t *states);

/* ================================================================
 * L6-4: 3-Coloring a Ring (Linial's Problem)
 * ================================================================ */

/**
 * @brief Compute a 3-coloring of a ring C_n using Linial's algorithm.
 *
 * Theorem (Linial, 1992): 3-coloring a ring requires Ω(log* n) rounds
 * in the deterministic LOCAL model.
 *
 * Cole & Vishkin (1986) gave a matching O(log* n) upper bound.
 *
 * This function implements the Cole-Vishkin algorithm.
 *
 * @param ctx       LOCAL context (must be a cycle C_n).
 * @param colors    Output array: 3-coloring (values in {0,1,2}).
 * @param pns       Port numberings (ring orientation).
 * @return          Number of rounds executed.
 *
 * Complexity: O(log* n) rounds.
 */
uint32_t ring_3color_cole_vishkin(local_context_t *ctx, uint32_t *colors,
                                  const port_numbering_t *pns);

/**
 * @brief Verify that a cycle is properly 3-colored.
 *
 * @param ctx       LOCAL context (must be a cycle).
 * @param colors    Array of colors.
 * @return          true iff proper 3-coloring.
 */
bool ring_3color_verify(const local_context_t *ctx, const uint32_t *colors);

/* ================================================================
 * L6-5: Sinkless Orientation
 * ================================================================ */

/**
 * @brief State for sinkless orientation problem.
 *
 * A sinkless orientation of a graph is an assignment of a direction
 * to each edge such that no node has all incident edges directed inward
 * (i.e., no node is a sink).
 *
 * Key facts:
 *  - Ω(log log n) randomized round lower bound (Brandt et al., 2016).
 *  - This is strictly harder than sinkless orientation on cycles (which is
 *    in O(1) rounds), showing the importance of the graph structure.
 */
typedef struct {
    uint32_t *outgoing_ports;   /* Ports to which this node directs edges outward */
    uint32_t  outgoing_count;   /* Number of outgoing edges */
    bool      is_sink;          /* Are all edges directed inward? */
} sinkless_state_t;

/**
 * @brief Initialize sinkless orientation state.
 */
int sinkless_init(sinkless_state_t *state, uint32_t degree);

/**
 * @brief Execute one round of sinkless orientation.
 *
 * Each node orients its incident edges. After the round, no node
 * should be a sink (unless degree = 0).
 *
 * @param ctx       LOCAL context.
 * @param states    Array of sinkless states.
 * @param pns       Port numberings.
 * @param seed      Random seed.
 * @return          Number of sink nodes remaining.
 */
uint32_t sinkless_round(local_context_t *ctx, sinkless_state_t *states,
                        const port_numbering_t *pns, uint64_t seed);

/**
 * @brief Verify sinkless property.
 *
 * @return true iff no node with degree > 0 is a sink.
 */
bool sinkless_is_valid(const local_context_t *ctx, const sinkless_state_t *states);

/* ================================================================
 * L6-6: Weak 2-Coloring (Awerbuch et al.)
 * ================================================================ */

/**
 * @brief Compute a weak 2-coloring of a graph.
 *
 * A weak k-coloring is a coloring where each node has at least one neighbor
 * of a different color. This is much weaker than proper coloring.
 *
 * Key fact: Weak 2-coloring can be computed in O(log* n) rounds (Naor & Stockmeyer).
 *
 * @param ctx       LOCAL context.
 * @param colors    Output: weak 2-coloring (values in {0,1}).
 * @param pns       Port numberings.
 * @return          Number of rounds executed.
 */
uint32_t weak_2coloring(local_context_t *ctx, uint32_t *colors, const port_numbering_t *pns);

/**
 * @brief Verify weak 2-coloring property.
 *
 * Each node with degree > 0 must have at least one neighbor of different color.
 */
bool weak_2coloring_verify(const local_context_t *ctx, const uint32_t *colors);

/* ================================================================
 * L6-7: Vertex Cover Approximation
 * ================================================================ */

/**
 * @brief Compute a 2-approximate vertex cover via the matching method.
 *
 * Take all endpoints of a maximal matching; this gives a vertex cover
 * of size at most 2×OPT.
 *
 * @param mm_states  Maximal matching result.
 * @param n          Number of nodes.
 * @param vc         Output: vc[i] = true iff node i is in the vertex cover.
 */
void vc_from_matching(const mm_node_state_t *mm_states, uint32_t n, bool *vc);

/**
 * @brief Verify that a set is a valid vertex cover.
 *
 * @param ctx   LOCAL context.
 * @param vc    Boolean array: vc[i] = true means node i is in the cover.
 * @return      true iff every edge has at least one endpoint in vc.
 */
bool vc_is_valid(const local_context_t *ctx, const bool *vc);

/**
 * @brief Count the size of the vertex cover.
 */
uint32_t vc_size(const bool *vc, uint32_t n);

/* ================================================================
 * L6-8: Dominating Set
 * ================================================================ */

/**
 * @brief Compute a dominating set via a simple greedy distributed algorithm.
 *
 * A dominating set D ⊆ V satisfies: every node v ∈ V is either in D or
 * has a neighbor in D.
 *
 * @param ctx       LOCAL context.
 * @param domset    Output: domset[i] = true iff node i is in the dominating set.
 * @param pns       Port numberings.
 * @param seed      Random seed.
 * @return          Number of rounds executed.
 */
uint32_t dominating_set_greedy(local_context_t *ctx, bool *domset,
                               const port_numbering_t *pns, uint64_t seed);

/**
 * @brief Verify dominating set property.
 *
 * @param ctx       LOCAL context.
 * @param domset    Boolean array.
 * @return          true iff valid dominating set.
 */
bool domset_is_valid(const local_context_t *ctx, const bool *domset);

#endif /* GRAPH_PROBLEMS_H */
