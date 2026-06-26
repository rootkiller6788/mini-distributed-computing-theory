/**
 * synchro.h — Core Synchronizer Definitions (α, β, γ)
 *
 * Reference: Baruch Awerbuch, "Complexity of Network Synchronization"
 *            Journal of the ACM, Vol. 32, No. 4, pp. 804-823, 1985.
 *
 * This header defines the fundamental data structures for the three
 * canonical synchronizers in distributed computing theory:
 *
 *   α (Alpha) Synchronizer — edge-based, O(|E|) messages/round, O(1) time/round
 *   β (Beta) Synchronizer  — tree-based, O(|V|) messages/round, O(|V|) time/round
 *   γ (Gamma) Synchronizer — cluster-based, balanced tradeoff
 *
 * Knowledge coverage:
 *   L1: Core definitions — synchronizer, pulse, safe state, round
 *   L2: Core concepts — synchronous simulation, message/time complexity tradeoff
 *   L3: Mathematical structures — distributed state machines, spanning trees
 */

#ifndef SYNCHRO_H
#define SYNCHRO_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*============================================================================
 * L1: Core Type Definitions
 *============================================================================*/

/** Node identifier in the distributed network */
typedef int32_t node_id_t;
#define NODE_ID_NONE  ((node_id_t)(-1))
#define NODE_ID_MAX   65536

/** Round number / pulse counter */
typedef int32_t round_t;
#define ROUND_ZERO    ((round_t)(0))
#define ROUND_MAX     ((round_t)(2147483647))

/** Edge weight for weighted networks */
typedef double weight_t;
#define WEIGHT_ZERO   ((weight_t)(0.0))
#define WEIGHT_INF    ((weight_t)(1e100))

/** Message type enumeration for synchronizer protocols */
typedef enum {
    MSG_SAFE         = 0,   /* "I am safe" — node has completed current round  */
    MSG_ACK          = 1,   /* "Acknowledged" — neighbor acknowledged safe    */
    MSG_NEXT_PULSE   = 2,   /* "Proceed to next round" — from root/leader     */
    MSG_SAFE_SUBTREE = 3,   /* "My subtree is safe" — convergecast signal     */
    MSG_NEXT_ROUND   = 4,   /* "Start next round" — broadcast down the tree   */
    MSG_CLUSTER_JOIN = 5,   /* Request to join a cluster (γ synchronizer)      */
    MSG_CLUSTER_ACK  = 6,   /* Cluster membership acknowledgment               */
    MSG_INTERCLUSTER = 7,   /* Inter-cluster message (γ synchronizer)          */
    MSG_BFS_INIT     = 8,   /* BFS tree construction initialization            */
    MSG_BFS_SEARCH   = 9,   /* BFS search/explore message                      */
    MSG_BFS_PARENT   = 10,  /* BFS parent assignment                           */
    MSG_HEARTBEAT    = 11,  /* Periodic heartbeat for fault detection           */
    MSG_RESET        = 12,  /* Reset message for self-stabilization             */
    MSG_APPLICATION  = 13,  /* Application-layer payload message                */
    MSG_SYNC_REQ     = 14,  /* Synchronization request                          */
    MSG_SYNC_RESP    = 15   /* Synchronization response                         */
} msg_type_t;

/** Synchronizer state for a single node */
typedef enum {
    NODE_UNSAFE       = 0,   /* Still processing current round                  */
    NODE_SAFE         = 1,   /* Finished current round, waiting for others      */
    NODE_WAITING      = 2,   /* Waiting for ACK/confirmation from neighbors     */
    NODE_READY        = 3,   /* Ready to proceed to next round                  */
    NODE_CRASHED      = 4    /* Node has crashed (fault model)                   */
} node_state_t;

/** Type of synchronizer */
typedef enum {
    SYNCHRO_ALPHA    = 0,    /* Edge-based synchronizer (O(|E|) msg, O(1) time) */
    SYNCHRO_BETA     = 1,    /* Tree-based synchronizer (O(|V|) msg, O(|V|) time)*/
    SYNCHRO_GAMMA    = 2     /* Cluster-based hybrid synchronizer                */
} synchro_type_t;

