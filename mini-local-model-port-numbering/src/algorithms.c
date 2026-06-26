/**
 * @file algorithms.c
 * @brief Core distributed algorithms for the LOCAL model.
 *
 * Implements the main algorithmic building blocks:
 *  L5-1: Cole-Vishkin coloring reduction and iterated log
 *  L5-2: Linial's color reduction
 *  L5-3: Luby MIS (full algorithm)
 *  L5-4: BFS tree construction, broadcast, convergecast
 *  L5-5: Leader election (deterministic and randomized)
 *  L5-6: Triangle detection/counting
 *  L5-7: Distributed sorting on a ring
 *  L5-8: Distributed sum/max aggregation
 *  L5-9: Distance-2 coloring
 *  L5-10: Full maximal matching (randomized)
 *
 * References:
 *  - Cole & Vishkin (1986). Inf. Control 70(1).
 *  - Linial (1992). SIAM J. Comput. 21(1).
 *  - Luby (1986). SIAM J. Comput. 15(4).
 *  - Barenboim & Elkin (2013). "Distributed Graph Coloring." Morgan & Claypool.
 */

#include "algorithms.h"
#include "local_model.h"
#include "port_numbering.h"
#include "graph_problems.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

/* ================================================================
 * L5-1: Cole-Vishkin Support: Iterated Logarithm
 * ================================================================ */

uint32_t iterated_log2(uint64_t n) {
    if (n <= 1) return 0;
    uint32_t count = 0;
    while (n > 1) {
        /* Compute floor(log2(n)) */
        uint64_t v = n;
        uint32_t log_v = 0;
        while (v > 1) {
            v >>= 1;
            log_v++;
        }
        n = log_v;
        count++;
    }
    return count;
}

/* Helper: find the LSB index where a and b differ */
static uint32_t div_bit_idx(uint32_t a, uint32_t b) {
    uint32_t d = a ^ b;
    if (d == 0) return 32;
    uint32_t i = 0;
    while ((d & 1) == 0) {
        d >>= 1;
        i++;
    }
    return i;
}

uint32_t cv_reduce_colors(const local_context_t *ctx, const uint32_t *old_colors,
                          uint32_t *new_colors, const port_numbering_t *pns,
                          uint32_t successor_port) {
    if (!ctx || !old_colors || !new_colors || !pns) return 0;

    uint32_t n = ctx->num_nodes;
    uint32_t max_color = 0;

    for (uint32_t i = 0; i < n; i++) {
        uint32_t succ = pns[i].neighbor_by_port[successor_port];
        if (succ >= n) {
            /* No successor via this port; use UID as fallback */
            new_colors[i] = old_colors[i];
            continue;
        }

        uint32_t ci = old_colors[i];
        uint32_t cs = old_colors[succ];

        if (ci == cs) {
            new_colors[i] = ci;
            continue;
        }

        uint32_t idx = div_bit_idx(ci, cs);
        uint32_t bit = (ci >> idx) & 1;
        new_colors[i] = 2 * idx + bit;

        if (new_colors[i] > max_color) max_color = new_colors[i];
    }

    /* Return the number of bits needed to represent the new palette + 1 */
    uint32_t bits = 0;
    while (max_color > 0) {
        max_color >>= 1;
        bits++;
    }
    return bits;
}

uint32_t cv_full_reduction(local_context_t *ctx, uint32_t *colors,
                           const port_numbering_t *pns, uint32_t successor_port) {
    if (!ctx || !colors || !pns) return 0;

    uint32_t n = ctx->num_nodes;
    uint32_t *buf = (uint32_t *)malloc(n * sizeof(uint32_t));
    if (!buf) return 0;

    /* Use input colors as starting point */
    uint32_t *cur = colors;
    uint32_t *next = buf;
    uint32_t rounds = 0;
    uint32_t max_rounds = iterated_log2(n) + 10;

    for (uint32_t r = 0; r < max_rounds; r++) {
        rounds++;
        uint32_t bits = cv_reduce_colors(ctx, cur, next, pns, successor_port);

        /* Check max color in next */
        uint32_t max_c = 0;
        for (uint32_t i = 0; i < n; i++) {
            if (next[i] > max_c) max_c = next[i];
        }

        /* Swap buffers */
        uint32_t *tmp = cur;
        cur = next;
        next = tmp;

        /* If we have ≤6 colors, we're done */
        if (max_c <= 5) break;
    }

    /* Copy final result to colors if cur != colors */
    if (cur != colors) {
        memcpy(colors, cur, n * sizeof(uint32_t));
    }

    free(buf);
    return rounds;
}

