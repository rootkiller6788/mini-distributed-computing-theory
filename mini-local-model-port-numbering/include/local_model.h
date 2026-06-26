/**
 * @file local_model.h
 * @brief LOCAL Model of Distributed Computing
 *
 * The LOCAL model (Linial, 1987, 1992) is the fundamental synchronous
 * message-passing model in distributed computing. In this model:
 * - The network is an undirected graph G = (V, E) with n = |V| nodes.
 * - Each node has a unique identifier (UID) from {1, ..., poly(n)}.
 * - Computation proceeds in synchronous rounds.
 * - In each round, each node:
 *   1. Sends a message to each neighbor.
 *   2. Receives messages from all neighbors.
 *   3. Performs arbitrary local computation.
 * - There is NO limit on message size or local computation time.
 * - The measure of complexity is the number of rounds until all nodes terminate.
 *
 * References:
 * - Linial, N. (1987). "Distributive graph algorithms: Global solutions from local data."
 * - Linial, N. (1992). "Locality in distributed graph algorithms."
 * - Peleg, D. (2000). "Distributed Computing: A Locality-Sensitive Approach."
 * - Barenboim & Elkin (2013). "Distributed Graph Coloring: Fundamentals and Recent Developments."
 *
 * Level: L1 (Definitions), L2 (Core Concepts), L3 (Mathematical Structures)
 */

#ifndef LOCAL_MODEL_H
#define LOCAL_MODEL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ================================================================
 * L1: Core Definitions — Node, Neighborhood, Network
 * ================================================================ */

/** Maximum number of nodes in a LOCAL network instance. */
#define LOCAL_MAX_NODES         1024

/** Maximum degree of any node. */
#define LOCAL_MAX_DEGREE        256

/** Sentinel for unlimited message size in the LOCAL model. */
#define LOCAL_MSG_UNBOUNDED     ((size_t)(-1))

/**
 * @brief LOCAL model node.
 *
 * Each node has:
 * - uid: unique identifier (per LOCAL model assumption)
 * - degree: number of incident edges
 * - neighbors[degree]: list of neighbor node indices
 * - ports[degree]: port numbers assigned to incident edges (see port_numbering.h)
 * - local_state: arbitrary pointer for per-node storage
 * - terminated: whether this node has halted
 */
typedef struct {
    uint32_t uid;                           /* Unique identifier */
    uint32_t degree;                        /* Number of neighbors (0 <= degree < LOCAL_MAX_DEGREE) */
    uint32_t neighbors[LOCAL_MAX_DEGREE];  /* Indices of neighboring nodes */
    uint32_t ports[LOCAL_MAX_DEGREE];      /* Port numbers for incident edges */
    void    *local_state;                   /* Opaque pointer for per-node internal state */
    bool     terminated;                    /* Has the node halted? */
} local_node_t;

/**
 * @brief A message exchanged between neighbors in one round.
 *
 * In the LOCAL model, messages have no size bound; here we use a
 * flexible buffer to represent arbitrary message content.
 */
typedef struct {
    uint32_t sender_uid;    /* UID of the sending node */
    uint32_t receiver_uid;  /* UID of the receiving node */
    uint32_t sender_port;   /* Port through which the message was sent */
    uint32_t round;         /* Round number in which this message was sent */
    size_t   length;        /* Length of payload in bytes */
    uint8_t *payload;       /* Arbitrary message content (freed by caller) */
} local_message_t;

/**
 * @brief Round-based execution context for the LOCAL model.
 *
 * Tracks the current round, number of nodes, the communication graph,
 * and per-round message buffers.
 */
typedef struct {
    uint32_t  num_nodes;                    /* Total number of nodes, n = |V| */
    uint32_t  current_round;               /* Current round number (0-based or 1-based depending on algorithm) */
    bool     *adjacency;                    /* n×n adjacency matrix (row-major) */
    local_node_t *nodes;                    /* Array of n nodes */

    /* Per-round send/receive buffers (reset each round) */
    local_message_t **outgoing;             /* out[i] = list of messages node i sends this round */
    size_t   *outgoing_count;              /* Number of messages node i sends */

    /* Termination tracking */
    uint32_t  terminated_count;            /* How many nodes have halted */
} local_context_t;

/* ================================================================
 * L2: Core Concepts — Synchronous Execution, Neighborhood,
 *     Round Complexity, Deterministic vs Randomized
 * ================================================================ */

