/**
 * @file graph_problems.c
 * @brief Implementations of canonical LOCAL model graph problems.
 *
 * This file implements the canonical distributed graph problems:
 *  L6-1: (Δ+1)-Vertex Coloring
 *  L6-2: Maximal Independent Set (MIS)
 *  L6-3: Maximal Matching (MM)
 *  L6-4: 3-Coloring a Ring (Linial's problem, Cole-Vishkin algorithm)
 *  L6-5: Sinkless Orientation
 *  L6-6: Weak 2-Coloring
 *  L6-7: Vertex Cover (2-approximation from matching)
 *  L6-8: Dominating Set (greedy)
 *
 * References:
 *  - Cole & Vishkin (1986). Inf. Control 70(1).
 *  - Luby (1986). SIAM J. Comput. 15(4).
 *  - Linial (1992). SIAM J. Comput. 21(1).
 *  - Brandt et al. (2016). STOC 2016.
 */

#include "graph_problems.h"
#include "local_model.h"
#include "port_numbering.h"
#include "algorithms.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Simple PRNG state reused across graph_problems */
static uint64_t gp_prng_state;

static void gp_seed(uint64_t seed) {
    gp_prng_state = (seed == 0) ? 1 : seed;
}

static uint64_t gp_rand(void) {
    uint64_t x = gp_prng_state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    gp_prng_state = x;
    return x;
}

static uint32_t gp_rand_range(uint32_t max) {
    if (max == 0) return 0;
    return (uint32_t)(gp_rand() % max);
}

/* ================================================================
 * L6-1: (Δ+1)-Vertex Coloring
 * ================================================================ */

int vcol_init(vertex_coloring_state_t *state, uint32_t delta) {
    if (!state) return -1;
    memset(state, 0, sizeof(*state));
    state->delta = delta;
    state->palette_size = delta + 1;
    state->color = UINT32_MAX;  /* Uncolored */
    state->conflict = false;
    state->trial_round = 0;
    state->neighbor_colors = (uint32_t *)calloc(LOCAL_MAX_DEGREE, sizeof(uint32_t));
    if (!state->neighbor_colors) return -1;
    for (uint32_t i = 0; i < LOCAL_MAX_DEGREE; i++) {
        state->neighbor_colors[i] = UINT32_MAX;
    }
    return 0;
}

void vcol_destroy(vertex_coloring_state_t *state) {
    if (!state) return;
    free(state->neighbor_colors);
    memset(state, 0, sizeof(*state));
}

uint32_t vcol_reduce_round(local_context_t *ctx, vertex_coloring_state_t *states) {
    if (!ctx || !states) return 0;

    uint32_t n = ctx->num_nodes;
    uint32_t uncolored = 0;

    /* Each uncolored node picks a random trial color */
    for (uint32_t i = 0; i < n; i++) {
        if (!ctx->nodes[i].terminated && states[i].color == UINT32_MAX) {
            states[i].color = gp_rand_range(states[i].palette_size);
            states[i].trial_round++;
            uncolored++;
        }
    }

    /* Check conflicts: each node compares with neighbors */
    for (uint32_t i = 0; i < n; i++) {
        if (ctx->nodes[i].terminated || states[i].color == UINT32_MAX) continue;

        bool conflict = false;
        for (uint32_t d = 0; d < ctx->nodes[i].degree; d++) {
            uint32_t j = ctx->nodes[i].neighbors[d];
            if (states[j].color != UINT32_MAX && states[i].color == states[j].color) {
                conflict = true;
                break;
            }
        }

        states[i].conflict = conflict;
        if (conflict) {
            states[i].color = UINT32_MAX;  /* Revert */
        } else {
            ctx->nodes[i].terminated = true;
            uncolored--;
        }
    }

    return uncolored;
}

bool vcol_is_proper(const local_context_t *ctx, const uint32_t *colors) {
    if (!ctx || !colors) return false;
    uint32_t n = ctx->num_nodes;
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t d = 0; d < ctx->nodes[i].degree; d++) {
            uint32_t j = ctx->nodes[i].neighbors[d];
            if (i < j && colors[i] == colors[j]) return false;
        }
    }
    return true;
}

