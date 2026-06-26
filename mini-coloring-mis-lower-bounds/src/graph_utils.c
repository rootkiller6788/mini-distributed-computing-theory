/*
 * graph_utils.c -- Graph Construction, Neighborhood Operations & Analysis
 *
 * L1 Definitions: Graph, degree, diameter, independence number
 * L3 Math Structures: adjacency lists, BFS trees, distance-k views
 * L6 Canonical Problems: graph construction for lower bounds
 *
 * Reference: Peleg (2000), Barenboim-Elkin (2013)
 */

#include "graph.h"
#include "distributed_constants.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

/* ── Graph Construction ────────────────────────────────────── */

Graph* graph_create(int n) {
    if (n <= 0) return NULL;
    Graph* g = (Graph*)calloc(1, sizeof(Graph));
    if (!g) return NULL;
    g->n = n;
    g->m = 0;
    g->max_degree = 0;
    g->adjacency = (AdjList*)calloc((size_t)n, sizeof(AdjList));
    if (!g->adjacency) { free(g); return NULL; }
    return g;
}

void graph_add_edge(Graph* g, int u, int v) {
    if (!g || u < 0 || v < 0 || u >= g->n || v >= g->n || u == v) return;

    /* Check for duplicate */
    for (int i = 0; i < g->adjacency[u].degree; i++) {
        if (g->adjacency[u].neighbors[i] == v) return;
    }

    /* Add v to u's adjacency */
    AdjList* al_u = &g->adjacency[u];
    if (al_u->degree >= al_u->capacity) {
        al_u->capacity = al_u->capacity == 0 ? 4 : al_u->capacity * 2;
        al_u->neighbors = (int*)realloc(al_u->neighbors,
                           (size_t)al_u->capacity * sizeof(int));
    }
    al_u->neighbors[al_u->degree++] = v;

    /* Add u to v's adjacency */
    AdjList* al_v = &g->adjacency[v];
    if (al_v->degree >= al_v->capacity) {
        al_v->capacity = al_v->capacity == 0 ? 4 : al_v->capacity * 2;
        al_v->neighbors = (int*)realloc(al_v->neighbors,
                           (size_t)al_v->capacity * sizeof(int));
    }
    al_v->neighbors[al_v->degree++] = u;

    g->m++;
    if (al_u->degree > g->max_degree) g->max_degree = al_u->degree;
    if (al_v->degree > g->max_degree) g->max_degree = al_v->degree;
}

void graph_free(Graph* g) {
    if (!g) return;
    if (g->adjacency) {
        for (int i = 0; i < g->n; i++) {
            free(g->adjacency[i].neighbors);
        }
        free(g->adjacency);
    }
    free(g);
}

Graph* graph_create_ring(int n) {
    if (n < 3) return NULL;
    Graph* g = graph_create(n);
    if (!g) return NULL;
    for (int i = 0; i < n; i++) {
        graph_add_edge(g, i, (i + 1) % n);
    }
    return g;
}

Graph* graph_create_complete(int n) {
    if (n < 1) return NULL;
    Graph* g = graph_create(n);
    if (!g) return NULL;
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            graph_add_edge(g, i, j);
        }
    }
    return g;
}

Graph* graph_create_tree(const int* parent, int n) {
    if (!parent || n < 1) return NULL;
    Graph* g = graph_create(n);
    if (!g) return NULL;
    for (int i = 1; i < n; i++) {
        int p = parent[i];
        if (p >= 0 && p < n) {
            graph_add_edge(g, i, p);
        }
    }
    return g;
}

/* Simple LCG pseudo-random for deterministic reproducibility */
static uint64_t lcg_next(uint64_t* state) {
    *state = (*state * 6364136223846793005ULL + 1442695040888963407ULL);
    return *state;
}