/*============================================================================
 * L3: Message Structure
 *============================================================================*/

/** A synchronizer protocol message */
typedef struct {
    msg_type_t   type;            /* Type of synchronizer message                */
    node_id_t    sender;          /* Sending node ID                             */
    node_id_t    receiver;        /* Receiving node ID                           */
    node_id_t    target;          /* Ultimate target (for routing)               */
    round_t      round;           /* Round number this message pertains to       */
    uint32_t     payload_size;    /* Size of application payload                 */
    void        *payload;         /* Optional application data                   */
    double       timestamp;       /* Logical timestamp                           */
    uint32_t     hop_count;       /* Hops traveled                               */
} synchronizer_msg_t;

/** Message queue entry (linked list node for per-node message buffers) */
typedef struct msg_queue_entry {
    synchronizer_msg_t     msg;
    struct msg_queue_entry *next;
} msg_queue_entry_t;

/** Per-node message queue (FIFO channel abstraction) */
typedef struct {
    msg_queue_entry_t *head;
    msg_queue_entry_t *tail;
    size_t             count;
    size_t             capacity;  /* 0 = unlimited                              */
} msg_queue_t;

/*============================================================================
 * L3: Network Node Structure
 *============================================================================*/

/** Adjacency list entry for a network neighbor */
typedef struct neighbor_entry {
    node_id_t               neighbor_id;
    double                  edge_weight;    /* For weighted network algorithms   */
    struct neighbor_entry  *next;
} neighbor_entry_t;

/** A single distributed node (processor) in the network */
typedef struct node {
    node_id_t        id;               /* Unique node identifier                 */
    node_state_t     state;            /* Current synchronizer state             */
    round_t          current_round;    /* Current round/pulse number             */
    round_t          safe_count;       /* Number of "safe" messages received     */
    round_t          ack_count;        /* Number of ACK messages received        */
    int              degree;           /* Total number of neighbors              */
    neighbor_entry_t *neighbors;       /* Adjacency list                         */
    msg_queue_t      incoming;         /* Incoming message queue                 */
    msg_queue_t      outgoing;         /* Outgoing message buffer                */
    /* Beta synchronizer specific fields */
    node_id_t        bfs_parent;       /* Parent in BFS tree (beta sync)         */
    int              bfs_children_count; /* Number of children in BFS tree       */
    node_id_t       *bfs_children;     /* Array of children (beta sync)          */
    int              safe_children;    /* Count of children reporting safe       */
    int              depth;            /* Depth in BFS tree                      */
    /* Gamma synchronizer specific fields */
    node_id_t        cluster_id;       /* Cluster this node belongs to (gamma)   */
    int              cluster_size;     /* Size of local cluster                  */
    node_id_t        cluster_leader;   /* Leader of this cluster (gamma)         */
    int              is_cluster_leader;/* Whether this node is a cluster leader  */
    node_id_t       *intercluster_nbrs; /* Neighbors in other clusters (gamma)   */
    int              intercluster_count;/* Number of inter-cluster neighbors     */
    /* Application-level data to demonstrate protocol execution */
    int              local_data;       /* Demonstration data value               */
    double           local_sensor;     /* Sensor reading (for applications)      */
} node_t;

/*============================================================================
 * L3: Network Structure (Distributed System Model)
 *============================================================================*/

/** The distributed network: a graph G = (V, E) with n nodes */
typedef struct {
    node_t     *nodes;          /* Array of n nodes in the network               */
    int         num_nodes;      /* |V| — number of vertices                     */
    int         num_edges;      /* |E| — number of edges (undirected count)      */
    synchro_type_t synchro_type;/* Which synchronizer is active                  */
    int         is_directed;    /* Whether edges are directed (0=undirected)     */
    int         diameter;       /* Diameter of the network graph                 */
    /* Complexity tracking */
    size_t      total_messages_sent;    /* Total messages across all rounds      */
    size_t      total_rounds_executed;   /* Total synchronous rounds simulated    */
    double      total_time_units;       /* Accumulated time complexity           */
    /* Statistics */
    int         max_degree;      /* Maximum degree across all nodes              */
    int         min_degree;      /* Minimum degree across all nodes              */
} network_t;