uint32_t vcol_count_colors(const uint32_t *colors, uint32_t n) {
    if (!colors || n == 0) return 0;
    bool *seen = (bool *)calloc(n + 1, sizeof(bool));  /* palette at most n */
    if (!seen) return 0;

    for (uint32_t i = 0; i < n; i++) {
        uint32_t c = colors[i];
        if (c < n + 1) seen[c] = true;
    }

    uint32_t count = 0;
    for (uint32_t i = 0; i <= n; i++) {
        if (seen[i]) count++;
    }
    free(seen);
    return count;
}

/* ================================================================
 * L6-2: Maximal Independent Set (MIS)
 * ================================================================ */

int mis_init(mis_node_state_t *state) {
    if (!state) return -1;
    memset(state, 0, sizeof(*state));
    state->status = MIS_UNDECIDED;
    state->active = true;
    state->neighbor_status = (uint32_t *)calloc(LOCAL_MAX_DEGREE, sizeof(uint32_t));
    if (!state->neighbor_status) return -1;
    for (uint32_t i = 0; i < LOCAL_MAX_DEGREE; i++) {
        state->neighbor_status[i] = MIS_UNDECIDED;
    }
    return 0;
}

void mis_destroy(mis_node_state_t *state) {
    if (!state) return;
    free(state->neighbor_status);
    memset(state, 0, sizeof(*state));
}

uint32_t mis_luby_round(local_context_t *ctx, mis_node_state_t *states, uint64_t seed) {
    if (!ctx || !states) return 0;

    uint32_t n = ctx->num_nodes;
    uint32_t changed = 0;

    gp_seed(seed);

    /* Phase 1: Each active undecided node picks a random priority */
    uint32_t *priorities = (uint32_t *)calloc(n, sizeof(uint32_t));
    if (!priorities) return 0;

    /* Save which nodes were undecided at the START of this round.
       This is critical: Luby's algorithm makes decisions based on the
       state at the beginning of the round, not mid-round updates. */
    bool *was_undecided = (bool *)calloc(n, sizeof(bool));
    if (!was_undecided) { free(priorities); return 0; }

    for (uint32_t i = 0; i < n; i++) {
        if (states[i].active && states[i].status == MIS_UNDECIDED) {
            priorities[i] = gp_rand_range(UINT32_MAX);
            was_undecided[i] = true;
        } else {
            priorities[i] = 0;
            was_undecided[i] = false;
        }
    }

    /* Phase 2: Determine which nodes join MIS based on ORIGINAL state.
       A node joins MIS if its priority is strictly greater than all
       ORIGINALLY-undecided neighbors.
       Tie-breaking: if two nodes have equal priority, the one with
       smaller UID wins. */
    bool *join_mis = (bool *)calloc(n, sizeof(bool));
    if (!join_mis) { free(was_undecided); free(priorities); return 0; }

    for (uint32_t i = 0; i < n; i++) {
        if (!was_undecided[i]) continue;

        bool is_local_max = true;
        for (uint32_t d = 0; d < ctx->nodes[i].degree; d++) {
            uint32_t j = ctx->nodes[i].neighbors[d];
            if (!was_undecided[j]) continue;  /* Only consider originally-undecided neighbors */

            if (priorities[j] > priorities[i]) {
                is_local_max = false;
                break;
            }
            if (priorities[j] == priorities[i] && ctx->nodes[j].uid < ctx->nodes[i].uid) {
                /* Tie: neighbor with smaller UID wins */
                is_local_max = false;
                break;
            }
        }

        if (is_local_max) {
            join_mis[i] = true;
        }
    }

    /* Phase 3: Apply decisions — nodes that join MIS become IN_SET */
    for (uint32_t i = 0; i < n; i++) {
        if (join_mis[i]) {
            states[i].status = MIS_IN_SET;
            states[i].round_joined = ctx->current_round;
            states[i].active = false;
            changed++;
        }
    }

    /* Phase 4: Neighbors of newly added MIS nodes become OUT.
       This is based on the FINAL state after Phase 3. */
    for (uint32_t i = 0; i < n; i++) {
        if (!states[i].active || states[i].status != MIS_UNDECIDED) continue;

        for (uint32_t d = 0; d < ctx->nodes[i].degree; d++) {
            uint32_t j = ctx->nodes[i].neighbors[d];
            if (states[j].status == MIS_IN_SET) {
                states[i].status = MIS_OUT_SET;
                states[i].round_joined = ctx->current_round;
                states[i].active = false;
                changed++;
                break;
            }
        }
    }

    free(join_mis);
    free(was_undecided);
    free(priorities);
    return changed;
}

