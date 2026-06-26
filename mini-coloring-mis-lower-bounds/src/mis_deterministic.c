#include "mis.h"
#include "distributed_constants.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

MIS* mis_linial_deterministic(const Graph* g) {
    if (!g) return NULL;
    int n = g->n;
    /* First get an O(Delta^2) coloring, then process color classes */
    Coloring* col = cole_vishkin_coloring(g);
    if (!col) return NULL;
    MIS* mis = mis_create(n);
    if (!mis) { coloring_free(col); return NULL; }
    int num_c = col->num_colors;
    for (int c = 0; c < num_c; c++) {
        for (int v = 0; v < n; v++) {
            if (col->colors[v] != c) continue;
            int blocked = 0;
            AdjList* al = &g->adjacency[v];
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
    }
    coloring_free(col);
    return mis;
}

MIS* mis_ring_deterministic(int n, const int* ids) {
    if (n < 3 || !ids) return NULL;
    MIS* mis = mis_create(n);
    if (!mis) return NULL;
    for (int r = 0; r < n; r++) {
        for (int v = 0; v < n; v++) {
            if (mis->in_mis[v]) continue;
            int l = (v - 1 + n) % n, ri = (v + 1) % n;
            int has_mis_nb = mis->in_mis[l] || mis->in_mis[ri];
            if (has_mis_nb) continue;
            /* Local minimum ID joins MIS */
            if (ids[v] < ids[l] && ids[v] < ids[ri]) {
                mis->in_mis[v] = true;
                mis->mis_size++;
                mis->dominator[l] = v;
                mis->dominator[ri] = v;
            }
        }
    }
    return mis;
}

MIS* mis_network_decomposition(const Graph* g, int cluster_diameter) {
    (void)cluster_diameter;
    return mis_linial_deterministic(g);
}
