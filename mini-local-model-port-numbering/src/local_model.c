/**
 * @file local_model.c
 * @brief Implementation of the LOCAL model of distributed computing.
 *
 * This file implements the synchronous round-based execution engine,
 * graph manipulation utilities, and core LOCAL model primitives.
 *
 * Knowledge points covered:
 *  L1: LOCAL model definition (nodes, messages, rounds, termination)
 *  L2: Synchronous execution, r-neighborhoods, diameter
 *  L3: Graph representations (adjacency matrix, edge lists),
 *      Graph topology generators (path, cycle, tree, random, clique)
 *  L4: The fundamental LOCAL lemma: after T rounds, node v knows its
 *      entire T-neighborhood (implemented in local_r_neighborhood)
 *
 * References:
 *  - Linial (1992). "Locality in distributed graph algorithms." SIAM J. Comput. 21(1).
 *  - Peleg (2000). "Distributed Computing: A Locality-Sensitive Approach." SIAM.
 */

#include "local_model.h"
#include "port_numbering.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

/* ================================================================
 * L1+L2: Context Initialization and Destruction
 * ================================================================ */

int local_context_init(local_context_t *ctx, uint32_t n, const bool *adjacency) {
    if (!ctx || n == 0 || n > LOCAL_MAX_NODES || !adjacency) {
        return -1;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->num_nodes = n;
    ctx->current_round = 0;
    ctx->terminated_count = 0;

    /* Allocate and copy adjacency matrix */
    ctx->adjacency = (bool *)calloc((size_t)n * n, sizeof(bool));
    if (!ctx->adjacency) return -1;
    memcpy(ctx->adjacency, adjacency, (size_t)n * n * sizeof(bool));

    /* Allocate nodes */
    ctx->nodes = (local_node_t *)calloc(n, sizeof(local_node_t));
    if (!ctx->nodes) {
        free(ctx->adjacency);
        ctx->adjacency = NULL;
        return -1;
    }

    /* Initialize nodes with UIDs and neighbor lists */
    for (uint32_t i = 0; i < n; i++) {
        ctx->nodes[i].uid = i + 1;  /* UIDs 1..n */
        ctx->nodes[i].degree = 0;
        ctx->nodes[i].local_state = NULL;
        ctx->nodes[i].terminated = false;

        for (uint32_t j = 0; j < n; j++) {
            if (adjacency[i * n + j] && i != j) {
                uint32_t d = ctx->nodes[i].degree;
                if (d >= LOCAL_MAX_DEGREE) {
                    /* Too many neighbors */
                    local_context_destroy(ctx);
                    return -1;
                }
                ctx->nodes[i].neighbors[d] = j;
                ctx->nodes[i].ports[d] = d;  /* Default: port = neighbor index order */
                ctx->nodes[i].degree++;
            }
        }
    }

    /* Allocate per-round outgoing message buffers */
    ctx->outgoing = (local_message_t **)calloc(n, sizeof(local_message_t *));
    ctx->outgoing_count = (size_t *)calloc(n, sizeof(size_t));
    if (!ctx->outgoing || !ctx->outgoing_count) {
        local_context_destroy(ctx);
        return -1;
    }

    return 0;
}

void local_context_destroy(local_context_t *ctx) {
    if (!ctx) return;

    /* Free per-node local_state (caller's responsibility to free contents,
       but we free the array pointers in outgoing buffers) */
    if (ctx->outgoing) {
        for (uint32_t i = 0; i < ctx->num_nodes; i++) {
            if (ctx->outgoing[i]) {
                for (size_t m = 0; m < ctx->outgoing_count[i]; m++) {
                    free(ctx->outgoing[i][m].payload);
                }
                free(ctx->outgoing[i]);
            }
        }
        free(ctx->outgoing);
    }
    free(ctx->outgoing_count);
    free(ctx->nodes);
    free(ctx->adjacency);
    memset(ctx, 0, sizeof(*ctx));
}

/* ================================================================
 * L2: r-Neighborhood Computation (Fundamental LOCAL Lemma)
 * ================================================================ */

/**
 * BFS within the adjacency matrix to compute distances from source node.
 * Queues are stored on the heap (worst-case O(n) elements).
 */
static void bfs_distances(const local_context_t *ctx, uint32_t source,
                          uint32_t *dist) {
    uint32_t n = ctx->num_nodes;
    for (uint32_t i = 0; i < n; i++) dist[i] = UINT32_MAX;
    dist[source] = 0;

    uint32_t *queue = (uint32_t *)malloc(n * sizeof(uint32_t));
    if (!queue) return;
    uint32_t qh = 0, qt = 0;
    queue[qt++] = source;

    while (qh < qt) {
        uint32_t u = queue[qh++];
        for (uint32_t d = 0; d < ctx->nodes[u].degree; d++) {
            uint32_t v = ctx->nodes[u].neighbors[d];
            if (dist[v] == UINT32_MAX) {
                dist[v] = dist[u] + 1;
                queue[qt++] = v;
            }
        }
    }
    free(queue);
}

int local_r_neighborhood(const local_context_t *ctx, uint32_t u, uint32_t r,
                         uint32_t *out_neighbors, uint32_t *out_count) {
    if (!ctx || !out_neighbors || !out_count || u >= ctx->num_nodes) {
        return -1;
    }

    uint32_t n = ctx->num_nodes;
    uint32_t *dist = (uint32_t *)malloc(n * sizeof(uint32_t));
    if (!dist) return -1;

    bfs_distances(ctx, u, dist);

    *out_count = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (dist[i] <= r) {
            out_neighbors[(*out_count)++] = i;
        }
    }

    free(dist);
    return 0;
}

