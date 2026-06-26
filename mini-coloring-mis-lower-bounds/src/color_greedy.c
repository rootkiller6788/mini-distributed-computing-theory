#include "coloring.h"
#include "distributed_constants.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

Coloring* coloring_greedy_sequential(const Graph* g, const int* order) {
    if (!g) return NULL;
    int n = g->n;
    Coloring* c = coloring_create(n);
    if (!c) return NULL;
    int* proc = (int*)malloc((size_t)n * sizeof(int));
    if (!proc) { coloring_free(c); return NULL; }
    if (order) {
        for (int i = 0; i < n; i++) proc[i] = order[i];
    } else {
        for (int i = 0; i < n; i++) proc[i] = i;
    }
    for (int idx = 0; idx < n; idx++) {
        int v = proc[idx];
        AdjList* al = &g->adjacency[v];
        bool* used = (bool*)calloc((size_t)(al->degree + 2), sizeof(bool));
        if (!used) continue;
        for (int i = 0; i < al->degree; i++) {
            int nb = al->neighbors[i];
            if (c->colors[nb] >= 0 && c->colors[nb] < al->degree + 2)
                used[c->colors[nb]] = true;
        }
        int col = 0;
        while (used[col]) col++;
        c->colors[v] = col;
        if (col + 1 > c->num_colors) c->num_colors = col + 1;
        free(used);
    }
    free(proc);
    return c;
}
