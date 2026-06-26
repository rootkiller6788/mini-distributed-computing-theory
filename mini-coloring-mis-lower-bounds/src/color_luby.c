#include "coloring.h"
#include "distributed_constants.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

extern uint64_t lcg_random_next(uint64_t* state);
extern double lcg_random_double(uint64_t* state);

Coloring* luby_style_coloring(const Graph* g, uint64_t seed) {
    if (!g) return NULL;
    int n = g->n, Delta = g->max_degree, K = Delta + 1;
    Coloring* c = coloring_create(n);
    if (!c) return NULL;
    c->num_colors = K;
    bool* unc = (bool*)malloc((size_t)n * sizeof(bool));
    bool* act = (bool*)malloc((size_t)n * sizeof(bool));
    if (!unc || !act) { free(unc); free(act); coloring_free(c); return NULL; }
    for (int v = 0; v < n; v++) { unc[v] = true; c->colors[v] = INVALID_COLOR; }
    uint64_t state = seed;
    int rem = n;
    for (int color = 0; color < K && rem > 0; color++) {
        for (int v = 0; v < n; v++) act[v] = unc[v];
        int kg = 1, mr = 0;
        while (kg && mr < 100) {
            kg = 0;
            double* ranks = (double*)malloc((size_t)n * sizeof(double));
            if (!ranks) break;
            for (int v = 0; v < n; v++)
                ranks[v] = act[v] ? lcg_random_double(&state) : -1.0;
            for (int v = 0; v < n; v++) {
                if (!act[v]) continue;
                int lmax = 1;
                AdjList* al = &g->adjacency[v];
                for (int i = 0; i < al->degree; i++) {
                    int nb = al->neighbors[i];
                    if (act[nb] && ranks[nb] > ranks[v]) { lmax = 0; break; }
                }
                if (lmax) {
                    c->colors[v] = color; unc[v] = false;
                    act[v] = false; rem--;
                    for (int i = 0; i < al->degree; i++)
                        act[al->neighbors[i]] = false;
                }
            }
            free(ranks);
            for (int v = 0; v < n; v++)
                if (act[v]) { kg = 1; break; }
            mr++;
        }
    }
    free(unc); free(act);
    return c;
}