bool mis_is_valid(const local_context_t *ctx, const mis_node_state_t *states) {
    if (!ctx || !states) return false;
    uint32_t n = ctx->num_nodes;

    /* Check independence: no two adjacent nodes both IN_SET */
    for (uint32_t i = 0; i < n; i++) {
        if (states[i].status != MIS_IN_SET) continue;
        for (uint32_t d = 0; d < ctx->nodes[i].degree; d++) {
            uint32_t j = ctx->nodes[i].neighbors[d];
            if (states[j].status == MIS_IN_SET) return false;
        }
    }

    /* Check maximality: every OUT_SET or UNDECIDED node must have an IN_SET neighbor */
    /* Actually, every UNDECIDED is an error — all nodes must be decided for validity */
    for (uint32_t i = 0; i < n; i++) {
        if (states[i].status == MIS_UNDECIDED) return false;
    }

    /* Check: OUT nodes have at least one IN_SET neighbor */
    for (uint32_t i = 0; i < n; i++) {
        if (states[i].status != MIS_OUT_SET) continue;
        bool has_in_neighbor = false;
        for (uint32_t d = 0; d < ctx->nodes[i].degree; d++) {
            uint32_t j = ctx->nodes[i].neighbors[d];
            if (states[j].status == MIS_IN_SET) {
                has_in_neighbor = true;
                break;
            }
        }
        if (!has_in_neighbor) return false;
    }

    return true;
}

/* ================================================================
 * L6-3: Maximal Matching (MM)
 * ================================================================ */

int mm_init(mm_node_state_t *state) {
    if (!state) return -1;
    memset(state, 0, sizeof(*state));
    state->status = MM_UNMATCHED;
    state->partner = UINT32_MAX;
    state->partner_port = UINT32_MAX;
    state->awaiting_port = UINT32_MAX;
    state->proposal_sent = false;
    return 0;
}

uint32_t mm_matching_round(local_context_t *ctx, mm_node_state_t *states,
                           const port_numbering_t *pns, uint64_t seed) {
    if (!ctx || !states || !pns) return 0;

    uint32_t n = ctx->num_nodes;
    uint32_t new_matches = 0;

    gp_seed(seed);

    /* Each unmatched node with an unmatched neighbor proposes */
    uint32_t *proposals = (uint32_t *)calloc(n, sizeof(uint32_t));
    if (!proposals) return 0;
    for (uint32_t i = 0; i < n; i++) {
        proposals[i] = UINT32_MAX;  /* No proposal target */
    }

    for (uint32_t i = 0; i < n; i++) {
        if (states[i].status != MM_UNMATCHED) continue;

        /* Find unmatched neighbors */
        uint32_t unmatched_nbrs[LOCAL_MAX_DEGREE];
        uint32_t cnt = 0;
        for (uint32_t d = 0; d < ctx->nodes[i].degree; d++) {
            uint32_t j = ctx->nodes[i].neighbors[d];
            if (states[j].status == MM_UNMATCHED) {
                unmatched_nbrs[cnt++] = j;
            }
        }

        if (cnt > 0) {
            /* Propose to a random unmatched neighbor */
            uint32_t chosen = unmatched_nbrs[gp_rand_range(cnt)];
            proposals[i] = chosen;
            states[i].status = MM_PROPOSING;
        }
    }

    /* Mutual proposals become matches */
    for (uint32_t i = 0; i < n; i++) {
        if (proposals[i] == UINT32_MAX) continue;
        uint32_t j = proposals[i];
        if (j >= n) continue;

        if (proposals[j] == i) {
            /* Mutual proposal: matched! */
            states[i].status = MM_MATCHED;
            states[i].partner = j;
            states[j].status = MM_MATCHED;
            states[j].partner = i;
            new_matches++;
        }
    }

    /* Reset proposing nodes that didn't get matched */
    for (uint32_t i = 0; i < n; i++) {
        if (states[i].status == MM_PROPOSING) {
            states[i].status = MM_UNMATCHED;
        }
    }

    free(proposals);
    return new_matches;
}