/**
 * @brief Initialize a LOCAL context with n nodes and a given adjacency matrix.
 *
 * Each node is assigned a UID from 1 to n. Adjacency is symmmetric
 * and irreflexive (no self-loops).
 *
 * @param ctx           Uninitialized context pointer (allocated by caller).
 * @param n             Number of nodes (1 <= n <= LOCAL_MAX_NODES).
 * @param adjacency     Row-major n×n bool array (adj[i*n + j] == true iff edge {i,j}).
 * @return              0 on success, -1 on error.
 *
 * Complexity: O(n^2) local precomputation.
 */
int local_context_init(local_context_t *ctx, uint32_t n, const bool *adjacency);

/**
 * @brief Free all resources held by the LOCAL context.
 * @param ctx   Initialized context to destroy.
 */
void local_context_destroy(local_context_t *ctx);

/**
 * @brief Get the set of neighbors of node u within distance r (r-neighborhood).
 *
 * In the LOCAL model, after r rounds, each node knows its entire r-neighborhood.
 * This function computes N^r(u) = {v | dist(u, v) <= r}.
 *
 * @param ctx           LOCAL context.
 * @param u             Query node index.
 * @param r             Radius (number of rounds / hops).
 * @param out_neighbors Output array (caller allocates, size >= n).
 * @param out_count     Number of nodes in N^r(u).
 * @return              0 on success.
 *
 * Theorem: After r rounds in the LOCAL model, node u can simulate knowing
 * its entire r-neighborhood (i.e., the subgraph induced by N^r(u)).
 * This is the fundamental port-numbering-free fact about the LOCAL model.
 */
int local_r_neighborhood(const local_context_t *ctx, uint32_t u, uint32_t r,
                         uint32_t *out_neighbors, uint32_t *out_count);

/**
 * @brief Execute one synchronous round of the LOCAL model.
 *
 * Each non-terminated node:
 * 1. Calls its round function (provided as callback), which may read
 *    the node's current state and outgoing buffer from the previous round,
 *    and must fill the outgoing buffer for the current round.
 * 2. The framework delivers all messages to neighbors.
 * 3. The round counter is incremented.
 *
 * @param ctx       LOCAL context.
 * @param round_fn  Per-node function: (ctx, node_idx) → (void)
 *                  Must fill ctx->outgoing[node_idx] and set outgoing_count.
 */
typedef void (*local_round_fn_t)(local_context_t *ctx, uint32_t node_idx);
int local_execute_round(local_context_t *ctx, local_round_fn_t round_fn);

/**
 * @brief Run the LOCAL model for t rounds with a given per-round function.
 *
 * @param ctx       LOCAL context.
 * @param t         Number of rounds to execute.
 * @param round_fn  Per-node round function.
 * @return          Number of rounds actually executed (may be < t if all nodes terminate early).
 */
uint32_t local_execute_rounds(local_context_t *ctx, uint32_t t, local_round_fn_t round_fn);

/**
 * @brief Compute the shortest path distance between two nodes.
 *
 * Uses BFS within the adjacency matrix. In the LOCAL model, computing
 * exact distances requires diameter-many rounds in the worst case.
 *
 * @param ctx   LOCAL context.
 * @param u, v  Node indices.
 * @return      Shortest path distance, or UINT32_MAX if unreachable.
 */
uint32_t local_distance(const local_context_t *ctx, uint32_t u, uint32_t v);

/**
 * @brief Compute the diameter of the communication graph.
 *
 * The diameter is the maximum shortest-path distance between any pair of nodes.
 * @param ctx   LOCAL context.
 * @return      Graph diameter.
 */
uint32_t local_diameter(const local_context_t *ctx);

/**
 * @brief Check if the graph is connected.
 * @return true iff the graph consists of a single connected component.
 */
bool local_is_connected(const local_context_t *ctx);

/* ================================================================
 * L3: Mathematical Structures — Graph Representations
 * ================================================================ */

/**
 * @brief Edge list representation (useful for storing input graph topology).
 */
typedef struct {
    uint32_t u, v;      /* Endpoints */
    double   weight;    /* Weights (for weighted problems, weight=1.0 for unweighted) */
} local_edge_t;

/**
 * @brief Build the adjacency matrix from an edge list.
 *
 * @param ctx           LOCAL context (nodes must already be allocated).
 * @param edges         Array of edges.
 * @param num_edges     Number of edges.
 * @return              0 on success.
 */
int local_build_from_edges(local_context_t *ctx, const local_edge_t *edges, size_t num_edges);

/**
 * @brief Generate a path graph P_n (n nodes in a line).
 *
 * Useful for lower bound constructions: many LOCAL lower bounds are proven on rings and paths.
 *
 * @param ctx   LOCAL context (will be initialized internally).
 * @param n     Number of nodes.
 * @return      0 on success.
 */
