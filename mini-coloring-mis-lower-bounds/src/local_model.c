/*
 * local_model.c -- LOCAL Model Simulator and Round Execution
 *
 * L1 Definitions: LOCAL model, synchronous rounds, message passing
 * L2 Core Concepts: locality, round-by-round simulation
 * L4 Fundamental Laws: locality lemma verification
 *
 * Reference: Peleg (2000), Suomela (2020)
 */

#include "local_model.h"
#include "distributed_constants.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

/* ── LOCAL Simulator ───────────────────────────────────────── */

LocalSimulator* local_simulator_create(Graph* g, const LocalConfig* cfg) {
    if (!g || !cfg) return NULL;
    LocalSimulator* sim = (LocalSimulator*)calloc(1, sizeof(LocalSimulator));
    if (!sim) return NULL;

    sim->graph = g;
    sim->config = *cfg;
    sim->current_round = 0;
    sim->all_terminated = false;

    /* Create port numbering */
    sim->port_num = port_numbering_create(g);
    if (!sim->port_num) { free(sim); return NULL; }

    /* Allocate node state array */
    sim->nodes = (LocalNodeState**)calloc((size_t)g->n,
                                           sizeof(LocalNodeState*));
    if (!sim->nodes) { local_simulator_free(sim); return NULL; }

    /* Create trace */
    sim->trace = trace_create(g->n, cfg->max_rounds + 10);
    if (!sim->trace) { local_simulator_free(sim); return NULL; }

    return sim;
}

void local_simulator_free(LocalSimulator* sim) {
    if (!sim) return;
    if (sim->nodes) {
        for (int i = 0; i < sim->graph->n; i++) {
            local_state_free(sim->nodes[i]);
        }
        free(sim->nodes);
    }
    port_numbering_free(sim->port_num);
    trace_free(sim->trace);
    /* Note: we do NOT free sim->graph (owned by caller) */
    free(sim);
}

void local_simulator_init(LocalSimulator* sim, const LocalAlgorithm* algo) {
    if (!sim || !algo || !algo->init) return;

    Graph* g = sim->graph;
    for (int v = 0; v < g->n; v++) {
        /* Create node state */
        int d = g->adjacency[v].degree;
        sim->nodes[v] = local_state_create(v, d, g->adjacency[v].neighbors);
        if (!sim->nodes[v]) continue;

        /* Call algorithm initializer */
        algo->init(sim->nodes[v], &sim->config, algo->algo_data);
    }
}

int local_simulator_round(LocalSimulator* sim, const LocalAlgorithm* algo) {
    if (!sim || !algo || !algo->round_fn) return -1;
    if (sim->all_terminated) return 0;
    if (sim->current_round >= sim->config.max_rounds) return -2;

    Graph* g = sim->graph;
    int n = g->n;

    /* Phase 1: each node computes and sends messages */
    int total_msgs = 0;
    for (int v = 0; v < n; v++) {
        LocalNodeState* node = sim->nodes[v];
        if (!node || node->terminated) continue;

        int msg_count;
        bool terminated;

        /* Allocate max possible messages for this round */
        Message* out_msgs = (Message*)calloc((size_t)(node->degree + 1),
                                              sizeof(Message));
        if (!out_msgs) continue;

        node->round = sim->current_round;
        algo->round_fn(node, &sim->config, algo->algo_data,
                       out_msgs, &msg_count, &terminated);

        if (terminated) node->terminated = true;

        /* Route messages to neighbors */
        for (int m = 0; m < msg_count; m++) {
            Message* msg = &out_msgs[m];
            int recipient = msg->recipient;
            if (recipient >= 0 && recipient < n && sim->nodes[recipient]) {
                msg->sender = v;
                local_state_receive(sim->nodes[recipient], msg);
                total_msgs++;
            }
        }
        free(out_msgs);
    }

    /* Phase 2: record trace */
    int* colors_snap = (int*)malloc((size_t)n * sizeof(int));
    bool* mis_snap = (bool*)malloc((size_t)n * sizeof(bool));
    int* msg_counts = (int*)calloc((size_t)n, sizeof(int));
    if (colors_snap && mis_snap && msg_counts) {
        for (int v = 0; v < n; v++) {
            LocalNodeState* node = sim->nodes[v];
            colors_snap[v] = node ? node->color : INVALID_COLOR;
            mis_snap[v] = node ? node->in_mis : false;
            msg_counts[v] = node ? node->inbox_size : 0;
        }
        trace_record_round(sim->trace, sim->current_round,
                          colors_snap, mis_snap, msg_counts);
    }
    free(colors_snap); free(mis_snap); free(msg_counts);

    sim->current_round++;

    /* Check if all terminated */
    bool all_term = true;
    for (int v = 0; v < n; v++) {
        if (sim->nodes[v] && !sim->nodes[v]->terminated) {
            all_term = false;
            break;
        }
    }
    sim->all_terminated = all_term;

    /* Clear inboxes for next round */
    for (int v = 0; v < n; v++) {
        if (sim->nodes[v]) local_state_clear_inbox(sim->nodes[v]);
    }

    return total_msgs;
}