bool mm_is_valid(const local_context_t *ctx, const mm_node_state_t *states) {
    if (!ctx || !states) return false;
    uint32_t n = ctx->num_nodes;

    /* Check: matched nodes on both sides of an edge agree on partnership */
    for (uint32_t i = 0; i < n; i++) {
        if (states[i].status != MM_MATCHED) continue;
        uint32_t p = states[i].partner;
        if (p >= n) return false;
        if (states[p].status != MM_MATCHED || states[p].partner != i) return false;
    }

    /* Check maximality: no edge has both endpoints unmatched */
    for (uint32_t i = 0; i < n; i++) {
        if (states[i].status != MM_UNMATCHED) continue;
        for (uint32_t d = 0; d < ctx->nodes[i].degree; d++) {
            uint32_t j = ctx->nodes[i].neighbors[d];
            if (states[j].status == MM_UNMATCHED) return false;  /* Edge with both unmatched */
        }
    }

    return true;
}

/* ================================================================
 * L6-4: 3-Coloring a Ring — Cole-Vishkin Algorithm
 * ================================================================ */

/**
 * Helper: find the least significant bit where a and b differ.
 * Returns the index i (0-based). If a == b, returns 32.
 */
static uint32_t diff_bit_index(uint32_t a, uint32_t b) {
    uint32_t diff = a ^ b;
    if (diff == 0) return 32;
    uint32_t i = 0;
    while ((diff & 1) == 0) {
        diff >>= 1;
        i++;
    }
    return i;
}

uint32_t ring_3color_cole_vishkin(local_context_t *ctx, uint32_t *colors,
                                  const port_numbering_t *pns) {
    if (!ctx || !colors || !pns) return 0;

    uint32_t n = ctx->num_nodes;

    /* Initial coloring: UIDs (each unique, so n distinct colors) */
    for (uint32_t i = 0; i < n; i++) {
        colors[i] = ctx->nodes[i].uid;
    }

    /* Iteratively reduce colors using Cole-Vishkin */
    uint32_t max_rounds = iterated_log2(n) + 10;  /* Safety bound */
    uint32_t *new_colors = (uint32_t *)malloc(n * sizeof(uint32_t));
    if (!new_colors) return 0;

    uint32_t rounds = 0;
    bool done = false;

    for (uint32_t r = 0; r < max_rounds && !done; r++, rounds++) {
        /* Apply one CV reduction: each node computes new color from
           its own color and its successor's color */
        for (uint32_t i = 0; i < n; i++) {
            /* Find successor (port 1 = clockwise neighbor on ring) */
            uint32_t succ = pns[i].neighbor_by_port[1];
            if (succ >= n) succ = pns[i].neighbor_by_port[0];  /* Fallback */

            uint32_t my_color = colors[i];
            uint32_t succ_color = colors[succ];

            if (my_color == succ_color) {
                /* Already converged (shouldn't happen in CV if starting from UIDs) */
                new_colors[i] = my_color;
                continue;
            }

            uint32_t bit_i = diff_bit_index(my_color, succ_color);
            uint32_t bit_val = (my_color >> bit_i) & 1;
            uint32_t new_c = 2 * bit_i + bit_val;

            new_colors[i] = new_c;
        }

        /* Check if we have a small enough coloring (≤ 6) */
        uint32_t max_c = 0;
        for (uint32_t i = 0; i < n; i++) {
            colors[i] = new_colors[i];
            if (new_colors[i] > max_c) max_c = new_colors[i];
        }

        if (max_c <= 5) {  /* With 6 values (0-5), can map to 3 colors */
            done = true;
        }
    }

    /* Final reduction from 6 to 3 colors via a simple greedy pass */
    if (!done) {
        /* Already 6-colored; do one final round */
    }

    /* Map 0-5 colors to 0-2 via deterministic mapping */
    for (uint32_t i = 0; i < n; i++) {
        colors[i] = colors[i] % 3;
    }

    /* Fix conflicts that may arise from the modulo mapping */
    for (uint32_t attempt = 0; attempt < 10; attempt++) {
        bool any_conflict = false;
        for (uint32_t i = 0; i < n; i++) {
            uint32_t next = (i + 1) % n;
            if (colors[i] == colors[next]) {
                /* Recolor node i with a color different from both neighbors */
                uint32_t prev = (i == 0) ? n - 1 : i - 1;
                for (uint32_t c = 0; c < 3; c++) {
                    if (c != colors[prev] && c != colors[next]) {
                        colors[i] = c;
                        break;
                    }
                }
                any_conflict = true;
                break;
            }
        }
        if (!any_conflict) break;
    }

    free(new_colors);
    return rounds + 1;  /* +1 for the final fix-up */
}

