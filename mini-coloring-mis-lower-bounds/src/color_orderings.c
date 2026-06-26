#include "coloring.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct { int v; int d; } DegVertex;

static int cmp_deg_desc(const void* a, const void* b) {
    return ((const DegVertex*)b)->d - ((const DegVertex*)a)->d;
}

int* coloring_smallest_last_order(const Graph* g) {
    if (!g) return NULL;
    int n = g->n;
    int* order = (int*)malloc((size_t)n * sizeof(int));
    int* deg = (int*)malloc((size_t)n * sizeof(int));
    bool* rm = (bool*)calloc((size_t)n, sizeof(bool));
    if (!order || !deg || !rm) { free(order); free(deg); free(rm); return NULL; }
    for (int i = 0; i < n; i++) deg[i] = g->adjacency[i].degree;
    int pos = n - 1;
    while (pos >= 0) {
        int min_v = -1, min_d = n + 1;
        for (int v = 0; v < n; v++) {
            if (!rm[v] && deg[v] < min_d) { min_d = deg[v]; min_v = v; }
        }
        if (min_v < 0) break;
        order[pos--] = min_v;
        rm[min_v] = true;
        AdjList* al = &g->adjacency[min_v];
        for (int i = 0; i < al->degree; i++) {
            int nb = al->neighbors[i];
            if (!rm[nb] && deg[nb] > 0) deg[nb]--;
        }
    }
    free(deg); free(rm);
    return order;
}

int* coloring_largest_first_order(const Graph* g) {
    if (!g) return NULL;
    int n = g->n;
    DegVertex* vd = (DegVertex*)malloc((size_t)n * sizeof(DegVertex));
    if (!vd) return NULL;
    for (int i = 0; i < n; i++) { vd[i].v = i; vd[i].d = g->adjacency[i].degree; }
    qsort(vd, (size_t)n, sizeof(DegVertex), cmp_deg_desc);
    int* order = (int*)malloc((size_t)n * sizeof(int));
    if (order) for (int i = 0; i < n; i++) order[i] = vd[i].v;
    free(vd);
    return order;
}