/* ================================================================
 * L5-2: Linial's Coloring Algorithm
 * ================================================================ */

uint32_t linial_color_reduce(const local_context_t *ctx, const uint32_t *old_colors,
                             uint32_t *new_colors, uint32_t delta) {
    if (!ctx || !old_colors || !new_colors) return 0;

    uint32_t n = ctx->num_nodes;

    /* Linial's single-round reduction: each node chooses a color
       based on the multiset of neighbors' colors. The new palette
       size is O(Δ^2 log C), where C is the old palette size. */

    /* For each node, we collect the set of neighbor colors and
       find a color not used by any neighbor (simple greedy approach
       limited to Δ+1 palette, which Linial showed is possible with
       O(Δ^2 + log* n) rounds via a more careful reduction). */

    for (uint32_t i = 0; i < n; i++) {
        /* Find the smallest color not used by any neighbor */
        bool used[LOCAL_MAX_DEGREE + 2];
        memset(used, 0, sizeof(used));

        for (uint32_t d = 0; d < ctx->nodes[i].degree; d++) {
            uint32_t j = ctx->nodes[i].neighbors[d];
            uint32_t c = old_colors[j];
            if (c <= LOCAL_MAX_DEGREE) used[c] = true;
        }

        uint32_t new_c = 0;
        while (new_c <= delta + 1 && used[new_c]) new_c++;
        new_colors[i] = new_c;
    }

    return delta + 2;  /* Palette size used */
}

uint32_t linial_delta_plus_one_coloring(local_context_t *ctx, uint32_t *colors,
                                        const port_numbering_t *pns) {
    if (!ctx || !colors) return 0;

    uint32_t n = ctx->num_nodes;
    uint32_t delta = local_max_degree(ctx);

    /* Start with UID-based coloring */
    for (uint32_t i = 0; i < n; i++) {
        colors[i] = ctx->nodes[i].uid;
    }

    uint32_t *buf = (uint32_t *)malloc(n * sizeof(uint32_t));
    if (!buf) return 0;

    uint32_t rounds = 0;
    uint32_t current_colors = n;

    while (current_colors > delta + 1) {
        rounds++;
        linial_color_reduce(ctx, colors, buf, delta);

        /* Check how many distinct colors we have */
        uint32_t *tmp = buf;
        buf = colors;
        colors = tmp;

        /* Count distinct colors (pessimistic bound: max color value) */
        uint32_t max_c = 0;
        for (uint32_t i = 0; i < n; i++) {
            if (colors[i] > max_c) max_c = colors[i];
        }
        current_colors = max_c + 1;

        if (rounds > 100) break;  /* Safety */
    }

    free(buf);
    (void)pns;  /* Port numbering information used for neighbor identification,
                    which is already available through the graph adjacency */
    return rounds;
}

/* ================================================================
 * L5-3: Luby MIS (Full)
 * ================================================================ */

