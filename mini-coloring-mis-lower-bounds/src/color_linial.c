#include "coloring.h"
#include "distributed_constants.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int linial_bound(int K, int Delta) {
    if (Delta <= 0) return K;
    for (int t = Delta + 1; t <= K; t++) {
        double binom = 1.0;
        for (int i = 1; i <= Delta; i++)
            binom *= (double)(t - 1 - Delta + i) / (double)i;
        if (binom >= 1e15) binom = 1e15;
        if ((double)t * binom >= (double)K) return t;
    }
    return K;
}

Coloring* linial_one_round_reduction(const Graph* g, const Coloring* input,
                                      int max_degree) {
    if (!g || !input || g->n != input->n) return NULL;
    int n = g->n;
    int K = coloring_num_colors_used(input);
    int new_K = linial_bound(K, max_degree);
    if (new_K <= 0) new_K = K;
    Coloring* out = coloring_create(n);
    if (!out) return NULL;
    for (int v = 0; v < n; v++) {
        if (input->colors[v] < 0) { out->colors[v] = INVALID_COLOR; continue; }
        AdjList* al = &g->adjacency[v];
        int cap = al->degree + 1;
        int* nb_c = (int*)malloc((size_t)cap * sizeof(int));
        if (!nb_c) continue;
        int cnt = 0;
        for (int i = 0; i < al->degree; i++) {
            int nb = al->neighbors[i];
            if (input->colors[nb] >= 0) nb_c[cnt++] = input->colors[nb];
        }
        for (int i = 0; i < cnt; i++)
            for (int j = i + 1; j < cnt; j++)
                if (nb_c[i] > nb_c[j]) {
                    int t = nb_c[i]; nb_c[i] = nb_c[j]; nb_c[j] = t;
                }
        unsigned long long hash = (unsigned long long)input->colors[v];
        for (int i = 0; i < cnt; i++)
            hash = hash * 6364136223846793005ULL + (unsigned long long)(nb_c[i] + 1);
        out->colors[v] = (int)(hash % (unsigned long long)new_K);
        free(nb_c);
    }
    out->num_colors = new_K;
    return out;
}

ExecutionTrace* linial_full_algorithm(const Graph* g) {
    if (!g) return NULL;
    ExecutionTrace* trace = trace_create(g->n, 100);
    if (!trace) return NULL;
    Coloring* cur = coloring_create(g->n);
    if (!cur) { trace_free(trace); return NULL; }
    for (int v = 0; v < g->n; v++) cur->colors[v] = v;
    cur->num_colors = g->n;
    int max_d = g->max_degree;
    int target = max_d * max_d + 1;
    int round = 0;
    while (cur->num_colors > target && round < 100) {
        int* snap = (int*)malloc((size_t)g->n * sizeof(int));
        if (snap) {
            for (int i = 0; i < g->n; i++) snap[i] = cur->colors[i];
            trace_record_round(trace, round, snap, NULL, NULL);
            free(snap);
        }
        Coloring* nxt = linial_one_round_reduction(g, cur, max_d);
        if (!nxt || nxt->num_colors >= cur->num_colors) {
            if (nxt) coloring_free(nxt); break;
        }
        coloring_free(cur); cur = nxt; round++;
    }
    coloring_free(cur);
    return trace;
}
