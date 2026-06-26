/*
 * lower_bounds.h -- Lower Bounds for Distributed Coloring and MIS
 *
 * L1 Definitions:
 *   - Lower bound: any algorithm solving problem P requires Omega(f(n)) rounds
 *   - Round complexity lower bound: proven via indistinguishability arguments
 *   - Neighborhood graph: captures what a node can learn in t rounds
 *
 * L2 Core Concepts:
 *   - Indistinguishability: executions that look identical to a node
 *     for t rounds force the same output
 *   - Covering maps: graph homomorphisms preserving local neighborhoods
 *   - Speedup simulation: T/2 rounds using distance-2 graph
 *
 * L4 Fundamental Laws:
 *   - Linial (1992): 3-coloring a ring requires Omega(log* n) rounds
 *   - Kuhn, Moscibroda, Wattenhofer:
 *     MIS: Omega(min{log Delta, sqrt(log n)}) rounds
 *   - Naor & Stockmeyer: LCL problems have O(1), Theta(log* n), or Omega(n)
 *   - Chang-Kopelowitz-Pettie (2019): exponential separation rand vs det
 *
 * L5 Methods for Lower Bounds:
 *   - Indistinguishability argument
 *   - Neighborhood graph analysis
 *   - Covering graph constructions
 *   - Ramsey-theoretic arguments
 *   - Round elimination technique
 *
 * L8 Advanced Topics:
 *   - Round elimination: mechanically prove lower bounds
 *   - Gap theorems for LCL complexity
 *   - Randomized vs deterministic separations
 *
 * L9 Research Frontiers:
 *   - Complexity landscape of LCL problems on trees (Balliu et al.)
 *   - Distributed Theta(log n) region for LCL problems
 *   - Quantum distributed computing lower bounds
 *
 * Reference: Linial (1992), KMW (2004/2016), Chang & Pettie (2019)
 * Courses: ETH 263-4650, MIT 6.852, CMU 15-859
 */

#ifndef LOWER_BOUNDS_H
#define LOWER_BOUNDS_H

#include "graph.h"
#include "local_model.h"

/* === Linial Lower Bound: Omega(log* n) for Ring Coloring === */

/* Pair of ring ID assignments that are t-indistinguishable
 * but require different colors. Proves no t-round algorithm
 * can 3-color a ring larger than 2^t. */
typedef struct {
    int* ids_a;
    int* ids_b;
    int  n;
    int  t;
} IndistinguishablePair;

IndistinguishablePair* lower_bound_ring_indistinguishable(int n, int t);
void lower_bound_indistinguishable_free(IndistinguishablePair* pair);
bool ring_configs_indistinguishable(const int* ids_a, const int* ids_b,
                                     int n, int t);

/* === Neighborhood Graph Lower Bounds === */

NeighborhoodGraph* neighborhood_graph_build(const Graph* g, int t);
int neighborhood_graph_chromatic(const NeighborhoodGraph* ng);

/* === KMW Lower Bound for MIS === */

Graph* kmw_ball_graph_create(int radius, int degree);
int   kmw_ball_graph_mis_rounds(int radius, int degree);

/* === Round Elimination === */

typedef struct {
    int* palette_sizes;
    int** palettes;
    int  n;
    int  max_palette_size;
} RelaxedColoring;

RelaxedColoring* round_elimination_coloring(const Graph* g);
bool relaxed_coloring_valid(const Graph* g, const RelaxedColoring* rc);
void relaxed_coloring_free(RelaxedColoring* rc);

/* === Speedup Theorem === */

Graph* graph_square(const Graph* g);

/* === Communication Complexity Lower Bounds === */

double mis_information_lower_bound(const Graph* g);

/* === Lower Bound Catalog === */

typedef enum {
    LB_LINIAL_RING_3COL,
    LB_KMW_MIS,
    LB_NAOR_STOCKMEYER,
    LB_GREENBEYER_COLORING,
    LB_BALLIU_TREES,
    LB_CHANG_PETTIE_GAP,
    LB_COUNT
} LowerBoundResultID;

typedef struct {
    LowerBoundResultID id;
    char*  problem;
    char*  model;
    char*  algo_type;
    double bound_value;
    char*  bound_expr;
    char*  reference;
    int    year;
} LowerBoundRecord;

const LowerBoundRecord* lower_bound_catalog(int* count);

/* === Lower Bound Verification === */

bool lower_bound_verify_exhaustive(const Graph* g, int claimed_rounds,
                                    int target_colors);
bool lower_bound_is_witness(const Graph* g, int t, int k);

#endif /* LOWER_BOUNDS_H */