Graph* graph_create_random_regular(int n, int d, uint64_t seed) {
    if (n < 1 || d < 0 || d >= n) return NULL;
    if (d % 2 != 0 && n % 2 != 0) {
        /* d-regular graph impossible with odd n and odd d */
        return NULL;
    }
    if (d == 0) return graph_create(n);

    Graph* g = graph_create(n);
    if (!g) return NULL;

    /* Use pairing model: create d stubs per node, randomly pair */
    int total_stubs = n * d;
    int* stubs = (int*)malloc((size_t)total_stubs * sizeof(int));
    if (!stubs) { graph_free(g); return NULL; }

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < d; j++) {
            stubs[i * d + j] = i;
        }
    }

    /* Fisher-Yates shuffle on stubs */
    uint64_t state = seed;
    for (int i = total_stubs - 1; i > 0; i--) {
        int j = (int)(lcg_next(&state) % ((uint64_t)i + 1));
        int tmp = stubs[i];
        stubs[i] = stubs[j];
        stubs[j] = tmp;
    }

    /* Pair adjacent stubs */
    for (int i = 0; i < total_stubs; i += 2) {
        int u = stubs[i];
        int v = stubs[i + 1];
        graph_add_edge(g, u, v);
    }

    free(stubs);
    return g;
}

Graph* graph_create_bipartite(int a, int b) {
    if (a < 1 || b < 1) return NULL;
    Graph* g = graph_create(a + b);
    if (!g) return NULL;
    for (int i = 0; i < a; i++) {
        for (int j = 0; j < b; j++) {
            graph_add_edge(g, i, a + j);
        }
    }
    return g;
}

Graph* graph_create_grid(int rows, int cols) {
    if (rows < 1 || cols < 1) return NULL;
    int n = rows * cols;
    Graph* g = graph_create(n);
    if (!g) return NULL;

#define GRID_IDX(r, c) ((r) * cols + (c))

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            if (c + 1 < cols) graph_add_edge(g, GRID_IDX(r, c), GRID_IDX(r, c + 1));
            if (r + 1 < rows) graph_add_edge(g, GRID_IDX(r, c), GRID_IDX(r + 1, c));
        }
    }
    return g;
}

Graph* graph_create_bounded_degree(int n, int d, uint64_t seed) {
    if (n < 1 || d < 1) return NULL;
    Graph* g = graph_create(n);
    if (!g) return NULL;
    uint64_t state = seed;
    for (int i = 0; i < n; i++) {
        int attempts = 0;
        while (g->adjacency[i].degree < d && attempts < 2 * d) {
            int j = (int)(lcg_next(&state) % (uint64_t)n);
            if (j != i && g->adjacency[j].degree < d) {
                graph_add_edge(g, i, j);
            }
            attempts++;
        }
    }
    return g;
}

void graph_print_stats(const Graph* g) {
    if (!g) { printf("Graph: NULL\n"); return; }
    printf("Graph: n=%d, m=%d, Delta=%d\n", g->n, g->m, g->max_degree);
    int* deg_dist = (int*)calloc((size_t)(g->max_degree + 1), sizeof(int));
    if (deg_dist) {
        for (int i = 0; i < g->n; i++) {
            if (g->adjacency[i].degree <= g->max_degree)
                deg_dist[g->adjacency[i].degree]++;
        }
        printf("Degree distribution:\n");
        for (int d = 0; d <= g->max_degree; d++) {
            if (deg_dist[d] > 0)
                printf("  deg %d: %d vertices\n", d, deg_dist[d]);
        }
        free(deg_dist);
    }
}

/* ── Neighborhood Operations ────────────────────────────────── */

DistanceKView* neighborhood_view(const Graph* g, int v, int k) {
    if (!g || v < 0 || v >= g->n || k < 0) return NULL;

    DistanceKView* view = (DistanceKView*)calloc(1, sizeof(DistanceKView));
    if (!view) return NULL;

    view->nodes = (int*)malloc((size_t)g->n * sizeof(int));
    view->distances = (int*)malloc((size_t)g->n * sizeof(int));
    view->parents = (int*)malloc((size_t)g->n * sizeof(int));
    if (!view->nodes || !view->distances || !view->parents) {
        neighborhood_view_free(view); return NULL;
    }

    /* BFS from v */
    int* queue = (int*)malloc((size_t)g->n * sizeof(int));
    if (!queue) { neighborhood_view_free(view); return NULL; }

    for (int i = 0; i < g->n; i++) {
        view->distances[i] = -1;
        view->parents[i] = -1;
    }

    int head = 0, tail = 0;
    queue[tail++] = v;
    view->distances[v] = 0;

    while (head < tail) {
        int u = queue[head++];
        int du = view->distances[u];
        if (du >= k) continue;

        AdjList* al = &g->adjacency[u];
        for (int i = 0; i < al->degree; i++) {
            int w = al->neighbors[i];
            if (view->distances[w] < 0) {
                view->distances[w] = du + 1;
                view->parents[w] = u;
                queue[tail++] = w;
            }
        }
    }

    /* Collect nodes within distance k */
    int count = 0;
    for (int i = 0; i < g->n; i++) {
        if (view->distances[i] >= 0 && view->distances[i] <= k) {
            view->nodes[count++] = i;
        }
    }
    view->size = count;
    view->max_dist = k;

    free(queue);
    return view;
}