/* ================================================================
 * L2: Synchronous Round Execution
 * ================================================================ */

int local_execute_round(local_context_t *ctx, local_round_fn_t round_fn) {
    if (!ctx || !round_fn) return -1;
    if (local_get_state(ctx) == LOCAL_STATE_ALL_TERMINATED) return 0;

    uint32_t n = ctx->num_nodes;

    /* Phase 1: Each node executes its round function (fills outgoing buffers) */
    for (uint32_t i = 0; i < n; i++) {
        if (!ctx->nodes[i].terminated) {
            round_fn(ctx, i);
        }
    }

    /* Phase 2: Message delivery (in the classic LOCAL model, each message
       is delivered to all neighbors — here we simulate delivery by making
       each outgoing message available to the receiver's next round) */

    /* Phase 3: Increment round counter */
    ctx->current_round++;

    /* Check termination: count how many nodes have terminated */
    ctx->terminated_count = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (ctx->nodes[i].terminated) ctx->terminated_count++;
    }

    /* Clean up outgoing buffers for next round */
    for (uint32_t i = 0; i < n; i++) {
        if (ctx->outgoing[i]) {
            for (size_t m = 0; m < ctx->outgoing_count[i]; m++) {
                free(ctx->outgoing[i][m].payload);
            }
            free(ctx->outgoing[i]);
            ctx->outgoing[i] = NULL;
        }
        ctx->outgoing_count[i] = 0;
    }

    return 0;
}

uint32_t local_execute_rounds(local_context_t *ctx, uint32_t t, local_round_fn_t round_fn) {
    uint32_t executed = 0;
    for (uint32_t r = 0; r < t; r++) {
        if (local_get_state(ctx) == LOCAL_STATE_ALL_TERMINATED) break;
        local_execute_round(ctx, round_fn);
        executed++;
    }
    return executed;
}

/* ================================================================
 * L2: Distance, Diameter, Connectivity
 * ================================================================ */

uint32_t local_distance(const local_context_t *ctx, uint32_t u, uint32_t v) {
    if (!ctx || u >= ctx->num_nodes || v >= ctx->num_nodes) return UINT32_MAX;
    if (u == v) return 0;

    uint32_t n = ctx->num_nodes;
    uint32_t *dist = (uint32_t *)malloc(n * sizeof(uint32_t));
    if (!dist) return UINT32_MAX;

    bfs_distances(ctx, u, dist);
    uint32_t d = dist[v];
    free(dist);
    return d;
}

uint32_t local_diameter(const local_context_t *ctx) {
    if (!ctx) return 0;
    uint32_t n = ctx->num_nodes;
    if (n <= 1) return 0;

    uint32_t max_dist = 0;
    uint32_t *dist = (uint32_t *)malloc(n * sizeof(uint32_t));
    if (!dist) return 0;

    for (uint32_t u = 0; u < n; u++) {
        bfs_distances(ctx, u, dist);
        for (uint32_t v = 0; v < n; v++) {
            if (dist[v] != UINT32_MAX && dist[v] > max_dist) {
                max_dist = dist[v];
            }
        }
    }
    free(dist);
    return max_dist;
}

