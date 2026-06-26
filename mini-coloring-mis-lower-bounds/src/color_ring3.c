#include "coloring.h"
#include "distributed_constants.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

Coloring* ring_3coloring_cole_vishkin(int n, const int* ids) {
    if (n < 3 || !ids) return NULL;
    int mr = log_star(n) + 5;
    int* cur = (int*)malloc((size_t)n * sizeof(int));
    if (!cur) return NULL;
    for (int v = 0; v < n; v++) cur[v] = ids[v];
    for (int r = 0; r < mr; r++) {
        int mx = cur[0];
        for (int v = 1; v < n; v++) if (cur[v] > mx) mx = cur[v];
        if (mx < 6) break;
        int* nxt = (int*)malloc((size_t)n * sizeof(int));
        if (!nxt) { free(cur); return NULL; }
        for (int v = 0; v < n; v++) {
            int l = (v - 1 + n) % n, ri = (v + 1) % n;
            int mv = cur[v], lv = cur[l], rv = cur[ri];
            int bits = mx > 0 ? (int)(log2((double)mx)) + 1 : 1;
            int bp = 0;
            for (; bp < bits; bp++) {
                int mb = (mv >> bp) & 1, lb = (lv >> bp) & 1, rb = (rv >> bp) & 1;
                if (mb != lb || mb != rb) break;
            }
            nxt[v] = (bp << 1) | ((mv >> bp) & 1);
        }
        free(cur); cur = nxt;
    }
    Coloring* c = coloring_create(n);
    if (!c) { free(cur); return NULL; }
    for (int v = 0; v < n; v++) {
        int l = (v - 1 + n) % n, ri = (v + 1) % n;
        int prop = cur[v] % 3;
        if (c->colors[l] >= 0 && c->colors[l] == prop) prop = (prop + 1) % 3;
        if (c->colors[l] >= 0 && c->colors[l] == prop) prop = (prop + 1) % 3;
        if (c->colors[ri] >= 0 && c->colors[ri] == prop) prop = (prop + 1) % 3;
        c->colors[v] = prop;
    }
    c->num_colors = 3;
    free(cur);
    return c;
}

int* ring_3coloring_lower_bound_ids(int n, int target_rounds) {
    if (n < 3) return NULL;
    int* ids = (int*)malloc((size_t)n * sizeof(int));
    if (!ids) return NULL;
    int bit = 1 << target_rounds;
    for (int v = 0; v < n; v++) ids[v] = (bit << v) | v;
    return ids;
}