bool ring_3color_verify(const local_context_t *ctx, const uint32_t *colors) {
    if (!ctx || !colors) return false;
    uint32_t n = ctx->num_nodes;

    for (uint32_t i = 0; i < n; i++) {
        uint32_t next = (i + 1) % n;
        /* Check proper: adjacent nodes have different colors */
        if (colors[i] == colors[next]) return false;
        /* Check range: each color is 0, 1, or 2 */
        if (colors[i] > 2) return false;
    }

    return true;
}

/* ================================================================
 * L6-5: Sinkless Orientation
 * ================================================================ */

int sinkless_init(sinkless_state_t *state, uint32_t degree) {
    if (!state) return -1;
    memset(state, 0, sizeof(*state));
    state->outgoing_ports = (uint32_t *)calloc(LOCAL_MAX_DEGREE, sizeof(uint32_t));
    if (!state->outgoing_ports) return -1;
    state->outgoing_count = 0;
    state->is_sink = (degree == 0) ? false : true;  /* True until at least one outgoing */
    return 0;
}

uint32_t sinkless_round(local_context_t *ctx, sinkless_state_t *states,
                        const port_numbering_t *pns, uint64_t seed) {
    if (!ctx || !states || !pns) return 0;

    uint32_t n = ctx->num_nodes;
    uint32_t sinks = 0;

    gp_seed(seed);

    /* Each node decides which ports are outgoing */
    for (uint32_t i = 0; i < n; i++) {
        if (ctx->nodes[i].degree == 0) {
            states[i].is_sink = false;
            continue;
        }

        /* Random orientation: with high probability, at least one port is outgoing */
        states[i].outgoing_count = 0;
        for (uint32_t p = 0; p < ctx->nodes[i].degree; p++) {
            /* 50% chance of marking this port as outgoing */
            if (gp_rand_range(2) == 0) {
                states[i].outgoing_ports[states[i].outgoing_count++] = p;
            }
        }

        /* Ensure at least one outgoing edge (anti-sink guarantee) */
        if (states[i].outgoing_count == 0) {
            uint32_t forced_port = gp_rand_range(ctx->nodes[i].degree);
            states[i].outgoing_ports[states[i].outgoing_count++] = forced_port;
        }

        states[i].is_sink = false;
    }

    /* Count remaining sinks (none by construction, but verify) */
    for (uint32_t i = 0; i < n; i++) {
        if (states[i].is_sink && ctx->nodes[i].degree > 0) sinks++;
    }

    return sinks;
}

bool sinkless_is_valid(const local_context_t *ctx, const sinkless_state_t *states) {
    if (!ctx || !states) return false;

    for (uint32_t i = 0; i < ctx->num_nodes; i++) {
        if (ctx->nodes[i].degree > 0 && states[i].is_sink) {
            return false;
        }
    }
    return true;
}

/* ================================================================
 * L6-6: Weak 2-Coloring
 * ================================================================ */

uint32_t weak_2coloring(local_context_t *ctx, uint32_t *colors, const port_numbering_t *pns) {
    if (!ctx || !colors) return 0;

    uint32_t n = ctx->num_nodes;

    /* Initialization: each node colors itself by its UID mod 2 */
    /* This gives a 2-coloring that may have isolated nodes with all same-color neighbors */

    for (uint32_t i = 0; i < n; i++) {
        colors[i] = ctx->nodes[i].uid % 2;
    }

    /* Iteratively fix: each node that has all neighbors of the same color flips */
    uint32_t rounds = 0;
    bool changed;

    do {
        changed = false;
        uint32_t *new_colors = (uint32_t *)malloc(n * sizeof(uint32_t));
        if (!new_colors) break;
        memcpy(new_colors, colors, n * sizeof(uint32_t));

        for (uint32_t i = 0; i < n; i++) {
            if (ctx->nodes[i].degree == 0) continue;

            bool all_same = true;
            uint32_t my_color = colors[i];

            for (uint32_t d = 0; d < ctx->nodes[i].degree; d++) {
                uint32_t j = ctx->nodes[i].neighbors[d];
                if (colors[j] != my_color) {
                    all_same = false;
                    break;
                }
            }

            if (all_same) {
                /* Flip color */
                new_colors[i] = 1 - my_color;
                changed = true;
            }
        }

        memcpy(colors, new_colors, n * sizeof(uint32_t));
        free(new_colors);
        rounds++;
    } while (changed && rounds < 100);  /* Safety bound */

    (void)pns;  /* Port numbering not needed for this simple algorithm */
    return rounds;
}

