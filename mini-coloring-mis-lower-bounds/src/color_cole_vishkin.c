#include "coloring.h"
#include "distributed_constants.h"
#include <stdlib.h>
#include <string.h>

Coloring* cole_vishkin_reduce(const Graph* g, const Coloring* input,
                               int target_colors) {
    if (!g || !input) return NULL;
    int n = g->n;
    Coloring* cur = coloring_create(n);
    if (!cur) return NULL;
    for (int i = 0; i < n; i++) cur->colors[i] = input->colors[i];
    cur->num_colors = input->num_colors;
    for (int r = 0; r < 100; r++) {
        if (cur->num_colors <= target_colors) break;
        Coloring* nxt = linial_one_round_reduction(g, cur, g->max_degree);
        if (!nxt || nxt->num_colors >= cur->num_colors) {
            if (nxt) coloring_free(nxt); break;
        }
        coloring_free(cur); cur = nxt;
    }
    return cur;
}

Coloring* cole_vishkin_coloring(const Graph* g) {
    if (!g) return NULL;
    int n = g->n, Delta = g->max_degree;
    Coloring* init = coloring_create(n);
    if (!init) return NULL;
    for (int v = 0; v < n; v++) init->colors[v] = v;
    init->num_colors = n;
    int target = Delta * Delta + 1;
    if (target > n) target = n;
    Coloring* mid = cole_vishkin_reduce(g, init, target);
    coloring_free(init);
    if (!mid) return NULL;
    int cur_c = mid->num_colors;
    while (cur_c > Delta + 1) {
        coloring_reduce_by_one(g, mid, cur_c - 1);
        cur_c = coloring_num_colors_used(mid);
    }
    return mid;
}

bool coloring_reduce_by_one(const Graph* g, Coloring* c, int rm_col) {
    if (!g || !c) return false;
    int n = g->n;
    int* reco = (int*)malloc((size_t)n * sizeof(int));
    int rcnt = 0;
    if (!reco) return false;
    for (int v = 0; v < n; v++)
        if (c->colors[v] == rm_col) { reco[rcnt++] = v; c->colors[v] = INVALID_COLOR; }
    for (int i = 0; i < rcnt; i++) {
        int v = reco[i];
        AdjList* al = &g->adjacency[v];
        bool* used = (bool*)calloc((size_t)rm_col, sizeof(bool));
        if (!used) continue;
        for (int j = 0; j < al->degree; j++) {
            int nb = al->neighbors[j];
            if (c->colors[nb] >= 0 && c->colors[nb] < rm_col)
                used[c->colors[nb]] = true;
        }
        int nc = 0;
        while (nc < rm_col && used[nc]) nc++;
        c->colors[v] = (nc < rm_col) ? nc : 0;
        free(used);
    }
    free(reco);
    c->num_colors = coloring_num_colors_used(c);
    return true;
}