void neighborhood_view_free(DistanceKView* view) {
    if (!view) return;
    free(view->nodes);
    free(view->distances);
    free(view->parents);
    free(view);
}

bool neighborhood_views_isomorphic(const DistanceKView* a,
                                    const DistanceKView* b) {
    if (!a || !b) return false;
    if (a->size != b->size || a->max_dist != b->max_dist) return false;

    /* Simple check: sorted distance multiset */
    int* da = (int*)malloc((size_t)a->size * sizeof(int));
    int* db = (int*)malloc((size_t)b->size * sizeof(int));
    if (!da || !db) { free(da); free(db); return false; }

    for (int i = 0; i < a->size; i++) {
        da[i] = a->distances[a->nodes[i]];
        db[i] = b->distances[b->nodes[i]];
    }

    /* Sort both */
    /* qsort comparison helper */
    {
        int cmp_int(const void* x, const void* y) {
            return *(const int*)x - *(const int*)y;
        }
        qsort(da, (size_t)a->size, sizeof(int), cmp_int);
        qsort(db, (size_t)b->size, sizeof(int), cmp_int);
    }

    bool iso = true;
    for (int i = 0; i < a->size; i++) {
        if (da[i] != db[i]) { iso = false; break; }
    }

    free(da); free(db);
    return iso;
}

int max_neighborhood_size(const Graph* g, int k) {
    if (!g || k < 0) return 0;
    int max_sz = 0;
    for (int v = 0; v < g->n; v++) {
        DistanceKView* view = neighborhood_view(g, v, k);
        if (view && view->size > max_sz) max_sz = view->size;
        neighborhood_view_free(view);
    }
    return max_sz;
}

/* ── Coloring Operations ────────────────────────────────────── */

Coloring* coloring_create(int n) {
    if (n <= 0) return NULL;
    Coloring* c = (Coloring*)calloc(1, sizeof(Coloring));
    if (!c) return NULL;
    c->n = n;
    c->num_colors = 0;
    c->colors = (int*)malloc((size_t)n * sizeof(int));
    if (!c->colors) { free(c); return NULL; }
    for (int i = 0; i < n; i++) c->colors[i] = INVALID_COLOR;
    return c;
}

void coloring_free(Coloring* c) {
    if (!c) return;
    free(c->colors);
    free(c);
}

bool coloring_is_proper(const Graph* g, const Coloring* c) {
    if (!g || !c || g->n != c->n) return false;
    for (int u = 0; u < g->n; u++) {
        if (c->colors[u] < 0) continue; /* uncolored OK */
        AdjList* al = &g->adjacency[u];
        for (int i = 0; i < al->degree; i++) {
            int v = al->neighbors[i];
            if (c->colors[v] >= 0 && c->colors[u] == c->colors[v])
                return false;
        }
    }
    return true;
}

int coloring_num_colors_used(const Coloring* c) {
    if (!c) return 0;
    int max_c = -1;
    for (int i = 0; i < c->n; i++) {
        if (c->colors[i] > max_c) max_c = c->colors[i];
    }
    return max_c + 1;
}

/* ── MIS Operations ─────────────────────────────────────────── */

MIS* mis_create(int n) {
    if (n <= 0) return NULL;
    MIS* mis = (MIS*)calloc(1, sizeof(MIS));
    if (!mis) return NULL;
    mis->n = n;
    mis->mis_size = 0;
    mis->in_mis = (bool*)calloc((size_t)n, sizeof(bool));
    mis->dominator = (int*)malloc((size_t)n * sizeof(int));
    if (!mis->in_mis || !mis->dominator) {
        mis_free(mis); return NULL;
    }
    for (int i = 0; i < n; i++) mis->dominator[i] = INVALID_NODE;
    return mis;
}

void mis_free(MIS* mis) {
    if (!mis) return;
    free(mis->in_mis);
    free(mis->dominator);
    free(mis);
}