/*============================================================================
 * L2: Synchronous Protocol Abstraction
 *============================================================================*/

/**
 * A synchronous protocol is a function applied by each node at each round.
 * The synchronizer's job is to simulate these synchronous rounds on an
 * asynchronous network.
 *
 * @param net    The network
 * @param node   The node executing the protocol
 * @param round  The current synchronous round
 * @return 0 on success, non-zero if the protocol wishes to halt
 */
typedef int (*sync_protocol_fn)(network_t *net, node_t *node, round_t round);

/*============================================================================
 * L4: Core Synchronizer API
 *============================================================================*/

/**
 * Initialize a new distributed network with n nodes.
 * Complexity: O(n).
 *
 * @param n           Number of nodes
 * @param synchro_type Which synchronizer to use
 * @return Allocated network, or NULL on failure
 */
network_t *network_create(int n, synchro_type_t synchro_type);

/**
 * Add an undirected edge between two nodes in the network.
 * If the edge already exists, update its weight.
 * Complexity: O(d) where d is the degree of node u.
 *
 * @param net    Network
 * @param u      First node
 * @param v      Second node
 * @param weight Edge weight (use 1.0 for unweighted)
 * @return 0 on success, -1 on error
 */
int network_add_edge(network_t *net, node_id_t u, node_id_t v, weight_t weight);

/**
 * Remove an edge between two nodes.
 * Complexity: O(d_u + d_v).
 *
 * @return 0 on success, -1 if edge doesn't exist
 */
int network_remove_edge(network_t *net, node_id_t u, node_id_t v);

/**
 * Check if an edge exists between two nodes.
 * Complexity: O(min(d_u, d_v)).
 *
 * @return 1 if edge exists, 0 otherwise
 */
int network_has_edge(const network_t *net, node_id_t u, node_id_t v);

/**
 * Get the degree (number of neighbors) of a node.
 * Complexity: O(1).
 */
int network_node_degree(const network_t *net, node_id_t node);

/**
 * Free all resources associated with a network.
 */
void network_destroy(network_t *net);

/**
 * Compute the graph diameter using BFS from each node.
 * Complexity: O(|V| * (|V| + |E|)).
 */
int network_compute_diameter(network_t *net);

/**
 * Print a summary of the network topology.
 */
void network_print_summary(const network_t *net);

/*============================================================================
 * L5: Synchronizer-Specific Initialization
 *============================================================================*/

/**
 * Initialize all nodes for Alpha synchronizer operation.
 * Every node sets up its safe/ack counters for edge-based synchronization.
 * Complexity: O(|V|).
 *
 * Theorem (Awerbuch 1985): The Alpha synchronizer simulates each synchronous
 * round using exactly 2|E| messages and O(1) time.
 */
void synchro_alpha_init(network_t *net);

/**
 * Initialize all nodes for Beta synchronizer operation.
 * First constructs a BFS spanning tree rooted at node 0, then sets up
 * tree-based convergecast/broadcast structures.
 * Complexity: O(|V| + |E|) for BFS tree construction.
 *
 * Theorem (Awerbuch 1985): The Beta synchronizer simulates each synchronous
 * round using O(|V|) messages and O(depth(tree)) = O(|V|) time.
 */
int synchro_beta_init(network_t *net);

/**
 * Initialize all nodes for Gamma synchronizer operation.
 * Partitions the network into clusters of size approximately k,
 * builds BFS trees within each cluster, and sets up inter-cluster edges.
 * Complexity: O(|V| + |E|).
 *
 * Theorem (Awerbuch 1985): With cluster size k, the Gamma synchronizer
 * simulates each round using O(k|V|) messages and O(|V|/k) time.
 * Choosing k = log|V| balances these to O(|V| log|V|) messages and time.
 *
 * @param net         Network
 * @param cluster_size Target size for each cluster (use 0 for auto-detect)
 */