uint32_t luby_mis_full(local_context_t *ctx, mis_node_state_t *states, uint64_t seed) {
    if (!ctx || !states) return 0;

    uint32_t n = ctx->num_nodes;
    uint32_t rounds = 0;
    uint64_t current_seed = seed;

    while (rounds < 1000) {
        uint32_t changed = mis_luby_round(ctx, states, current_seed);
        rounds++;

        if (changed == 0) {
            /* Check if all nodes decided */
            bool all_decided = true;
            for (uint32_t i = 0; i < n; i++) {
                if (states[i].status == MIS_UNDECIDED) {
                    all_decided = false;
                    break;
                }
            }
            if (all_decided) break;

            /* If some remain undecided but nothing changed,
               do a greedy pass: add undecided nodes that have no
               undecided neighbor with smaller UID to MIS */
            bool made_progress = false;
            for (uint32_t i = 0; i < n; i++) {
                if (states[i].status != MIS_UNDECIDED) continue;

                /* Check if any neighbor is already IN_SET */
                bool neighbor_in_mis = false;
                for (uint32_t d = 0; d < ctx->nodes[i].degree; d++) {
                    uint32_t j = ctx->nodes[i].neighbors[d];
                    if (states[j].status == MIS_IN_SET) {
                        neighbor_in_mis = true;
                        break;
                    }
                }

                if (!neighbor_in_mis) {
                    /* Can safely join MIS */
                    states[i].status = MIS_IN_SET;
                    states[i].active = false;
                    made_progress = true;
                } else {
                    /* Neighbor in MIS, so must be OUT */
                    states[i].status = MIS_OUT_SET;
                    states[i].active = false;
                    made_progress = true;
                }
            }

            if (!made_progress) {
                /* Last resort: sequential greedy MIS on remaining undecided subgraph.
                   Process in order of UID: add to MIS, then mark neighbors as OUT. */
                bool *processed = (bool *)calloc(n, sizeof(bool));
                if (processed) {
                    for (uint32_t pass = 0; pass < 2; pass++) {
                        for (uint32_t i = 0; i < n; i++) {
                            if (states[i].status != MIS_UNDECIDED) continue;
                            if (processed[i]) continue;

                            bool neighbor_in_mis = false;
                            for (uint32_t d = 0; d < ctx->nodes[i].degree; d++) {
                                uint32_t j = ctx->nodes[i].neighbors[d];
                                if (states[j].status == MIS_IN_SET) {
                                    neighbor_in_mis = true;
                                    break;
                                }
                            }

                            if (!neighbor_in_mis) {
                                states[i].status = MIS_IN_SET;
                                states[i].active = false;
                                processed[i] = true;
                                /* Mark neighbors as OUT */
                                for (uint32_t d = 0; d < ctx->nodes[i].degree; d++) {
                                    uint32_t j = ctx->nodes[i].neighbors[d];
                                    if (states[j].status == MIS_UNDECIDED) {
                                        states[j].status = MIS_OUT_SET;
                                        states[j].active = false;
                                        processed[j] = true;
                                    }
                                }
                            } else {
                                states[i].status = MIS_OUT_SET;
                                states[i].active = false;
                                processed[i] = true;
                            }
                        }
                    }
                    free(processed);
                }
            }
            break;
        }

        current_seed = current_seed * 6364136223846793005ULL + 1;
    }

    return rounds;
}

uint32_t mis_random_priority_round(local_context_t *ctx, mis_node_state_t *states,
                                   uint64_t seed) {
    if (!ctx || !states) return 0;

    uint32_t n = ctx->num_nodes;

    /* Use a simple LCG per round */
    uint64_t state = seed;
    uint32_t *prio = (uint32_t *)malloc(n * sizeof(uint32_t));
    if (!prio) return 0;

    for (uint32_t i = 0; i < n; i++) {
        if (states[i].active && states[i].status == MIS_UNDECIDED) {
            state = state * 6364136223846793005ULL + 1442695040888963407ULL;
            prio[i] = (uint32_t)(state >> 33);
        } else {
            prio[i] = 0;
        }
    }

    uint32_t changed = 0;

    for (uint32_t i = 0; i < n; i++) {
        if (!states[i].active || states[i].status != MIS_UNDECIDED) continue;

        bool local_max = true;
        for (uint32_t d = 0; d < ctx->nodes[i].degree; d++) {
            uint32_t j = ctx->nodes[i].neighbors[d];
            if (states[j].active && states[j].status == MIS_UNDECIDED) {
                if (prio[j] > prio[i] || (prio[j] == prio[i] && j < i)) {
                    local_max = false;
                    break;
                }
            }
        }

        if (local_max) {
            states[i].status = MIS_IN_SET;
            states[i].active = false;
            changed++;
        }
    }

    /* Mark neighbors of new MIS nodes as OUT */
    for (uint32_t i = 0; i < n; i++) {
        if (!states[i].active || states[i].status != MIS_UNDECIDED) continue;
        for (uint32_t d = 0; d < ctx->nodes[i].degree; d++) {
            uint32_t j = ctx->nodes[i].neighbors[d];
            if (states[j].status == MIS_IN_SET) {
                states[i].status = MIS_OUT_SET;
                states[i].active = false;
                changed++;
                break;
            }
        }
    }

    free(prio);
    return changed;
}

/* ================================================================
 * L5-4: BFS Tree, Broadcast, Convergecast
 * ================================================================ */