bool mis_is_independent(const Graph* g, const MIS* mis) {
    if (!g || !mis || g->n != mis->n) return false;
    for (int u = 0; u < g->n; u++) {
        if (!mis->in_mis[u]) continue;
        AdjList* al = &g->adjacency[u];
        for (int i = 0; i < al->degree; i++) {
            int v = al->neighbors[i];
            if (mis->in_mis[v]) return false;
        }
    }
    return true;
}

bool mis_is_maximal(const Graph* g, const MIS* mis) {
    if (!g || !mis || g->n != mis->n) return false;
    if (!mis_is_independent(g, mis)) return false;
    for (int u = 0; u < g->n; u++) {
        if (mis->in_mis[u]) continue;
        bool has_mis_neighbor = false;
        AdjList* al = &g->adjacency[u];
        for (int i = 0; i < al->degree; i++) {
            if (mis->in_mis[al->neighbors[i]]) {
                has_mis_neighbor = true;
                break;
            }
        }
        if (!has_mis_neighbor) return false;
    }
    return true;
}

bool mis_verify(const Graph* g, const MIS* mis) {
    return mis_is_independent(g, mis) && mis_is_maximal(g, mis);
}

/* ── Port Numbering Operations ──────────────────────────────── */

PortNumbered* port_numbering_create(const Graph* g) {
    if (!g) return NULL;
    PortNumbered* pn = (PortNumbered*)calloc(1, sizeof(PortNumbered));
    if (!pn) return NULL;
    pn->n = g->n;
    pn->degree = (int*)malloc((size_t)g->n * sizeof(int));
    pn->port_map = (int**)calloc((size_t)g->n, sizeof(int*));
    pn->reverse_port = (int**)calloc((size_t)g->n, sizeof(int*));
    if (!pn->degree || !pn->port_map || !pn->reverse_port) {
        port_numbering_free(pn); return NULL;
    }
    for (int v = 0; v < g->n; v++) {
        int d = g->adjacency[v].degree;
        pn->degree[v] = d;
        pn->port_map[v] = (int*)malloc((size_t)d * sizeof(int));
        pn->reverse_port[v] = (int*)malloc((size_t)d * sizeof(int));
        if (!pn->port_map[v] || !pn->reverse_port[v]) {
            port_numbering_free(pn); return NULL;
        }
        for (int p = 0; p < d; p++) {
            int neighbor = g->adjacency[v].neighbors[p];
            pn->port_map[v][p] = neighbor;

            /* Find reverse port: which port at neighbor leads back to v? */
            pn->reverse_port[v][p] = -1;
            AdjList* nal = &g->adjacency[neighbor];
            for (int q = 0; q < nal->degree; q++) {
                if (nal->neighbors[q] == v) {
                    pn->reverse_port[v][p] = q;
                    break;
                }
            }
        }
    }
    return pn;
}

void port_numbering_free(PortNumbered* pn) {
    if (!pn) return;
    if (pn->port_map) {
        for (int i = 0; i < pn->n; i++) free(pn->port_map[i]);
        free(pn->port_map);
    }
    if (pn->reverse_port) {
        for (int i = 0; i < pn->n; i++) free(pn->reverse_port[i]);
        free(pn->reverse_port);
    }
    free(pn->degree);
    free(pn);
}

int port_to_neighbor(const PortNumbered* pn, int v, int port) {
    if (!pn || v < 0 || v >= pn->n || port < 0 || port >= pn->degree[v])
        return INVALID_NODE;
    return pn->port_map[v][port];
}

int reverse_port_at_neighbor(const PortNumbered* pn, int v, int port) {
    if (!pn || v < 0 || v >= pn->n || port < 0 || port >= pn->degree[v])
        return INVALID_PORT;
    return pn->reverse_port[v][port];
}

/* ── Local State Operations ─────────────────────────────────── */

LocalNodeState* local_state_create(int node_id, int degree,
                                     const int* neighbor_ids) {
    LocalNodeState* st = (LocalNodeState*)calloc(1, sizeof(LocalNodeState));
    if (!st) return NULL;
    st->node_id = node_id;
    st->degree = degree;
    st->round = 0;
    st->color = INVALID_COLOR;
    st->in_mis = false;
    st->terminated = false;

    st->neighbor_ids = (int*)malloc((size_t)degree * sizeof(int));
    st->neighbor_colors = (int*)malloc((size_t)degree * sizeof(int));
    st->neighbor_in_mis = (bool*)calloc((size_t)degree, sizeof(bool));
    if (!st->neighbor_ids || !st->neighbor_colors || !st->neighbor_in_mis) {
        local_state_free(st); return NULL;
    }
    for (int i = 0; i < degree; i++) {
        st->neighbor_ids[i] = neighbor_ids ? neighbor_ids[i] : INVALID_NODE;
        st->neighbor_colors[i] = INVALID_COLOR;
    }

    st->inbox_capacity = 16;
    st->inbox = (Message*)malloc((size_t)st->inbox_capacity * sizeof(Message));
    st->inbox_size = 0;

    return st;
}