int synchro_gamma_init(network_t *net, int cluster_size);

/*============================================================================
 * L5: Round Simulation (the core synchronizer operation)
 *============================================================================*/

/**
 * Execute one synchronous round using the active synchronizer.
 * This is the main entry point: it dispatches to the appropriate
 * Alpha, Beta, or Gamma implementation.
 *
 * @param net      Network with active synchronizer
 * @param protocol The synchronous protocol to execute at each node
 * @return Number of messages exchanged during this round, -1 on error
 */
int synchro_execute_round(network_t *net, sync_protocol_fn protocol);

/**
 * Run multiple synchronous rounds until the protocol signals completion.
 *
 * @param net       Network with active synchronizer
 * @param protocol  The synchronous protocol
 * @param max_rounds Maximum rounds to execute (safety bound)
 * @return Total rounds executed, -1 on error
 */
int synchro_run_protocol(network_t *net, sync_protocol_fn protocol,
                         int max_rounds);

/*============================================================================
 * L5: Individual Synchronizer Implementations
 *============================================================================*/

/**
 * Alpha synchronizer: execute one round with edge-based synchronization.
 *
 * Algorithm:
 *   Phase 1 (Local): Each node calls protocol(net, node, round).
 *     When done, node enters SAFE state.
 *   Phase 2 (Safe propagation): Each SAFE node sends MSG_SAFE to all neighbors.
 *   Phase 3 (Ack): When a node receives MSG_SAFE from ALL neighbors,
 *     it sends MSG_ACK to all neighbors.
 *   Phase 4 (Proceed): When a node receives MSG_ACK from ALL neighbors,
 *     it increments round and proceeds.
 *
 * Message complexity: 2|E| messages per round.
 * Time complexity: O(1) (4 phases, each takes 1 message delay).
 */
int alpha_execute_round(network_t *net, sync_protocol_fn protocol);

/**
 * Beta synchronizer: execute one round with tree-based synchronization.
 *
 * Algorithm:
 *   Phase 1 (Local): Each node calls protocol at current round.
 *   Phase 2 (Convergecast): Leaf nodes send MSG_SAFE_SUBTREE to parent when
 *     locally SAFE. Internal nodes wait for all children + local SAFE,
 *     then send MSG_SAFE_SUBTREE to parent.
 *   Phase 3 (Broadcast): Root sends MSG_NEXT_PULSE down the BFS tree when
 *     it receives from all children and is locally SAFE. Nodes propagate
 *     MSG_NEXT_PULSE to all children, then increment round.
 *
 * Message complexity: 2(|V|-1) messages per round (O(|V|)).
 * Time complexity: O(depth) = O(|V|) per round.
 */
int beta_execute_round(network_t *net, sync_protocol_fn protocol);

/**
 * Gamma synchronizer: execute one round with cluster-based synchronization.
 *
 * Uses Beta synchronizer within each cluster and Alpha synchronizer
 * between clusters. The cluster tree structure balances message and
 * time complexity.
 *
 * Message complexity: O(|V| * k) where k is cluster size.
 * Time complexity: O(depth of cluster tree).
 */
int gamma_execute_round(network_t *net, sync_protocol_fn protocol);

/*============================================================================
 * L6: Message Queue Operations
 *============================================================================*/

/**
 * Initialize a message queue.
 * Complexity: O(1).
 */
void msg_queue_init(msg_queue_t *q, size_t capacity);

/**
 * Enqueue a message (FIFO ordering).
 * Complexity: O(1).
 */
int msg_queue_enqueue(msg_queue_t *q, const synchronizer_msg_t *msg);

/**
 * Dequeue a message (FIFO ordering).
 * Complexity: O(1).
 * @return 0 on success, -1 if queue is empty
 */