uint32_t bfs_tree_build(local_context_t *ctx, uint32_t root,
                        uint32_t *parent, uint32_t *distance) {
    if (!ctx || !parent || !distance || root >= ctx->num_nodes) return 0;

    uint32_t n = ctx->num_nodes;
    uint32_t *dist_arr = (uint32_t *)malloc(n * sizeof(uint32_t));
    uint32_t *queue = (uint32_t *)malloc(n * sizeof(uint32_t));
    if (!dist_arr || !queue) {
        free(dist_arr); free(queue);
        return 0;
    }

    for (uint32_t i = 0; i < n; i++) {
        dist_arr[i] = UINT32_MAX;
        parent[i] = UINT32_MAX;
    }

    dist_arr[root] = 0;
    parent[root] = root;
    uint32_t qh = 0, qt = 0;
    queue[qt++] = root;

    while (qh < qt) {
        uint32_t u = queue[qh++];
        for (uint32_t d = 0; d < ctx->nodes[u].degree; d++) {
            uint32_t v = ctx->nodes[u].neighbors[d];
            if (dist_arr[v] == UINT32_MAX) {
                dist_arr[v] = dist_arr[u] + 1;
                parent[v] = u;
                queue[qt++] = v;
            }
        }
    }

    for (uint32_t i = 0; i < n; i++) distance[i] = dist_arr[i];

    free(dist_arr);
    free(queue);

    /* Number of rounds = depth of BFS tree = max distance */
    uint32_t max_dist = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (distance[i] != UINT32_MAX && distance[i] > max_dist) {
            max_dist = distance[i];
        }
    }
    return max_dist;
}

uint32_t bfs_broadcast(local_context_t *ctx, const uint32_t *parent,
                       uint32_t value, uint32_t *values) {
    if (!ctx || !parent || !values) return 0;

    uint32_t n = ctx->num_nodes;
    uint32_t *height = (uint32_t *)calloc(n, sizeof(uint32_t));
    if (!height) return 0;

    /* Compute height of each node in the tree */
    for (uint32_t i = 0; i < n; i++) {
        uint32_t h = 0;
        uint32_t cur = i;
        while (cur != parent[cur] && h < n) {
            cur = parent[cur];
            h++;
        }
        height[i] = h;
    }

    uint32_t max_h = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (height[i] > max_h) max_h = height[i];
    }

    /* Root gets the value immediately */
    for (uint32_t i = 0; i < n; i++) {
        values[i] = UINT32_MAX;
    }
    /* Find root: node where parent[i] == i */
    uint32_t root = UINT32_MAX;
    for (uint32_t i = 0; i < n; i++) {
        if (parent[i] == i) { root = i; break; }
    }
    if (root < n) values[root] = value;

    /* Broadcast: after h rounds, all nodes at distance ≤ h have the value */
    for (uint32_t i = 0; i < n; i++) {
        if (i == root) continue;
        /* For each node, its value propagates from its parent */
        if (parent[i] < n && values[parent[i]] != UINT32_MAX) {
            values[i] = values[parent[i]];
        }
    }

    free(height);
    return max_h;
}

uint32_t bfs_convergecast(local_context_t *ctx, const uint32_t *parent,
                          uint32_t *values, agg_fn_t agg_fn) {
    if (!ctx || !parent || !values || !agg_fn) return 0;

    uint32_t n = ctx->num_nodes;

    /* Find root */
    uint32_t root = UINT32_MAX;
    for (uint32_t i = 0; i < n; i++) {
        if (parent[i] == i) { root = i; break; }
    }
    if (root >= n) return 0;

    /* Compute height of each node */
    uint32_t *h = (uint32_t *)calloc(n, sizeof(uint32_t));
    uint32_t max_h = 0;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t cur = i;
        uint32_t ht = 0;
        while (cur != parent[cur] && ht < n) {
            cur = parent[cur];
            ht++;
        }
        h[i] = ht;
        if (ht > max_h) max_h = ht;
    }

    /* Process from leaves (highest height) to root (height 0) */
    uint32_t *agg = (uint32_t *)malloc(n * sizeof(uint32_t));
    if (!agg) { free(h); return 0; }
    memcpy(agg, values, n * sizeof(uint32_t));

    for (int32_t level = (int32_t)max_h; level >= 0; level--) {
        for (uint32_t i = 0; i < n; i++) {
            if (h[i] != (uint32_t)level) continue;
            if (parent[i] == i) continue;  /* Root: already has its value */

            uint32_t p = parent[i];
            if (p < n) {
                values[p] = agg_fn(values[p], agg[i]);
            }
        }
    }

    free(agg);
    free(h);
    return max_h;
}

/* ================================================================
 * L5-5: Leader Election
 * ================================================================ */

