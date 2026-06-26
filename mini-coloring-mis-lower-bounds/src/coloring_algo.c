#include "coloring.h"
#include "distributed_constants.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

static uint64_t lcg_next(uint64_t* state) {
    *state = (*state * 6364136223846793005ULL + 1442695040888963407ULL);
    return *state;
}

/* === Greedy Sequential Coloring === */
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

typedef struct { int v; int d; } Vdeg;
static int cmp_vdeg_desc(const void* a, const void* b) {
    return ((const Vdeg*)b)->d - ((const Vdeg*)a)->d;
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
    Vdeg* vd = (Vdeg*)malloc((size_t)n * sizeof(Vdeg));
    if (!vd) return NULL;
    for (int i = 0; i < n; i++) { vd[i].v = i; vd[i].d = g->adjacency[i].degree; }
    qsort(vd, (size_t)n, sizeof(Vdeg), cmp_vdeg_desc);
    int* order = (int*)malloc((size_t)n * sizeof(int));
    if (order) for (int i = 0; i < n; i++) order[i] = vd[i].v;
    free(vd);
    return order;
}

/* === Linial Color Reduction === */
static int linial_color_bound(int K, int Delta) {
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
    int new_K = linial_color_bound(K, max_degree);
    if (new_K <= 0) new_K = K;
    Coloring* output = coloring_create(n);
    if (!output) return NULL;
    for (int v = 0; v < n; v++) {
        if (input->colors[v] < 0) { output->colors[v] = INVALID_COLOR; continue; }
        AdjList* al = &g->adjacency[v];
        int* nb_colors = (int*)malloc((size_t)(al->degree + 1) * sizeof(int));
        if (!nb_colors) continue;
        int cnt = 0;
        for (int i = 0; i < al->degree; i++) {
            int nb = al->neighbors[i];
            if (input->colors[nb] >= 0) nb_colors[cnt++] = input->colors[nb];
        }
        for (int i = 0; i < cnt; i++)
            for (int j = i + 1; j < cnt; j++)
                if (nb_colors[i] > nb_colors[j]) {
                    int t = nb_colors[i]; nb_colors[i] = nb_colors[j]; nb_colors[j] = t;
                }
        unsigned long long hash = (unsigned long long)input->colors[v];
        for (int i = 0; i < cnt; i++)
            hash = hash * 6364136223846793005ULL + (unsigned long long)(nb_colors[i] + 1);
        output->colors[v] = (int)(hash % (unsigned long long)new_K);
        free(nb_colors);
    }
    output->num_colors = new_K;
    return output;
}

ExecutionTrace* linial_full_algorithm(const Graph* g) {
    if (!g) return NULL;
    ExecutionTrace* trace = trace_create(g->n, 100);
    if (!trace) return NULL;
    Coloring* current = coloring_create(g->n);
    if (!current) { trace_free(trace); return NULL; }
    for (int v = 0; v < g->n; v++) current->colors[v] = v;
    current->num_colors = g->n;
    int max_d = g->max_degree;
    int target = max_d * max_d + 1;
    int round = 0;
    while (current->num_colors > target && round < 100) {
        int* snap = (int*)malloc((size_t)g->n * sizeof(int));
        if (snap) {
            for (int i = 0; i < g->n; i++) snap[i] = current->colors[i];
            trace_record_round(trace, round, snap, NULL, NULL);
            free(snap);
        }
        Coloring* next = linial_one_round_reduction(g, current, max_d);
        if (!next || next->num_colors >= current->num_colors) {
            if (next) coloring_free(next); break;
        }
        coloring_free(current); current = next; round++;
    }
    coloring_free(current);
    return trace;
}