void local_state_free(LocalNodeState* st) {
    if (!st) return;
    free(st->neighbor_colors);
    free(st->neighbor_in_mis);
    free(st->neighbor_ids);
    free(st->palette);
    free(st->inbox);
    free(st);
}

void local_state_receive(LocalNodeState* st, const Message* msg) {
    if (!st || !msg) return;
    if (st->inbox_size >= st->inbox_capacity) {
        st->inbox_capacity *= 2;
        st->inbox = (Message*)realloc(st->inbox,
                      (size_t)st->inbox_capacity * sizeof(Message));
    }
    st->inbox[st->inbox_size++] = *msg;
}

void local_state_clear_inbox(LocalNodeState* st) {
    if (st) st->inbox_size = 0;
}

/* ── Execution Trace Operations ─────────────────────────────── */

ExecutionTrace* trace_create(int n, int capacity) {
    ExecutionTrace* t = (ExecutionTrace*)calloc(1, sizeof(ExecutionTrace));
    if (!t) return NULL;
    t->n = n;
    t->n_rounds = 0;
    t->capacity = capacity > 0 ? capacity : 64;
    t->rounds = (RoundSnapshot*)calloc((size_t)t->capacity,
                                        sizeof(RoundSnapshot));
    if (!t->rounds) { free(t); return NULL; }
    return t;
}

void trace_record_round(ExecutionTrace* t, int round,
                        const int* colors, const bool* in_mis,
                        const int* msg_counts) {
    if (!t) return;
    if (t->n_rounds >= t->capacity) {
        t->capacity *= 2;
        t->rounds = (RoundSnapshot*)realloc(t->rounds,
                     (size_t)t->capacity * sizeof(RoundSnapshot));
    }
    RoundSnapshot* rs = &t->rounds[t->n_rounds++];
    rs->round = round;
    rs->colors = (int*)malloc((size_t)t->n * sizeof(int));
    rs->in_mis = (bool*)malloc((size_t)t->n * sizeof(bool));
    rs->msg_counts = (int*)malloc((size_t)t->n * sizeof(int));
    if (rs->colors && rs->in_mis && rs->msg_counts) {
        for (int i = 0; i < t->n; i++) {
            rs->colors[i] = colors ? colors[i] : INVALID_COLOR;
            rs->in_mis[i] = in_mis ? in_mis[i] : false;
            rs->msg_counts[i] = msg_counts ? msg_counts[i] : 0;
        }
    }
}

void trace_free(ExecutionTrace* t) {
    if (!t) return;
    if (t->rounds) {
        for (int i = 0; i < t->n_rounds; i++) {
            free(t->rounds[i].colors);
            free(t->rounds[i].in_mis);
            free(t->rounds[i].msg_counts);
        }
        free(t->rounds);
    }
    free(t);
}

/* ── Graph Analysis ─────────────────────────────────────────── */

int graph_diameter(const Graph* g) {
    if (!g || g->n == 0) return 0;
    int diam = 0;
    int* dist = (int*)malloc((size_t)g->n * sizeof(int));
    int* queue = (int*)malloc((size_t)g->n * sizeof(int));
    if (!dist || !queue) { free(dist); free(queue); return -1; }

    for (int src = 0; src < g->n; src++) {
        for (int i = 0; i < g->n; i++) dist[i] = -1;
        int head = 0, tail = 0;
        dist[src] = 0;
        queue[tail++] = src;
        while (head < tail) {
            int u = queue[head++];
            AdjList* al = &g->adjacency[u];
            for (int i = 0; i < al->degree; i++) {
                int w = al->neighbors[i];
                if (dist[w] < 0) {
                    dist[w] = dist[u] + 1;
                    queue[tail++] = w;
                }
            }
        }
        for (int i = 0; i < g->n; i++) {
            if (dist[i] > diam) diam = dist[i];
        }
    }
    free(dist); free(queue);
    return diam;
}

