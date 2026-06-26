#include "mis.h"
#include "distributed_constants.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

extern uint64_t lcg_random_next(uint64_t* state);

/* Greedy sequential MIS */
MIS* mis_greedy_sequential(const Graph* g, const int* order) {
    if (!g) return NULL;
    int n = g->n;
    MIS* mis = mis_create(n);
    if (!mis) return NULL;
    int* proc = (int*)malloc((size_t)n * sizeof(int));
    if (!proc) { mis_free(mis); return NULL; }
    if (order) { for (int i = 0; i < n; i++) proc[i] = order[i]; }
    else { for (int i = 0; i < n; i++) proc[i] = i; }
    for (int idx = 0; idx < n; idx++) {
        int v = proc[idx];
        AdjList* al = &g->adjacency[v];
        int blocked = 0;
        for (int i = 0; i < al->degree; i++)
            if (mis->in_mis[al->neighbors[i]]) { blocked = 1; break; }
        if (!blocked) {
            mis->in_mis[v] = true;
            mis->mis_size++;
            for (int i = 0; i < al->degree; i++)
                if (mis->dominator[al->neighbors[i]] < 0)
                    mis->dominator[al->neighbors[i]] = v;
        }
    }
    free(proc);
    return mis;
}

/* Random-order greedy MIS */
MIS* mis_greedy_random_order(const Graph* g, uint64_t seed) {
    if (!g) return NULL;
    int n = g->n;
    int* order = (int*)malloc((size_t)n * sizeof(int));
    if (!order) return NULL;
    for (int i = 0; i < n; i++) order[i] = i;
    uint64_t st = seed;
    for (int i = n - 1; i > 0; i--) {
        int j = (int)(lcg_random_next(&st) % ((uint64_t)i + 1));
        int t = order[i]; order[i] = order[j]; order[j] = t;
    }
    MIS* mis = mis_greedy_sequential(g, order);
    free(order);
    return mis;
}

/* Min-degree greedy MIS */
MIS* mis_greedy_min_degree(const Graph* g) {
    if (!g) return NULL;
    int n = g->n;
    typedef struct { int v; int d; } VD;
    VD* vd = (VD*)malloc((size_t)n * sizeof(VD));
    if (!vd) return NULL;
    for (int i = 0; i < n; i++) { vd[i].v = i; vd[i].d = g->adjacency[i].degree; }
    /* Sort by degree ascending */
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (vd[i].d > vd[j].d) { VD t = vd[i]; vd[i] = vd[j]; vd[j] = t; }
    int* order = (int*)malloc((size_t)n * sizeof(int));
    if (order) for (int i = 0; i < n; i++) order[i] = vd[i].v;
    free(vd);
    MIS* mis = mis_greedy_sequential(g, order);
    free(order);
    return mis;
}