int msg_queue_dequeue(msg_queue_t *q, synchronizer_msg_t *msg);

/**
 * Peek at the front message without removing it.
 * @return 0 on success, -1 if queue is empty
 */
int msg_queue_peek(const msg_queue_t *q, synchronizer_msg_t *msg);

/**
 * Check if the queue is empty.
 */
int msg_queue_is_empty(const msg_queue_t *q);

/**
 * Get the number of messages in the queue.
 */
size_t msg_queue_count(const msg_queue_t *q);

/**
 * Process all messages in a node's incoming queue, dispatching to
 * the appropriate handler based on message type and synchronizer mode.
 * Complexity: O(m) where m = queue size.
 */
void node_process_incoming(network_t *net, node_t *node);

/**
 * Remove all messages of a given type and round from a node's incoming queue.
 * This is used during round cleanup across all synchronizer implementations.
 * Returns the number of messages removed.
 * Complexity: O(m) where m = queue size.
 */
int node_consume_messages(node_t *node, msg_type_t type, round_t round);

/**
 * Send a message from one node to another (enqueue in receiver's incoming).
 * Complexity: O(1).
 */
int node_send_message(network_t *net, node_id_t sender, node_id_t receiver,
                      msg_type_t type, round_t round, void *payload,
                      uint32_t payload_size);

/**
 * Broadcast a message from one node to all its neighbors.
 * Complexity: O(deg(node)).
 */
int node_broadcast(network_t *net, node_id_t sender, msg_type_t type,
                   round_t round);

/**
 * Free all resources in a message queue.
 */
void msg_queue_destroy(msg_queue_t *q);

/*============================================================================
 * Complexity Analysis API
 *============================================================================*/

/**
 * Reset the message and time counters for a fresh measurement.
 */
void synchro_reset_counters(network_t *net);

/**
 * Get the total message complexity of all executed rounds.
 */
size_t synchro_get_message_complexity(const network_t *net);

/**
 * Get the time complexity estimate (rounds × per-round time).
 */
double synchro_get_time_complexity(const network_t *net);

/**
 * Print a detailed complexity report.
 */
void synchro_print_complexity_report(const network_t *net);

/*============================================================================
 * Graph Generation Utilities
 *============================================================================*/

/**
 * Generate a complete graph K_n.
 */
network_t *network_gen_complete(int n, synchro_type_t st);

/**
 * Generate an n-node cycle graph C_n.
 */
network_t *network_gen_cycle(int n, synchro_type_t st);

/**
 * Generate an n×m grid graph.
 */
network_t *network_gen_grid(int rows, int cols, synchro_type_t st);

/**
 * Generate a random graph G(n, p) using the Erdos-Renyi model.
 * Each of the n(n-1)/2 possible edges is included with probability p.
 *
 * @param n Number of nodes
 * @param p Edge probability (0.0 to 1.0)
 * @param st Synchronizer type
 * @param seed Random seed
 */
network_t *network_gen_erdos_renyi(int n, double p, synchro_type_t st,
                                   unsigned int seed);

/**
 * Generate a star graph with n nodes (node 0 is center).
 */
network_t *network_gen_star(int n, synchro_type_t st);

/**
 * Generate a Barabasi-Albert scale-free network using preferential attachment.
 * Starts with m0 nodes, adds nodes one-by-one connecting to m existing nodes.
 */
network_t *network_gen_barabasi_albert(int n, int m0, int m,
                                        synchro_type_t st, unsigned int seed);

/**
 * Generate a binary tree network with given height.
 */
network_t *network_gen_binary_tree(int height, synchro_type_t st);

/**
 * Generate a Watts-Strogatz small-world network.
 * Starts with a ring lattice of n nodes each connected to k neighbors,
 * then rewires each edge with probability p.
 */
network_t *network_gen_watts_strogatz(int n, int k, double p,
                                       synchro_type_t st, unsigned int seed);

#endif /* SYNCHRO_H */