static void brute_force_is(const Graph* g, bool* current, int idx,
                            int* best_size, bool* best_set) {
    if (idx >= g->n) {
        int sz = 0;
        for (int i = 0; i < g->n; i++) if (current[i]) sz++;
        /* Check independence */
        for (int u = 0; u < g->n; u++) {
            if (!current[u]) continue;
            AdjList* al = &g->adjacency[u];
            for (int i = 0; i < al->degree; i++) {
                if (current[al->neighbors[i]]) return;
            }
        }
        if (sz > *best_size) {
            *best_size = sz;
            for (int i = 0; i < g->n; i++) best_set[i] = current[i];
        }
        return;
    }
    /* Branch: exclude idx */
    current[idx] = false;
    brute_force_is(g, current, idx + 1, best_size, best_set);
    /* Branch: include idx */
    current[idx] = true;
    brute_force_is(g, current, idx + 1, best_size, best_set);
}

int graph_independence_number(const Graph* g) {
    if (!g || g->n > 30) return -1; /* brute force limit */
    bool* current = (bool*)calloc((size_t)g->n, sizeof(bool));
    bool* best = (bool*)calloc((size_t)g->n, sizeof(bool));
    int best_size = 0;
    if (current && best) {
        brute_force_is(g, current, 0, &best_size, best);
    }
    free(current); free(best);
    return best_size;
}

int graph_chromatic_number(const Graph* g) {
    if (!g || g->n > 20) return -1;
    /* Brute force: try k=1,2,... until proper coloring found */
    int* colors = (int*)malloc((size_t)g->n * sizeof(int));
    if (!colors) return -1;
    int n = g->n;

    for (int k = 1; k <= n; k++) {
        /* Try all k^n assignments via backtracking */
        int* stack = (int*)calloc((size_t)n, sizeof(int));
        if (!stack) { free(colors); return -1; }
        int pos = 0;
        bool found = false;
        while (pos >= 0 && pos < n) {
            if (stack[pos] >= k) {
                stack[pos] = 0;
                pos--;
                if (pos >= 0) stack[pos]++;
                continue;
            }
            colors[pos] = stack[pos];
            /* Check conflicts with already colored neighbors */
            bool conflict = false;
            AdjList* al = &g->adjacency[pos];
            for (int i = 0; i < al->degree; i++) {
                int nb = al->neighbors[i];
                if (nb < pos && colors[nb] == colors[pos]) {
                    conflict = true; break;
                }
            }
            if (!conflict) {
                if (pos == n - 1) { found = true; break; }
                pos++;
            } else {
                stack[pos]++;
            }
        }
        free(stack);
        if (found) { free(colors); return k; }
    }
    free(colors);
    return n;
}

double graph_clustering_coeff(const Graph* g, int v) {
    if (!g || v < 0 || v >= g->n) return 0.0;
    int d = g->adjacency[v].degree;
    if (d < 2) return 0.0;

    /* Count edges between neighbors of v */
    int edge_count = 0;
    for (int i = 0; i < d; i++) {
        int u = g->adjacency[v].neighbors[i];
        for (int j = i + 1; j < d; j++) {
            int w = g->adjacency[v].neighbors[j];
            /* Check if u and w are adjacent */
            AdjList* al = &g->adjacency[u];
            for (int k = 0; k < al->degree; k++) {
                if (al->neighbors[k] == w) { edge_count++; break; }
            }
        }
    }
    double possible = (double)d * (double)(d - 1) / 2.0;
    return (double)edge_count / possible;
}

double graph_avg_clustering(const Graph* g) {
    if (!g || g->n == 0) return 0.0;
    double sum = 0.0;
    for (int v = 0; v < g->n; v++) {
        sum += graph_clustering_coeff(g, v);
    }
    return sum / (double)g->n;
}