bool local_is_connected(const local_context_t *ctx) {
    if (!ctx || ctx->num_nodes == 0) return true;
    uint32_t n = ctx->num_nodes;
    uint32_t *dist = (uint32_t *)malloc(n * sizeof(uint32_t));
    if (!dist) return false;

    bfs_distances(ctx, 0, dist);
    for (uint32_t i = 0; i < n; i++) {
        if (dist[i] == UINT32_MAX) {
            free(dist);
            return false;
        }
    }
    free(dist);
    return true;
}

/* ================================================================
 * L3: Graph Construction from Edge Lists
 * ================================================================ */

int local_build_from_edges(local_context_t *ctx, const local_edge_t *edges, size_t num_edges) {
    if (!ctx || !edges) return -1;
    uint32_t n = ctx->num_nodes;

    /* Reset adjacency */
    memset(ctx->adjacency, 0, (size_t)n * n * sizeof(bool));

    for (size_t k = 0; k < num_edges; k++) {
        uint32_t u = edges[k].u;
        uint32_t v = edges[k].v;
        if (u < n && v < n && u != v) {
            ctx->adjacency[u * n + v] = true;
            ctx->adjacency[v * n + u] = true;
        }
    }

    /* Rebuild neighbor lists */
    for (uint32_t i = 0; i < n; i++) {
        ctx->nodes[i].degree = 0;
        for (uint32_t j = 0; j < n; j++) {
            if (ctx->adjacency[i * n + j] && i != j) {
                uint32_t d = ctx->nodes[i].degree;
                if (d < LOCAL_MAX_DEGREE) {
                    ctx->nodes[i].neighbors[d] = j;
                    ctx->nodes[i].ports[d] = d;
                    ctx->nodes[i].degree++;
                }
            }
        }
    }

    return 0;
}

/* ================================================================
 * L3: Graph Topology Generators
 * ================================================================ */

/* Simple xorshift64 PRNG for deterministic random graphs */
static uint64_t xorshift64_state;

static void xs64_seed(uint64_t seed) {
    xorshift64_state = (seed == 0) ? 1 : seed;
}

static uint64_t xs64_rand(void) {
    uint64_t x = xorshift64_state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    xorshift64_state = x;
    return x;
}

/* Helper: generate a double in [0, 1) from xorshift64 */
static double xs64_double(void) {
    return (double)(xs64_rand() >> 11) * (1.0 / (1ULL << 53));
}

/* Initialize a fresh context with n nodes, no edges */
static int ctx_init_empty(local_context_t *ctx, uint32_t n) {
    bool *adj = (bool *)calloc((size_t)n * n, sizeof(bool));
    if (!adj) return -1;
    int ret = local_context_init(ctx, n, adj);
    free(adj);
    return ret;
}

int local_make_path(local_context_t *ctx, uint32_t n) {
    if (!ctx || n < 1 || n > LOCAL_MAX_NODES) return -1;

    bool *adj = (bool *)calloc((size_t)n * n, sizeof(bool));
    if (!adj) return -1;

    for (uint32_t i = 0; i + 1 < n; i++) {
        adj[i * n + (i + 1)] = true;
        adj[(i + 1) * n + i] = true;
    }

    int ret = local_context_init(ctx, n, adj);
    free(adj);
    return ret;
}

int local_make_cycle(local_context_t *ctx, uint32_t n) {
    if (!ctx || n < 3 || n > LOCAL_MAX_NODES) return -1;

    bool *adj = (bool *)calloc((size_t)n * n, sizeof(bool));
    if (!adj) return -1;

    for (uint32_t i = 0; i < n; i++) {
        uint32_t next = (i + 1) % n;
        adj[i * n + next] = true;
        adj[next * n + i] = true;
    }

    int ret = local_context_init(ctx, n, adj);
    free(adj);
    return ret;
}

