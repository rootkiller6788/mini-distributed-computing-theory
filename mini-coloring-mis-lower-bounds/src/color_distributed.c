#include "coloring.h"
#include "distributed_constants.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

Coloring* distributed_greedy_coloring(const Graph* g, const LocalConfig* cfg) {
    if (!g) return NULL;
    int n = g->n, Delta = g->max_degree;
    (void)cfg;
    Coloring* c = coloring_create(n);
    if (!c) return NULL;
    bool* done = (bool*)calloc((size_t)n, sizeof(bool));
    if (!done) { coloring_free(c); return NULL; }
    int rem = n;
    for (int r = 0; r < n * (Delta + 1) && rem > 0; r++) {
        for (int v = 0; v < n; v++) {
            if (done[v]) continue;
            int can = 1;
            AdjList* al = &g->adjacency[v];
            for (int i = 0; i < al->degree; i++)
                if (!done[al->neighbors[i]] && al->neighbors[i] < v)
                    { can = 0; break; }
            if (!can) continue;
            bool* used = (bool*)calloc((size_t)(Delta + 2), sizeof(bool));
            if (!used) continue;
            for (int i = 0; i < al->degree; i++) {
                int nb = al->neighbors[i];
                if (c->colors[nb] >= 0 && c->colors[nb] <= Delta + 1)
                    used[c->colors[nb]] = true;
            }
            int col = 0;
            while (used[col]) col++;
            c->colors[v] = col;
            if (col + 1 > c->num_colors) c->num_colors = col + 1;
            done[v] = true; rem--;
            free(used);
        }
    }
    free(done);
    return c;
}

bool graph_is_planar(const Graph* g) {
    if (!g) return false;
    if (g->n >= 3 && g->m > 3 * g->n - 6) return false;
    return true;
}

Coloring* planar_6coloring(const Graph* g) {
    if (!g || !graph_is_planar(g)) return NULL;
    int n = g->n;
    int* order = coloring_smallest_last_order(g);
    if (!order) return NULL;
    Coloring* c = coloring_create(n);
    if (!c) { free(order); return NULL; }
    for (int idx = n - 1; idx >= 0; idx--) {
        int v = order[idx];
        bool* used = (bool*)calloc(7, sizeof(bool));
        if (!used) continue;
        AdjList* al = &g->adjacency[v];
        for (int i = 0; i < al->degree; i++) {
            int nb = al->neighbors[i];
            if (c->colors[nb] >= 0 && c->colors[nb] < 6) used[c->colors[nb]] = true;
        }
        int col = 0;
        while (used[col] && col < 6) col++;
        c->colors[v] = col;
        free(used);
    }
    c->num_colors = 6;
    free(order);
    return c;
}