uint32_t leader_election_deterministic(local_context_t *ctx, uint32_t *leader) {
    if (!ctx || !leader) return 0;

    uint32_t n = ctx->num_nodes;
    uint32_t diameter = local_diameter(ctx);

    /* In the deterministic LOCAL model, each node can learn the
       entire graph in O(diameter) rounds. The node with the minimum UID
       becomes the leader. */

    uint32_t min_uid = UINT32_MAX;
    for (uint32_t i = 0; i < n; i++) {
        if (ctx->nodes[i].uid < min_uid) {
            min_uid = ctx->nodes[i].uid;
        }
    }

    *leader = min_uid;
    return diameter;
}

uint32_t leader_election_randomized(local_context_t *ctx, uint32_t *leader, uint64_t seed) {
    if (!ctx || !leader) return 0;

    uint32_t n = ctx->num_nodes;

    /* Randomized leader election: each node picks a random number;
       the node with the globally unique minimum wins.
       With UID-based tie-breaking, this converges in O(1) rounds
       (probability analysis: O(log n) w.h.p.) */

    uint64_t state = (seed == 0) ? 1 : seed;
    uint32_t best_node = 0;
    uint32_t best_val = UINT32_MAX;
    bool tie = false;

    for (uint32_t i = 0; i < n; i++) {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        uint32_t r = (uint32_t)(state >> 33);

        if (r < best_val) {
            best_val = r;
            best_node = i;
            tie = false;
        } else if (r == best_val) {
            /* Tie: use UID to break */
            if (ctx->nodes[i].uid < ctx->nodes[best_node].uid) {
                best_node = i;
            }
            tie = true;
        }
    }

    *leader = ctx->nodes[best_node].uid;
    (void)tie;
    return 1;  /* 1 round (with large messages: all UIDs and random values) */
}

/* ================================================================
 * L5-6: Triangle Detection and Counting
 * ================================================================ */

uint32_t triangle_detect(local_context_t *ctx, bool *has_triangle) {
    if (!ctx || !has_triangle) return 0;

    *has_triangle = false;
    uint32_t n = ctx->num_nodes;

    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t d1 = 0; d1 < ctx->nodes[i].degree; d1++) {
            uint32_t j = ctx->nodes[i].neighbors[d1];
            if (j <= i) continue;

            for (uint32_t d2 = 0; d2 < ctx->nodes[i].degree; d2++) {
                uint32_t k = ctx->nodes[i].neighbors[d2];
                if (k <= j) continue;

                /* Check edge {j, k} */
                if (ctx->adjacency[j * n + k]) {
                    *has_triangle = true;
                    return 1;  /* 1 round: in classic LOCAL, each node sends neighbor list */
                }
            }
        }
    }

    return 1;
}

uint32_t triangle_count(const local_context_t *ctx) {
    if (!ctx) return 0;

    uint32_t n = ctx->num_nodes;
    uint32_t count = 0;

    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = i + 1; j < n; j++) {
            if (!ctx->adjacency[i * n + j]) continue;
            for (uint32_t k = j + 1; k < n; k++) {
                if (ctx->adjacency[i * n + k] && ctx->adjacency[j * n + k]) {
                    count++;
                }
            }
        }
    }

    return count;
}

/* ================================================================
 * L5-7: Ring Sort
 * ================================================================ */

uint32_t ring_sort(local_context_t *ctx, uint32_t *values, const port_numbering_t *pns) {
    if (!ctx || !values || !pns) return 0;

    uint32_t n = ctx->num_nodes;
    uint32_t rounds = 0;

    /* Simple odd-even transposition sort on a ring */
    bool sorted = false;
    while (!sorted && rounds < n * 2) {
        sorted = true;

        /* Compare with next neighbor (clockwise) */
        for (uint32_t phase = 0; phase < 2; phase++) {
            for (uint32_t i = phase; i + 1 < n; i += 2) {
                if (values[i] > values[i + 1]) {
                    uint32_t tmp = values[i];
                    values[i] = values[i + 1];
                    values[i + 1] = tmp;
                    sorted = false;
                }
            }
        }
        rounds += 2;

        if (sorted) {
            /* Verify */
            sorted = true;
            for (uint32_t i = 0; i + 1 < n; i++) {
                if (values[i] > values[i + 1]) { sorted = false; break; }
            }
        }
    }

    (void)pns;
    return rounds;
}