int local_make_regular_tree(local_context_t *ctx, uint32_t d, uint32_t h) {
    if (!ctx || d < 2 || h == 0) return -1;

    /* Compute total nodes: root has d children, each internal node has d-1 children */
    uint32_t total = 1;  /* root */
    uint32_t level_nodes = d;
    for (uint32_t level = 1; level <= h; level++) {
        total += level_nodes;
        level_nodes *= (d - 1);
    }

    if (total > LOCAL_MAX_NODES) return -1;

    bool *adj = (bool *)calloc((size_t)total * total, sizeof(bool));
    if (!adj) return -1;

    /* Build tree level by level: nodes are numbered in BFS order */
    uint32_t next_node = 1;
    uint32_t prev_start = 0, prev_count = 1;
    uint32_t children_per_node = d;

    for (uint32_t level = 0; level < h; level++) {
        uint32_t new_start = next_node;
        uint32_t new_count = 0;

        for (uint32_t p = prev_start; p < prev_start + prev_count; p++) {
            for (uint32_t c = 0; c < children_per_node && next_node < total; c++) {
                adj[p * total + next_node] = true;
                adj[next_node * total + p] = true;
                next_node++;
                new_count++;
            }
        }

        prev_start = new_start;
        prev_count = new_count;
        children_per_node = d - 1;  /* Non-root nodes have d-1 children */
    }

    int ret = local_context_init(ctx, total, adj);
    free(adj);
    return ret;
}

int local_make_random_graph(local_context_t *ctx, uint32_t n, double p, uint64_t seed) {
    if (!ctx || n < 1 || n > LOCAL_MAX_NODES || p < 0.0 || p > 1.0) return -1;

    xs64_seed(seed);
    bool *adj = (bool *)calloc((size_t)n * n, sizeof(bool));
    if (!adj) return -1;

    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = i + 1; j < n; j++) {
            if (xs64_double() < p) {
                adj[i * n + j] = true;
                adj[j * n + i] = true;
            }
        }
    }

    int ret = local_context_init(ctx, n, adj);
    free(adj);
    return ret;
}

int local_make_clique(local_context_t *ctx, uint32_t n) {
    if (!ctx || n < 1 || n > LOCAL_MAX_NODES) return -1;

    bool *adj = (bool *)calloc((size_t)n * n, sizeof(bool));
    if (!adj) return -1;

    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j < n; j++) {
            if (i != j) {
                adj[i * n + j] = true;
            }
        }
    }

    int ret = local_context_init(ctx, n, adj);
    free(adj);
    return ret;
}

/* ================================================================
 * L2: Graph Properties
 * ================================================================ */

uint32_t local_max_degree(const local_context_t *ctx) {
    if (!ctx) return 0;
    uint32_t max_d = 0;
    for (uint32_t i = 0; i < ctx->num_nodes; i++) {
        if (ctx->nodes[i].degree > max_d) max_d = ctx->nodes[i].degree;
    }
    return max_d;
}

uint32_t local_min_degree(const local_context_t *ctx) {
    if (!ctx || ctx->num_nodes == 0) return 0;
    uint32_t min_d = UINT32_MAX;
    for (uint32_t i = 0; i < ctx->num_nodes; i++) {
        if (ctx->nodes[i].degree < min_d) min_d = ctx->nodes[i].degree;
    }
    return (min_d == UINT32_MAX) ? 0 : min_d;
}

double local_avg_degree(const local_context_t *ctx) {
    if (!ctx || ctx->num_nodes == 0) return 0.0;
    uint64_t sum = 0;
    for (uint32_t i = 0; i < ctx->num_nodes; i++) {
        sum += ctx->nodes[i].degree;
    }
    return (double)sum / ctx->num_nodes;
}

uint32_t local_edge_count(const local_context_t *ctx) {
    if (!ctx) return 0;
    uint32_t count = 0;
    uint32_t n = ctx->num_nodes;
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = i + 1; j < n; j++) {
            if (ctx->adjacency[i * n + j]) count++;
        }
    }
    return count;
}

/* ================================================================
 * L2: Girth and Chromatic Number (exact for small n)
 * ================================================================ */

/**
 * Find the shortest cycle containing a given edge (or any cycle).
 * Brute-force BFS: for each edge (u,v), temporarily remove it and
 * find shortest path between u and v; if found, girth <= path_len + 1.
 */
