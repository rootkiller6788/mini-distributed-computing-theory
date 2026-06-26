/*
 * local_model.h — LOCAL Model of Distributed Computing
 *
 * L1 Definitions:
 *   - LOCAL model: synchronous message-passing in a network graph G=(V,E)
 *   - Each node v has a unique Theta(log n)-bit identifier ID(v)
 *   - Initially, each node knows: n, Delta (or bound), its own ID, its degree
 *   - In each synchronous round, each node:
 *       1. Sends a message to each neighbor (possibly distinct messages)
 *       2. Receives messages from all neighbors
 *       3. Performs arbitrary local computation
 *   - LOCAL model: message size is unbounded
 *   - CONGEST model: message size bounded by O(log n) bits
 *   - Round complexity T(G): number of rounds until all nodes terminate
 *
 * L2 Core Concepts:
 *   - Locality: a node's output after t rounds depends only on its t-neighborhood
 *   - Port numbering model: edges incident to v numbered 1..deg(v)
 *   - Synchronous execution: all nodes proceed in lockstep
 *   - Termination detection: node may output and stop participating
 *   - Randomized vs deterministic algorithms
 *
 * L3 Mathematical Structures:
 *   - Round as a function mapping local state x messages -> new state x messages
 *   - Configuration = vector of all node states
 *   - Execution = sequence of configurations C_0 -> C_1 -> ... -> C_T
 *
 * L4 Fundamental Laws:
 *   - Locality lemma: after t rounds, node v's output depends only on N^t(v)
 *   - Linial's Omega(log* n) lower bound for O(1)-coloring a ring
 *   - Kuhn-Moscibroda-Wattenhofer lower bounds for MIS
 *
 * Reference: Linial (1992) "Locality in Distributed Graph Algorithms"
 *            Peleg (2000) "Distributed Computing"
 *            Suomela (2013) "Distributed Algorithms" (online)
 * Courses: ETH 263-4650, MIT 6.852, CMU 15-859
 */

#ifndef LOCAL_MODEL_H
#define LOCAL_MODEL_H

#include "graph.h"
#include <stdbool.h>
#include <stdint.h>

/* ── LOCAL Model Configuration ─────────────────────────────── */

typedef enum {
    MODEL_LOCAL = 0,
    MODEL_CONGEST,
    MODEL_BROADCAST_CON
} DistributedModel;

typedef enum {
    ALGO_DETERMINISTIC = 0,
    ALGO_RANDOMIZED_MONTECARLO,
    ALGO_RANDOMIZED_LASVEGAS
} AlgorithmType;

typedef struct {
    DistributedModel model;
    AlgorithmType    algo_type;
    int              n;
    int              max_degree;
    int              max_rounds;
    uint64_t         random_seed;
    bool             verbose;
} LocalConfig;

/* ── Distributed Algorithm Signature ────────────────────────── */

struct LocalAlgorithm;

typedef void (*InitFn)(LocalNodeState* node, const LocalConfig* cfg, void* algo_data);
typedef void (*RoundFn)(LocalNodeState* node, const LocalConfig* cfg,
                        void* algo_data, Message* messages_out,
                        int* messages_count, bool* terminated);

typedef struct LocalAlgorithm {
    InitFn    init;
    RoundFn   round_fn;
    void*     algo_data;
    char*     name;
} LocalAlgorithm;

/* ── LOCAL Model Simulator ─────────────────────────────────── */

typedef struct {
    Graph*            graph;
    PortNumbered*     port_num;
    LocalNodeState**  nodes;
    LocalConfig       config;
    ExecutionTrace*   trace;
    int               current_round;
    bool              all_terminated;
} LocalSimulator;

/* ── Simulator API ──────────────────────────────────────────── */

LocalSimulator* local_simulator_create(Graph* g, const LocalConfig* cfg);
void local_simulator_free(LocalSimulator* sim);
void local_simulator_init(LocalSimulator* sim, const LocalAlgorithm* algo);
int  local_simulator_round(LocalSimulator* sim, const LocalAlgorithm* algo);
int  local_simulator_run(LocalSimulator* sim, const LocalAlgorithm* algo);
int  local_simulator_rounds(const LocalSimulator* sim);
Coloring* local_simulator_extract_coloring(const LocalSimulator* sim);
MIS* local_simulator_extract_mis(const LocalSimulator* sim);
void local_simulator_print_summary(const LocalSimulator* sim);

/* ── Message Routing ───────────────────────────────────────── */

void local_route_messages(LocalSimulator* sim);
void local_broadcast(LocalSimulator* sim, int node_id, MessageType type,
                     int value, int value2, double random_bits);
void local_send_to_port(LocalSimulator* sim, int sender, int port,
                        MessageType type, int value, int value2);

/* ── LOCAL Model Properties ─────────────────────────────────── */

bool locality_lemma_holds(const Graph* g, int v1, int v2, int t,
                          const LocalAlgorithm* algo);
bool configurations_indistinguishable(const LocalNodeState* a,
                                      const LocalNodeState* b, int rounds);

/* ── Distance-t Neighborhood Graph ──────────────────────────── */

typedef struct {
    int             n;
    DistanceKView*  views;
    bool**          edges;
    int*            view_of;
} NeighborhoodGraph;

/* ── Round Complexity Bounds ────────────────────────────────── */

typedef struct {
    char*   problem_name;
    double  upper_bound_rounds;
    double  lower_bound_rounds;
    bool    tight;
    char*   upper_reference;
    char*   lower_reference;
} RoundComplexity;

RoundComplexity* round_complexity_table(int* count);
const RoundComplexity* round_complexity_lookup(const char* problem_name);

/* ── Symmetry Breaking Primitives ───────────────────────────── */

bool symmetry_break_by_id(int my_id, int neighbor_id, bool smaller_wins);
bool symmetry_break_random(double my_rand, const double* neighbor_rands,
                           int n_neighbors, bool largest_wins);
int  symmetry_break_id_bits(int my_id, const int* neighbor_ids,
                            int n_neighbors, int round);

#endif /* LOCAL_MODEL_H */
