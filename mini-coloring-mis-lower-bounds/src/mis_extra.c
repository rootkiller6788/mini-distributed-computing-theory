#include "mis.h"
#include <stdlib.h>
#include <string.h>

MIS* mis_tree(const Graph* g) {
    (void)g; return mis_create(g ? g->n : 0);
}

MIS* mis_bipartite(const Graph* g, const Coloring* bipartition) {
    (void)bipartition;
    return mis_greedy_sequential(g, NULL);
}

MIS* mis_bounded_independence(const Graph* g, int independence_bound, uint64_t seed) {
    (void)independence_bound;
    return mis_luby_randomized(g, seed);
}

int mis_dominating_count(const Graph* g, const MIS* mis) {
    if (!g || !mis) return 0;
    int cnt = 0;
    for (int v = 0; v < g->n; v++) {
        if (mis->in_mis[v]) { cnt++; continue; }
        AdjList* al = &g->adjacency[v];
        for (int i = 0; i < al->degree; i++)
            if (mis->in_mis[al->neighbors[i]]) { cnt++; break; }
    }
    return cnt;
}

bool mis_is_maximum(const Graph* g, const MIS* mis) {
    if (!g || !mis) return false;
    int max_sz = mis_maximum_size(g);
    return max_sz >= 0 && mis->mis_size == max_sz;
}

int mis_maximum_size(const Graph* g) {
    if (!g || g->n > 30) return -1;
    return graph_independence_number(g);
}

Graph* mis_lower_bound_graph(int n, int Delta, uint64_t seed) {
    return graph_create_bounded_degree(n, Delta, seed);
}

Graph* mis_kmw_ball_graph(int radius, int degree) {
    (void)radius; (void)degree;
    return graph_create_ring(4);
}

ExecutionTrace* mis_simulate_luby(const Graph* g, uint64_t seed, int max_rounds) {
    if (!g) return NULL;
    ExecutionTrace* t = trace_create(g->n, max_rounds);
    bool* active = (bool*)malloc((size_t)g->n * sizeof(bool));
    bool* in_mis = (bool*)calloc((size_t)g->n, sizeof(bool));
    if (!active || !in_mis) { free(active); free(in_mis); trace_free(t); return NULL; }
    for (int v = 0; v < g->n; v++) active[v] = true;
    uint64_t st = seed;
    for (int r = 0; r < max_rounds; r++) {
        mis_luby_round(g, active, in_mis, &st);
        trace_record_round(t, r, NULL, in_mis, NULL);
        int any = 0;
        for (int v = 0; v < g->n; v++)
            if (active[v]) { any = 1; break; }
        if (!any) break;
    }
    free(active); free(in_mis);
    return t;
}

double mis_luby_expected_rounds(const Graph* g, int trials, uint64_t seed) {
    if (!g || trials <= 0) return -1.0;
    double total = 0.0;
    uint64_t st = seed;
    for (int t = 0; t < trials; t++) {
        ExecutionTrace* trace = mis_simulate_luby(g, st++, 1000);
        if (trace) {
            total += (double)trace->n_rounds;
            trace_free(trace);
        }
    }
    return total / (double)trials;
}
