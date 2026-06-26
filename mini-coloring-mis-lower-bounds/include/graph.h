/*
 * graph.h — Distributed Graph Data Structures & Neighborhood Operations
 *
 * L1 Definitions:
 *   - Graph G = (V, E), n = |V|, m = |E|
 *   - Node ID: unique integer identifier from [1..n] or custom ID space
 *   - Degree deg(v): number of neighbors of v
 *   - Maximum degree Δ = max_{v∈V} deg(v)
 *   - Distance dist(u,v): minimum number of edges on path from u to v
 *   - k-neighborhood N^k(v): all nodes at distance ≤ k from v
 *   - Graph diameter D = max_{u,v} dist(u,v)
 *   - Independence number α(G): size of largest independent set
 *   - Chromatic number χ(G): minimum colors needed for proper coloring
 *
 * L2 Core Concepts:
 *   - Distributed graph representation (each node knows only local topology)
 *   - Port numbering: each edge at node v assigned a unique port ∈ [0..deg(v)-1]
 *   - Symmetry breaking: nodes must decide based on local information
 *   - Network decomposition: partition into clusters of limited diameter
 *
 * L3 Mathematical Structures:
 *   - Adjacency list representation for local computation
 *   - Neighborhood matrices for lower bound arguments
 *   - Distance-k views (what a node can learn in k rounds)
 *
 * L6 Canonical Problems:
 *   - Maximal Independent Set (MIS)
 *   - (Δ+1)-vertex coloring
 *   - Maximal matching
 *   - Sinkless orientation
 *
 * Reference: Peleg "Distributed Computing: A Locality-Sensitive Approach" (2000)
 *            Barenboim & Elkin "Distributed Graph Coloring" (2013)
 *            Suomela "Distributed Algorithms" online textbook (2020)
 * Courses: MIT 6.852, ETH 263-4650, CMU 15-859
 */

#ifndef GRAPH_H
#define GRAPH_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

/* ── Basic Graph Types ─────────────────────────────────────── */

typedef struct {
    int* neighbors;
    int  degree;
    int  capacity;
} AdjList;

typedef struct {
    AdjList* adjacency;
    int      n;
    int      m;
    int      max_degree;
} Graph;

/* ── Port Numbering ────────────────────────────────────────── */

typedef struct {
    int**  port_map;
    int**  reverse_port;
    int*   degree;
    int    n;
} PortNumbered;

/* ── Neighborhood / Distance-k View ────────────────────────── */

typedef struct {
    int*  nodes;
    int*  distances;
    int*  parents;
    int   size;
    int   max_dist;
} DistanceKView;

/* ── Coloring & MIS State ──────────────────────────────────── */

typedef struct {
    int* colors;
    int  n;
    int  num_colors;
} Coloring;

typedef struct {
    bool* in_mis;
    int*  dominator;
    int   n;
    int   mis_size;
} MIS;

/* ── Distributed Round State ───────────────────────────────── */

typedef enum {
    MSG_PROPOSE_COLOR = 0,
    MSG_COLOR_CONFLICT,
    MSG_COLOR_ACCEPT,
    MSG_MIS_PROPOSE,
    MSG_MIS_CONFLICT,
    MSG_MIS_ACK,
    MSG_ID_BROADCAST,
    MSG_DEGREE_REPORT,
    MSG_ORIENT_REQUEST,
    MSG_ORIENT_RESPONSE,
    MSG_PALETTE_UPDATE,
    MSG_SYMMETRY_BREAK,
    MSG_TERMINATE,
    MSG_NUM_TYPES
} MessageType;

typedef struct {
    MessageType type;
    int         sender;
    int         recipient;
    int         value;
    int         value2;
    double      random_bits;
} Message;

typedef struct {
    int     node_id;
    int     degree;
    int     round;
    int     color;
    bool    in_mis;
    bool    terminated;
    int*    neighbor_colors;
    bool*   neighbor_in_mis;
    int*    neighbor_ids;
    int*    palette;
    int     palette_size;
    double  random_value;
    Message* inbox;
    int     inbox_size;
    int     inbox_capacity;
} LocalNodeState;

/* ── Distributed Execution ─────────────────────────────────── */

typedef struct {
    int    round;
    int*   colors;
    bool*  in_mis;
    int*   msg_counts;
} RoundSnapshot;

typedef struct {
    RoundSnapshot* rounds;
    int            n_rounds;
    int            capacity;
    int            n;
} ExecutionTrace;

/* ── Graph Construction & I/O ───────────────────────────────── */

Graph* graph_create(int n);
void   graph_add_edge(Graph* g, int u, int v);
void   graph_free(Graph* g);
Graph* graph_create_ring(int n);
Graph* graph_create_complete(int n);
Graph* graph_create_tree(const int* parent, int n);
Graph* graph_create_random_regular(int n, int d, uint64_t seed);
Graph* graph_create_bipartite(int a, int b);
Graph* graph_create_grid(int rows, int cols);
Graph* graph_create_bounded_degree(int n, int d, uint64_t seed);
void   graph_print_stats(const Graph* g);

/* ── Neighborhood Operations ────────────────────────────────── */

DistanceKView* neighborhood_view(const Graph* g, int v, int k);
void neighborhood_view_free(DistanceKView* view);
bool neighborhood_views_isomorphic(const DistanceKView* a, const DistanceKView* b);
int max_neighborhood_size(const Graph* g, int k);

/* ── Coloring Operations ────────────────────────────────────── */

Coloring* coloring_create(int n);
void coloring_free(Coloring* c);
bool coloring_is_proper(const Graph* g, const Coloring* c);
int  coloring_num_colors_used(const Coloring* c);

/* ── MIS Operations ─────────────────────────────────────────── */

MIS* mis_create(int n);
void mis_free(MIS* mis);
bool mis_is_independent(const Graph* g, const MIS* mis);
bool mis_is_maximal(const Graph* g, const MIS* mis);
bool mis_verify(const Graph* g, const MIS* mis);

/* ── Port Numbering Operations ──────────────────────────────── */

PortNumbered* port_numbering_create(const Graph* g);
void port_numbering_free(PortNumbered* pn);
int port_to_neighbor(const PortNumbered* pn, int v, int port);
int reverse_port_at_neighbor(const PortNumbered* pn, int v, int port);

/* ── LOCAL Model Simulation Support ─────────────────────────── */

LocalNodeState* local_state_create(int node_id, int degree, const int* neighbor_ids);
void local_state_free(LocalNodeState* st);
void local_state_receive(LocalNodeState* st, const Message* msg);
void local_state_clear_inbox(LocalNodeState* st);

/* ── Execution Trace Operations ─────────────────────────────── */

ExecutionTrace* trace_create(int n, int capacity);
void trace_record_round(ExecutionTrace* t, int round,
                        const int* colors, const bool* in_mis,
                        const int* msg_counts);
void trace_free(ExecutionTrace* t);

/* ── Graph Analysis ─────────────────────────────────────────── */

int graph_diameter(const Graph* g);
int graph_independence_number(const Graph* g);
int graph_chromatic_number(const Graph* g);
double graph_clustering_coeff(const Graph* g, int v);
double graph_avg_clustering(const Graph* g);
int graph_girth(const Graph* g);
bool graph_is_bipartite(const Graph* g);

/* ── Lower Bound Support Functions ──────────────────────────── */

int log_star(int n);
int count_distinct_k_views(const Graph* g, int k);
int max_component_size(const Graph* g);

#endif /* GRAPH_H */
