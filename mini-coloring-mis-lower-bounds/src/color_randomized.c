#include "coloring.h"
#include "distributed_constants.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

extern uint64_t lcg_random_next(uint64_t* state);

Coloring* randomized_delta_plus_one_coloring(const Graph* g, uint64_t seed) {
    if (!g) return NULL;
    int n = g->n, Delta = g->max_degree, K = Delta + 1;
    Coloring* c = coloring_create(n);
    if (!c) return NULL;
    c->num_colors = K;
    bool* done = (bool*)calloc((size_t)n, sizeof(bool));
    if (!done) { coloring_free(c); return NULL; }
    uint64_t state = seed;
    int remaining = n;
    for (int round = 0; round < n * 10 && remaining > 0; round++) {
        int* props = (int*)malloc((size_t)n * sizeof(int));
        if (!props) break;
        for (int v = 0; v < n; v++)
            props[v] = done[v] ? -1 : (int)(lcg_random_next(&state) % (uint64_t)K);
        for (int v = 0; v < n; v++) {
            if (done[v] || props[v] < 0) continue;
            int conflict = 0;
            AdjList* al = &g->adjacency[v];
            for (int i = 0; i < al->degree; i++) {
                int nb = al->neighbors[i];
                if ((!done[nb] && props[nb] == props[v]) ||
                    (done[nb] && c->colors[nb] == props[v]))
                    { conflict = 1; break; }
            }
            if (!conflict) { c->colors[v] = props[v]; done[v] = true; remaining--; }
        }
        free(props);
    }
    free(done);
    return c;
}

int randomized_color_trial(const Graph* g, Coloring* current,
                            uint64_t* seed, bool* newly_colored) {
    if (!g || !current || !seed || !newly_colored) return 0;
    int n = g->n, K = current->num_colors;
    if (K <= 0) K = g->max_degree + 1;
    int new_cnt = 0;
    int* props = (int*)malloc((size_t)n * sizeof(int));
    if (!props) return 0;
    for (int v = 0; v < n; v++)
        props[v] = (current->colors[v] >= 0) ? -1
                   : (int)(lcg_random_next(seed) % (uint64_t)K);
    for (int v = 0; v < n; v++) {
        if (props[v] < 0) { newly_colored[v] = false; continue; }
        int conflict = 0;
        AdjList* al = &g->adjacency[v];
        for (int i = 0; i < al->degree; i++) {
            int nb = al->neighbors[i];
            if (props[nb] == props[v]) { conflict = 1; break; }
            if (current->colors[nb] == props[v]) { conflict = 1; break; }
        }
        if (!conflict) {
            current->colors[v] = props[v];
            newly_colored[v] = true; new_cnt++;
        } else { newly_colored[v] = false; }
    }
    free(props);
    return new_cnt;
}