int local_make_path(local_context_t *ctx, uint32_t n);

/**
 * @brief Generate a cycle graph C_n (n nodes forming a ring).
 *
 * The ring is the most fundamental topology for LOCAL model lower bounds.
 * Linial (1992) proved that 3-coloring a ring requires Ω(log* n) rounds.
 *
 * @param ctx   LOCAL context (will be initialized internally).
 * @param n     Number of nodes (>= 3).
 * @return      0 on success.
 */
int local_make_cycle(local_context_t *ctx, uint32_t n);

/**
 * @brief Generate a d-regular tree truncated at depth h.
 *
 * Regular trees are essential for understanding the information flow
 * in the LOCAL model: after r rounds, each node sees a tree of depth r.
 *
 * @param ctx   LOCAL context (will be initialized internally).
 * @param d     Degree (>= 2).
 * @param h     Depth/height of the tree.
 * @return      0 on success.
 */
int local_make_regular_tree(local_context_t *ctx, uint32_t d, uint32_t h);

/**
 * @brief Generate a random graph G(n, p) (Erdos-Renyi model).
 *
 * @param ctx   LOCAL context (will be initialized internally).
 * @param n     Number of nodes.
 * @param p     Edge probability (0.0 <= p <= 1.0).
 * @param seed  Random seed for reproducibility.
 * @return      0 on success.
 */
int local_make_random_graph(local_context_t *ctx, uint32_t n, double p, uint64_t seed);

/**
 * @brief Generate a complete graph K_n.
 *
 * In K_n, every node sees the entire graph after 1 round, making all
 * problems trivial from a locality perspective.
 *
 * @param ctx   LOCAL context.
 * @param n     Number of nodes.
 * @return      0 on success.
 */
int local_make_clique(local_context_t *ctx, uint32_t n);

/* ================================================================
 * L2 (cont): Graph Properties
 * ================================================================ */

/**
 * @brief Compute the maximum degree Δ of the graph.
 * @return Maximum degree.
 */
uint32_t local_max_degree(const local_context_t *ctx);

/**
 * @brief Compute the minimum degree δ of the graph.
 * @return Minimum degree.
 */
uint32_t local_min_degree(const local_context_t *ctx);

/**
 * @brief Compute the average degree of the graph.
 * @return Average degree as double.
 */
double local_avg_degree(const local_context_t *ctx);

/**
 * @brief Compute the number of edges in the graph.
 * @return Edge count.
 */
uint32_t local_edge_count(const local_context_t *ctx);

/**
 * @brief Compute the girth (length of the shortest cycle) of a graph.
 *
 * The girth is a fundamental parameter in LOCAL lower bounds.
 * Graphs with high girth look locally like trees, enabling round
 * elimination arguments.
 *
 * @param ctx   LOCAL context.
 * @return      Girth (UINT32_MAX if acyclic).
 */
uint32_t local_girth(const local_context_t *ctx);

/**
 * @brief Compute the chromatic number χ(G) (minimum colors needed for proper coloring).
 *
 * This is NP-hard in general but computable for small n via brute force.
 * The LOCAL model studies how many rounds are needed to find a (Δ+1)-coloring
 * or other approximate colorings.
 *
 * @param ctx   LOCAL context.
 * @return      Chromatic number.
 */
uint32_t local_chromatic_number(const local_context_t *ctx);

/* ================================================================
 * L3: Synchronous Execution State Machine
 * ================================================================ */

/**
 * @brief States of the LOCAL execution state machine.
 */
typedef enum {
    LOCAL_STATE_INIT,           /* Context allocated, graph set */
    LOCAL_STATE_RUNNING,        /* Rounds being executed */
    LOCAL_STATE_ALL_TERMINATED, /* All nodes have halted */
    LOCAL_STATE_ERROR           /* An error occurred */
} local_exec_state_t;

/**
 * @brief Get the current execution state.
 */
local_exec_state_t local_get_state(const local_context_t *ctx);

/**
 * @brief Reset the LOCAL context to round 0, clearing all node states
 *        but preserving the graph structure.
 */
void local_reset(local_context_t *ctx);

/**
 * @brief Clone a local node (deep copy, including local_state if a copy function is provided).
 *
 * @param dst      Destination node.
 * @param src      Source node.
 * @param copy_fn  If non-NULL, called to copy src->local_state to dst->local_state.
 *                 If NULL, dst->local_state = src->local_state (shallow copy).
 */
typedef void *(*local_copy_fn_t)(const void *state);
void local_node_copy(local_node_t *dst, const local_node_t *src, local_copy_fn_t copy_fn);

#endif /* LOCAL_MODEL_H */