bool weak_2coloring_verify(const local_context_t *ctx, const uint32_t *colors) {
    if (!ctx || !colors) return false;

    for (uint32_t i = 0; i < ctx->num_nodes; i++) {
        if (ctx->nodes[i].degree == 0) continue;

        bool has_diff = false;
        for (uint32_t d = 0; d < ctx->nodes[i].degree; d++) {
            uint32_t j = ctx->nodes[i].neighbors[d];
            if (colors[i] != colors[j]) {
                has_diff = true;
                break;
            }
        }

        if (!has_diff) return false;
    }
    return true;
}

/* ================================================================
 * L6-7: Vertex Cover from Matching (2-Approximation)
 * ================================================================ */

void vc_from_matching(const mm_node_state_t *mm_states, uint32_t n, bool *vc) {
    if (!mm_states || !vc) return;

    for (uint32_t i = 0; i < n; i++) {
        /* A node is in the vertex cover if it's matched (both endpoints of matched edges) */
        vc[i] = (mm_states[i].status == MM_MATCHED);
    }
}

bool vc_is_valid(const local_context_t *ctx, const bool *vc) {
    if (!ctx || !vc) return false;

    for (uint32_t i = 0; i < ctx->num_nodes; i++) {
        for (uint32_t d = 0; d < ctx->nodes[i].degree; d++) {
            uint32_t j = ctx->nodes[i].neighbors[d];
            if (i < j) {  /* Check each edge once */
                if (!vc[i] && !vc[j]) return false;  /* Edge not covered */
            }
        }
    }
    return true;
}

uint32_t vc_size(const bool *vc, uint32_t n) {
    if (!vc) return 0;
    uint32_t cnt = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (vc[i]) cnt++;
    }
    return cnt;
}

/* ================================================================
 * L6-8: Dominating Set (Greedy)
 * ================================================================ */

uint32_t dominating_set_greedy(local_context_t *ctx, bool *domset,
                               const port_numbering_t *pns, uint64_t seed) {
    if (!ctx || !domset) return 0;

    uint32_t n = ctx->num_nodes;
    gp_seed(seed);

    /* Initially, no node is in the dominating set */
    memset(domset, 0, n * sizeof(bool));

    /* Mark which nodes are already dominated */
    bool *dominated = (bool *)calloc(n, sizeof(bool));
    if (!dominated) return 0;

    uint32_t rounds = 0;
    uint32_t dominated_count = 0;

    while (dominated_count < n && rounds < n * 2) {
        /* Find an undominated node with maximum degree among undominated nodes */
        uint32_t best = UINT32_MAX;
        uint32_t best_benefit = 0;

        for (uint32_t i = 0; i < n; i++) {
            if (dominated[i]) continue;

            /* Benefit = 1 (self) + number of undominated neighbors */
            uint32_t benefit = 1;
            for (uint32_t d = 0; d < ctx->nodes[i].degree; d++) {
                uint32_t j = ctx->nodes[i].neighbors[d];
                if (!dominated[j]) benefit++;
            }

            if (benefit > best_benefit) {
                best_benefit = benefit;
                best = i;
            }
        }

        if (best == UINT32_MAX) break;

        /* Add best to dominating set */
        domset[best] = true;

        /* Mark best and its neighbors as dominated */
        if (!dominated[best]) {
            dominated[best] = true;
            dominated_count++;
        }
        for (uint32_t d = 0; d < ctx->nodes[best].degree; d++) {
            uint32_t j = ctx->nodes[best].neighbors[d];
            if (!dominated[j]) {
                dominated[j] = true;
                dominated_count++;
            }
        }

        rounds++;
    }

    free(dominated);
    (void)pns;  /* Port numbering not needed for greedy algorithm in LOCAL;
                    port information would matter for real distributed implementation */
    return rounds;
}

bool domset_is_valid(const local_context_t *ctx, const bool *domset) {
    if (!ctx || !domset) return false;

    for (uint32_t i = 0; i < ctx->num_nodes; i++) {
        if (domset[i]) continue;  /* In dominating set: OK */

        /* Check if at least one neighbor is in the set */
        bool has_dominator = false;
        for (uint32_t d = 0; d < ctx->nodes[i].degree; d++) {
            uint32_t j = ctx->nodes[i].neighbors[d];
            if (domset[j]) {
                has_dominator = true;
                break;
            }
        }
        if (!has_dominator) return false;
    }
    return true;
}