int local_simulator_run(LocalSimulator* sim, const LocalAlgorithm* algo) {
    if (!sim || !algo) return -1;
    int rounds = 0;
    while (!sim->all_terminated && sim->current_round < sim->config.max_rounds) {
        int msgs = local_simulator_round(sim, algo);
        if (msgs < 0) break;
        rounds++;
    }
    return rounds;
}

int local_simulator_rounds(const LocalSimulator* sim) {
    return sim ? sim->current_round : 0;
}

Coloring* local_simulator_extract_coloring(const LocalSimulator* sim) {
    if (!sim) return NULL;
    Graph* g = sim->graph;
    Coloring* c = coloring_create(g->n);
    if (!c) return NULL;
    int max_c = -1;
    for (int v = 0; v < g->n; v++) {
        if (sim->nodes[v]) {
            c->colors[v] = sim->nodes[v]->color;
            if (c->colors[v] > max_c) max_c = c->colors[v];
        }
    }
    c->num_colors = max_c + 1;
    return c;
}

MIS* local_simulator_extract_mis(const LocalSimulator* sim) {
    if (!sim) return NULL;
    Graph* g = sim->graph;
    MIS* mis = mis_create(g->n);
    if (!mis) return NULL;
    int sz = 0;
    for (int v = 0; v < g->n; v++) {
        if (sim->nodes[v] && sim->nodes[v]->in_mis) {
            mis->in_mis[v] = true;
            sz++;
        }
    }
    mis->mis_size = sz;

    /* Fill dominator info */
    for (int v = 0; v < g->n; v++) {
        if (mis->in_mis[v]) continue;
        AdjList* al = &g->adjacency[v];
        for (int i = 0; i < al->degree; i++) {
            int nb = al->neighbors[i];
            if (mis->in_mis[nb]) {
                mis->dominator[v] = nb;
                break;
            }
        }
    }
    return mis;
}

void local_simulator_print_summary(const LocalSimulator* sim) {
    if (!sim) { printf("Simulator: NULL\n"); return; }
    printf("=== LOCAL Simulation Summary ===\n");
    printf("Graph: n=%d, m=%d, Delta=%d\n",
           sim->graph->n, sim->graph->m, sim->graph->max_degree);
    printf("Rounds executed: %d\n", sim->current_round);
    printf("All terminated: %s\n",
           sim->all_terminated ? "yes" : "no");

    int colored = 0, in_mis = 0, terminated = 0;
    for (int v = 0; v < sim->graph->n; v++) {
        LocalNodeState* node = sim->nodes[v];
        if (!node) continue;
        if (node->color >= 0) colored++;
        if (node->in_mis) in_mis++;
        if (node->terminated) terminated++;
    }
    printf("Colored: %d/%d\n", colored, sim->graph->n);
    printf("In MIS:  %d/%d\n", in_mis, sim->graph->n);
    printf("Terminated: %d/%d\n", terminated, sim->graph->n);
}

/* ── Message Routing ───────────────────────────────────────── */

void local_route_messages(LocalSimulator* sim) {
    /* Messages are already routed in local_simulator_round().
     * This function is provided for direct message injection
     * in custom simulation scenarios. */
    (void)sim;
}

void local_broadcast(LocalSimulator* sim, int node_id, MessageType type,
                     int value, int value2, double random_bits) {
    if (!sim || node_id < 0 || node_id >= sim->graph->n) return;
    AdjList* al = &sim->graph->adjacency[node_id];
    for (int i = 0; i < al->degree; i++) {
        Message msg;
        msg.type = type;
        msg.sender = node_id;
        msg.recipient = al->neighbors[i];
        msg.value = value;
        msg.value2 = value2;
        msg.random_bits = random_bits;
        if (sim->nodes[msg.recipient]) {
            local_state_receive(sim->nodes[msg.recipient], &msg);
        }
    }
}

void local_send_to_port(LocalSimulator* sim, int sender, int port,
                        MessageType type, int value, int value2) {
    if (!sim || !sim->port_num) return;
    int recipient = port_to_neighbor(sim->port_num, sender, port);
    if (recipient < 0) return;
    Message msg;
    msg.type = type;
    msg.sender = sender;
    msg.recipient = recipient;
    msg.value = value;
    msg.value2 = value2;
    msg.random_bits = 0.0;
    if (sim->nodes[recipient]) {
        local_state_receive(sim->nodes[recipient], &msg);
    }
}

/* ── LOCAL Model Properties ─────────────────────────────────── */

bool locality_lemma_holds(const Graph* g, int v1, int v2, int t,
                          const LocalAlgorithm* algo) {
    if (!g || !algo) return false;

    /* Build t-neighborhood views for both nodes */
    DistanceKView* view1 = neighborhood_view(g, v1, t);
    DistanceKView* view2 = neighborhood_view(g, v2, t);
    if (!view1 || !view2) {
        neighborhood_view_free(view1);
        neighborhood_view_free(view2);
        return false;
    }

    /* If views are isomorphic, deterministic algorithm must
     * produce same output. This is the locality lemma. */
    bool iso = neighborhood_views_isomorphic(view1, view2);
    neighborhood_view_free(view1);
    neighborhood_view_free(view2);
    return iso;
}

