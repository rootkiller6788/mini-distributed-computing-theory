#include "mis.h"
#include "distributed_constants.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

extern uint64_t lcg_random_next(uint64_t* state);
extern double lcg_random_double(uint64_t* state);

int mis_luby_round(const Graph* g, bool* active, bool* in_mis,
                   uint64_t* seed) {
    if (!g || !active || !in_mis || !seed) return 0;
    int n = g->n;
    int joined = 0;
    double* ranks = (double*)malloc((size_t)n * sizeof(double));
    if (!ranks) return 0;
    for (int v = 0; v < n; v++)
        ranks[v] = active[v] ? lcg_random_double(seed) : -1.0;
    for (int v = 0; v < n; v++) {
        if (!active[v]) continue;
        int lmax = 1;
        AdjList* al = &g->adjacency[v];
        for (int i = 0; i < al->degree; i++) {
            int nb = al->neighbors[i];
            if (active[nb] && ranks[nb] > ranks[v]) { lmax = 0; break; }
        }
        if (lmax) {
            in_mis[v] = true; active[v] = false; joined++;
            for (int i = 0; i < al->degree; i++)
                active[al->neighbors[i]] = false;
        }
    }
    free(ranks);
    return joined;
}

MIS* mis_luby_randomized(const Graph* g, uint64_t seed) {
    if (!g) return NULL;
    int n = g->n;
    MIS* mis = mis_create(n);
    if (!mis) return NULL;
    bool* active = (bool*)malloc((size_t)n * sizeof(bool));
    if (!active) { mis_free(mis); return NULL; }
    for (int v = 0; v < n; v++) active[v] = true;
    uint64_t st = seed;
    int max_r = n * 20;
    for (int r = 0; r < max_r; r++) {
        int jn = mis_luby_round(g, active, mis->in_mis, &st);
        if (jn == 0) {
            int any = 0;
            for (int v = 0; v < n; v++)
                if (active[v]) { any = 1; break; }
            if (!any) break;
        }
        mis->mis_size += jn;
    }
    /* Fill dominator info */
    for (int v = 0; v < n; v++) {
        if (mis->in_mis[v]) continue;
        AdjList* al = &g->adjacency[v];
        for (int i = 0; i < al->degree; i++) {
            int nb = al->neighbors[i];
            if (mis->in_mis[nb]) { mis->dominator[v] = nb; break; }
        }
    }
    free(active);
    return mis;
}

MIS* mis_luby_phased(const Graph* g, uint64_t seed) {
    return mis_luby_randomized(g, seed);
}

MIS* mis_luby_discrete(const Graph* g, int K, uint64_t seed) {
    (void)K;
    return mis_luby_randomized(g, seed);
}