int graph_girth(const Graph* g) {
    if (!g) return -1;
    /* BFS from each node to find shortest cycle */
    int girth = g->n + 1;
    int* dist = (int*)malloc((size_t)g->n * sizeof(int));
    int* parent = (int*)malloc((size_t)g->n * sizeof(int));
    int* queue = (int*)malloc((size_t)g->n * sizeof(int));
    if (!dist || !parent || !queue) { free(dist); free(parent); free(queue); return -1; }

    for (int src = 0; src < g->n; src++) {
        for (int i = 0; i < g->n; i++) { dist[i] = -1; parent[i] = -1; }
        int head = 0, tail = 0;
        dist[src] = 0;
        queue[tail++] = src;
        while (head < tail) {
            int u = queue[head++];
            AdjList* al = &g->adjacency[u];
            for (int i = 0; i < al->degree; i++) {
                int w = al->neighbors[i];
                if (dist[w] < 0) {
                    dist[w] = dist[u] + 1;
                    parent[w] = u;
                    queue[tail++] = w;
                } else if (w != parent[u]) {
                    /* Found a cycle */
                    int cycle_len = dist[u] + dist[w] + 1;
                    if (cycle_len < girth) girth = cycle_len;
                }
            }
        }
    }
    free(dist); free(parent); free(queue);
    return girth <= g->n ? girth : -1;
}

bool graph_is_bipartite(const Graph* g) {
    if (!g) return false;
    int* color = (int*)malloc((size_t)g->n * sizeof(int));
    if (!color) return false;
    for (int i = 0; i < g->n; i++) color[i] = -1;

    int* queue = (int*)malloc((size_t)g->n * sizeof(int));
    if (!queue) { free(color); return false; }
    bool bipartite = true;

    for (int src = 0; src < g->n && bipartite; src++) {
        if (color[src] >= 0) continue;
        int head = 0, tail = 0;
        color[src] = 0;
        queue[tail++] = src;
        while (head < tail && bipartite) {
            int u = queue[head++];
            AdjList* al = &g->adjacency[u];
            for (int i = 0; i < al->degree; i++) {
                int w = al->neighbors[i];
                if (color[w] < 0) {
                    color[w] = 1 - color[u];
                    queue[tail++] = w;
                } else if (color[w] == color[u]) {
                    bipartite = false;
                    break;
                }
            }
        }
    }
    free(color); free(queue);
    return bipartite;
}

/* ── Lower Bound Support ────────────────────────────────────── */

int log_star(int n) {
    if (n <= 2) return 1;
    int count = 0;
    int val = n;
    while (val > 1) {
        int log2 = 0;
        int tmp = val;
        while (tmp > 1) { tmp >>= 1; log2++; }
        val = log2;
        count++;
    }
    return count;
}

int count_distinct_k_views(const Graph* g, int k) {
    if (!g || k < 0) return 0;
    /* Build all views, compare pairwise for isomorphism */
    DistanceKView** views = (DistanceKView**)calloc((size_t)g->n,
                                                      sizeof(DistanceKView*));
    if (!views) return 0;
    for (int v = 0; v < g->n; v++) {
        views[v] = neighborhood_view(g, v, k);
    }

    bool* unique = (bool*)calloc((size_t)g->n, sizeof(bool));
    if (!unique) {
        for (int i = 0; i < g->n; i++) neighborhood_view_free(views[i]);
        free(views); return 0;
    }

    int distinct = 0;
    for (int i = 0; i < g->n; i++) {
        unique[i] = true;
        for (int j = 0; j < i; j++) {
            if (unique[j] && neighborhood_views_isomorphic(views[i], views[j])) {
                unique[i] = false;
                break;
            }
        }
        if (unique[i]) distinct++;
    }

    free(unique);
    for (int i = 0; i < g->n; i++) neighborhood_view_free(views[i]);
    free(views);
    return distinct;
}

int max_component_size(const Graph* g) {
    if (!g) return 0;
    bool* visited = (bool*)calloc((size_t)g->n, sizeof(bool));
    int* queue = (int*)malloc((size_t)g->n * sizeof(int));
    if (!visited || !queue) { free(visited); free(queue); return -1; }

    int max_comp = 0;
    for (int src = 0; src < g->n; src++) {
        if (visited[src]) continue;
        int comp_size = 0;
        int head = 0, tail = 0;
        visited[src] = true;
        queue[tail++] = src;
        while (head < tail) {
            int u = queue[head++];
            comp_size++;
            AdjList* al = &g->adjacency[u];
            for (int i = 0; i < al->degree; i++) {
                int w = al->neighbors[i];
                if (!visited[w]) {
                    visited[w] = true;
                    queue[tail++] = w;
                }
            }
        }
        if (comp_size > max_comp) max_comp = comp_size;
    }
    free(visited); free(queue);
    return max_comp;
}