bool configurations_indistinguishable(const LocalNodeState* a,
                                       const LocalNodeState* b, int rounds) {
    if (!a || !b) return false;
    (void)rounds;
    /* For full indistinguishability in t rounds, need to check
     * that the t-neighborhood views are isomorphic and that
     * the port numbering and IDs are consistent. */
    /* Simplified: check basic equality of local state */
    if (a->degree != b->degree) return false;
    /* For now, return degree match as necessary condition */
    return true;
}

/* ── Round Complexity Bounds ────────────────────────────────── */

RoundComplexity* round_complexity_table(int* count) {
    static RoundComplexity table[] = {
        {"3-coloring ring (det)",    0.5, 0.5, true,
         "Cole-Vishkin (1986)", "Linial (1992)"},
        {"(Delta+1)-coloring (det)", 0.0, 0.0, false,
         "Barenboim-Elkin (2009)", "Kuhn-Wattenhofer (2006)"},
        {"(Delta+1)-coloring (rand)", 0.0, 0.0, false,
         "Johansson (1999)", "Chang-Pettie (2019)"},
        {"MIS (rand)",               0.0, 0.0, false,
         "Luby (1986), Ghaffari (2016)", "KMW (2004/2016)"},
        {"MIS (det)",                0.0, 0.0, false,
         "Linial (1992), Panconesi (1994)", "KMW (2004)"},
        {"Maximal matching (rand)",  0.0, 0.0, false,
         "Israeli-Itai (1986)", "KMW (2004)"},
    };
    *count = sizeof(table) / sizeof(table[0]);
    return table;
}

const RoundComplexity* round_complexity_lookup(const char* problem_name) {
    int count;
    RoundComplexity* table = round_complexity_table(&count);
    for (int i = 0; i < count; i++) {
        if (strstr(table[i].problem_name, problem_name))
            return &table[i];
    }
    return NULL;
}

/* ── Symmetry Breaking Primitives ───────────────────────────── */

bool symmetry_break_by_id(int my_id, int neighbor_id, bool smaller_wins) {
    if (smaller_wins) return my_id < neighbor_id;
    return my_id > neighbor_id;
}

bool symmetry_break_random(double my_rand, const double* neighbor_rands,
                           int n_neighbors, bool largest_wins) {
    if (!neighbor_rands && n_neighbors > 0) return false;
    for (int i = 0; i < n_neighbors; i++) {
        if (largest_wins) {
            if (neighbor_rands[i] > my_rand) return false;
            if (neighbor_rands[i] == my_rand && i < 0) return false;
        } else {
            if (neighbor_rands[i] < my_rand) return false;
            if (neighbor_rands[i] == my_rand && i < 0) return false;
        }
    }
    return true;
}

int symmetry_break_id_bits(int my_id, const int* neighbor_ids,
                           int n_neighbors, int round) {
    if (!neighbor_ids && n_neighbors > 0) return 0;
    int bit_pos = round;
    int my_bit = (my_id >> bit_pos) & 1;

    /* Check if my bit differs from all neighbors at this position */
    for (int i = 0; i < n_neighbors; i++) {
        if (neighbor_ids[i] >= 0) {
            int nb_bit = (neighbor_ids[i] >> bit_pos) & 1;
            if (nb_bit == my_bit) {
                /* Conflict: need to look at next bit */
                return -1;
            }
        }
    }
    return my_bit;
}

/* ── Round Complexity Table with Actual Values ──────────────── */

/* Fill in the actual asymptotic values for the table entries */
void round_complexity_fill_values(void) {
    int count;
    RoundComplexity* table = round_complexity_table(&count);
    if (count >= 6) {
        table[0].upper_bound_rounds = 0.5; /* log* n */
        table[0].lower_bound_rounds = 0.5; /* log* n; tight */
        table[0].tight = true;
        table[1].upper_bound_rounds = 100.0; /* Delta + log* n */
        table[1].lower_bound_rounds = 100.0; /* min(log Delta, sqrt(log n)) */
        table[2].upper_bound_rounds = 2.0;   /* O(sqrt(log Delta log log n) + poly(log log n)) */
        table[2].lower_bound_rounds = 24.5;  /* Omega(log* n) */
        table[3].upper_bound_rounds = 2.0;   /* O(log Delta + 2^O(sqrt(log log n))) */
        table[3].lower_bound_rounds = 24.5;  /* Omega(min{log Delta, sqrt(log n/Delta)}) */
        table[4].upper_bound_rounds = 100.0; /* 2^O(sqrt(log n)) */
        table[4].lower_bound_rounds = 24.5;  /* Omega(sqrt(log n / log log n)) */
        table[5].upper_bound_rounds = 0.5;  /* O(log n) rand */
        table[5].lower_bound_rounds = 0.5;  /* Omega(log n) */
    }
}