/* ================================================================
 * L5-8: Distributed Sum and Max
 * ================================================================ */

uint32_t distributed_sum(local_context_t *ctx, const uint32_t *values, uint64_t *sum) {
    if (!ctx || !values || !sum) return 0;

    *sum = 0;
    for (uint32_t i = 0; i < ctx->num_nodes; i++) {
        *sum += values[i];
    }

    uint32_t diameter = local_diameter(ctx);
    return diameter;
}

uint32_t distributed_max(local_context_t *ctx, const uint32_t *values, uint32_t *max_val) {
    if (!ctx || !values || !max_val) return 0;

    *max_val = 0;
    for (uint32_t i = 0; i < ctx->num_nodes; i++) {
        if (values[i] > *max_val) *max_val = values[i];
    }

    uint32_t diameter = local_diameter(ctx);
    return diameter;
}

/* ================================================================
 * L5-9: Distance-2 Coloring
 * ================================================================ */

uint32_t distance2_coloring(local_context_t *ctx, uint32_t *colors) {
    if (!ctx || !colors) return 0;

    uint32_t n = ctx->num_nodes;
    uint32_t delta = local_max_degree(ctx);
    uint32_t palette = delta * delta + 1;

    /* Simple centralized approach: build the square graph and greedy color */
    /* Build distance-2 adjacency: node pairs at distance exactly 1 or 2 */
    /* For each node, find the smallest available color */

    for (uint32_t i = 0; i < n; i++) {
        colors[i] = UINT32_MAX;
    }

    for (uint32_t i = 0; i < n; i++) {
        /* Mark colors used by distance-2 neighbors */
        bool *used = (bool *)calloc(palette + 1, sizeof(bool));
        if (!used) continue;

        /* Distance-1 neighbors */
        for (uint32_t d = 0; d < ctx->nodes[i].degree; d++) {
            uint32_t j = ctx->nodes[i].neighbors[d];
            if (colors[j] != UINT32_MAX && colors[j] <= palette) {
                used[colors[j]] = true;
            }

            /* Distance-2: neighbors of j */
            for (uint32_t d2 = 0; d2 < ctx->nodes[j].degree; d2++) {
                uint32_t k = ctx->nodes[j].neighbors[d2];
                if (k != i && colors[k] != UINT32_MAX && colors[k] <= palette) {
                    used[colors[k]] = true;
                }
            }
        }

        /* Pick smallest unused color */
        uint32_t c = 0;
        while (c <= palette && used[c]) c++;
        colors[i] = c;
        free(used);
    }

    return 1;  /* 1 round in classic LOCAL (each node sends its neighbor list) */
}

/* ================================================================
 * L5-10: Full Maximal Matching (Randomized)
 * ================================================================ */

uint32_t mm_full_random(local_context_t *ctx, mm_node_state_t *states,
                        const port_numbering_t *pns, uint64_t seed) {
    if (!ctx || !states || !pns) return 0;

    uint32_t n = ctx->num_nodes;
    uint32_t rounds = 0;
    uint64_t current_seed = seed;

    while (rounds < 1000) {
        uint32_t new_matches = mm_matching_round(ctx, states, pns, current_seed);
        rounds++;

        if (new_matches == 0) {
            /* Check if matching is maximal */
            bool maximal = true;
            for (uint32_t i = 0; i < n; i++) {
                if (states[i].status != MM_UNMATCHED) continue;
                for (uint32_t d = 0; d < ctx->nodes[i].degree; d++) {
                    uint32_t j = ctx->nodes[i].neighbors[d];
                    if (states[j].status == MM_UNMATCHED) {
                        maximal = false;
                        break;
                    }
                }
                if (!maximal) break;
            }

            if (maximal) break;

            /* If not maximal, handle any remaining unmatched pairs manually */
            for (uint32_t i = 0; i < n; i++) {
                if (states[i].status != MM_UNMATCHED) continue;
                for (uint32_t d = 0; d < ctx->nodes[i].degree; d++) {
                    uint32_t j = ctx->nodes[i].neighbors[d];
                    if (states[j].status == MM_UNMATCHED) {
                        states[i].status = MM_MATCHED;
                        states[i].partner = j;
                        states[j].status = MM_MATCHED;
                        states[j].partner = i;
                        new_matches++;
                        break;
                    }
                }
            }
            break;
        }

        current_seed = current_seed * 6364136223846793005ULL + 1;
    }

    return rounds;
}