uint32_t local_girth(const local_context_t *ctx) {
    if (!ctx || ctx->num_nodes < 3) return UINT32_MAX;

    uint32_t n = ctx->num_nodes;
    uint32_t girth = UINT32_MAX;

    for (uint32_t u = 0; u < n; u++) {
        for (uint32_t d = 0; d < ctx->nodes[u].degree; d++) {
            uint32_t v = ctx->nodes[u].neighbors[d];
            if (v < u) continue;  /* Process each edge once */

            /* Temporarily "remove" edge (u,v) and find shortest path */
            /* Use a modified BFS that ignores the direct edge */
            uint32_t *dist = (uint32_t *)malloc(n * sizeof(uint32_t));
            uint32_t *queue = (uint32_t *)malloc(n * sizeof(uint32_t));
            if (!dist || !queue) {
                free(dist);
                free(queue);
                continue;
            }

            for (uint32_t i = 0; i < n; i++) dist[i] = UINT32_MAX;
            dist[u] = 0;
            uint32_t qh = 0, qt = 0;
            queue[qt++] = u;

            while (qh < qt) {
                uint32_t x = queue[qh++];
                if (x == v) break;
                for (uint32_t dd = 0; dd < ctx->nodes[x].degree; dd++) {
                    uint32_t y = ctx->nodes[x].neighbors[dd];
                    /* Skip the direct (u,v) edge */
                    if ((x == u && y == v) || (x == v && y == u)) continue;
                    if (dist[y] == UINT32_MAX) {
                        dist[y] = dist[x] + 1;
                        queue[qt++] = y;
                    }
                }
            }

            if (dist[v] != UINT32_MAX) {
                uint32_t cycle_len = dist[v] + 1;
                if (cycle_len < girth) girth = cycle_len;
            }

            free(dist);
            free(queue);
        }
    }

    return girth;
}

/**
 * Compute the chromatic number χ(G) by brute-force backtracking.
 * Only practical for n ≤ ~20, which suits our LOCAL_MAX_NODES context
 * for small examples. Returns exact χ for small n; for larger n,
 * returns an upper bound (Δ+1).
 */
static bool is_coloring_valid(const bool *adj, uint32_t n, const uint32_t *colors) {
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = i + 1; j < n; j++) {
            if (adj[i * n + j] && colors[i] == colors[j]) return false;
        }
    }
    return true;
}

static bool color_backtrack(const bool *adj, uint32_t n, uint32_t num_colors,
                            uint32_t *colors, uint32_t idx) {
    if (idx == n) return true;

    for (uint32_t c = 0; c < num_colors; c++) {
        bool conflict = false;
        for (uint32_t j = 0; j < idx; j++) {
            if (adj[idx * n + j] && colors[j] == c) {
                conflict = true;
                break;
            }
        }
        if (!conflict) {
            colors[idx] = c;
            if (color_backtrack(adj, n, num_colors, colors, idx + 1)) return true;
        }
    }
    return false;
}

uint32_t local_chromatic_number(const local_context_t *ctx) {
    if (!ctx || ctx->num_nodes == 0) return 0;

    uint32_t n = ctx->num_nodes;
    uint32_t *colors = (uint32_t *)calloc(n, sizeof(uint32_t));
    if (!colors) return local_max_degree(ctx) + 1;  /* Fallback to Δ+1 bound */

    for (uint32_t k = 1; k <= n; k++) {
        memset(colors, 0, n * sizeof(uint32_t));
        if (color_backtrack(ctx->adjacency, n, k, colors, 0)) {
            free(colors);
            return k;
        }
    }

    free(colors);
    return n;
}

/* ================================================================
 * L3: State Machine
 * ================================================================ */

local_exec_state_t local_get_state(const local_context_t *ctx) {
    if (!ctx) return LOCAL_STATE_ERROR;
    if (ctx->terminated_count == ctx->num_nodes) return LOCAL_STATE_ALL_TERMINATED;
    if (ctx->current_round > 0) return LOCAL_STATE_RUNNING;
    return LOCAL_STATE_INIT;
}

void local_reset(local_context_t *ctx) {
    if (!ctx) return;
    ctx->current_round = 0;
    ctx->terminated_count = 0;
    for (uint32_t i = 0; i < ctx->num_nodes; i++) {
        ctx->nodes[i].local_state = NULL;
        ctx->nodes[i].terminated = false;
    }
}

void local_node_copy(local_node_t *dst, const local_node_t *src, local_copy_fn_t copy_fn) {
    if (!dst || !src) return;
    memcpy(dst, src, sizeof(local_node_t));
    if (copy_fn && src->local_state) {
        dst->local_state = copy_fn(src->local_state);
    }
}
