/*
 * mis.h -- Maximal Independent Set (MIS) in Distributed Computing
 *
 * L1: Independent Set, Maximal Independent Set, dominating set
 * L2: Luby randomized MIS, deterministic MIS
 * L4: Luby theorem: O(log n) expected rounds
 *     KMW: Omega(min{log Delta, sqrt(log n)}) lower bound
 * L5: Greedy MIS, Luby, Linial deterministic, network decomposition
 * L6: MIS on ring, general graphs, trees, bipartite graphs
 *
 * Reference: Luby (1986), Linial (1992), KMW (2004/2016)
 * Courses: ETH 263-4650, MIT 6.852, CMU 15-859
 */

#ifndef MIS_H
#define MIS_H

#include "graph.h"
#include "local_model.h"
#include "coloring.h"
#include <stdint.h>
#include <stdbool.h>

MIS* mis_greedy_sequential(const Graph* g, const int* order);
MIS* mis_greedy_random_order(const Graph* g, uint64_t seed);
MIS* mis_greedy_min_degree(const Graph* g);

MIS* mis_luby_randomized(const Graph* g, uint64_t seed);
int mis_luby_round(const Graph* g, bool* active, bool* in_mis, uint64_t* seed);
MIS* mis_luby_phased(const Graph* g, uint64_t seed);
MIS* mis_luby_discrete(const Graph* g, int K, uint64_t seed);

MIS* mis_linial_deterministic(const Graph* g);
MIS* mis_network_decomposition(const Graph* g, int cluster_diameter);
MIS* mis_ring_deterministic(int n, const int* ids);

MIS* mis_tree(const Graph* g);
MIS* mis_bipartite(const Graph* g, const Coloring* bipartition);
MIS* mis_bounded_independence(const Graph* g, int independence_bound, uint64_t seed);

int mis_dominating_count(const Graph* g, const MIS* mis);
bool mis_is_maximum(const Graph* g, const MIS* mis);
int mis_maximum_size(const Graph* g);

Graph* mis_lower_bound_graph(int n, int Delta, uint64_t seed);
Graph* mis_kmw_ball_graph(int radius, int degree);

ExecutionTrace* mis_simulate_luby(const Graph* g, uint64_t seed, int max_rounds);
double mis_luby_expected_rounds(const Graph* g, int trials, uint64_t seed);

#endif /* MIS_H */
