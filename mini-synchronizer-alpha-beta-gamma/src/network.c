/**
 * network.c — Distributed Network Implementation
 *
 * Implements the fundamental network graph data structure for
 * distributed computing simulations. Provides the underlying
 * graph operations needed by all three synchronizers.
 *
 * Knowledge coverage:
 *   L1: Network, node, edge definitions (C structs → runtime instances)
 *   L2: Asynchronous message-passing model
 *   L3: Graph adjacency list representation, FIFO channels
 *   L4: Graph diameter computation theorem (Floyd or BFS-based)
 */

#include "synchro.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

/*============================================================================
 * Message Queue Implementation
 *============================================================================*/

void msg_queue_init(msg_queue_t *q, size_t capacity) {
    q->head = NULL;
    q->tail = NULL;
    q->count = 0;
    q->capacity = capacity;
}

int msg_queue_enqueue(msg_queue_t *q, const synchronizer_msg_t *msg) {
    if (q->capacity > 0 && q->count >= q->capacity) {
        return -1; /* Queue full */
    }
    msg_queue_entry_t *entry = (msg_queue_entry_t *)malloc(sizeof(msg_queue_entry_t));
    if (!entry) return -1;
    memcpy(&entry->msg, msg, sizeof(synchronizer_msg_t));
    entry->next = NULL;
    if (q->tail) {
        q->tail->next = entry;
    } else {
        q->head = entry;
    }
    q->tail = entry;
    q->count++;
    return 0;
}

int msg_queue_dequeue(msg_queue_t *q, synchronizer_msg_t *msg) {
    if (!q->head) return -1;
    msg_queue_entry_t *old = q->head;
    memcpy(msg, &old->msg, sizeof(synchronizer_msg_t));
    q->head = old->next;
    if (!q->head) q->tail = NULL;
    free(old);
    q->count--;
    return 0;
}

int msg_queue_peek(const msg_queue_t *q, synchronizer_msg_t *msg) {
    if (!q->head) return -1;
    memcpy(msg, &q->head->msg, sizeof(synchronizer_msg_t));
    return 0;
}

int msg_queue_is_empty(const msg_queue_t *q) {
    return (q->count == 0);
}

size_t msg_queue_count(const msg_queue_t *q) {
    return q->count;
}

void msg_queue_destroy(msg_queue_t *q) {
    synchronizer_msg_t dummy;
    while (msg_queue_dequeue(q, &dummy) == 0) {
        /* dequeue frees each entry */
    }
    q->head = NULL;
    q->tail = NULL;
    q->count = 0;
}

/*============================================================================
 * Node-Level Message Operations
 *============================================================================*/

int node_send_message(network_t *net, node_id_t sender, node_id_t receiver,
                      msg_type_t type, round_t round, void *payload,
                      uint32_t payload_size) {
    if (!net || sender < 0 || sender >= net->num_nodes ||
        receiver < 0 || receiver >= net->num_nodes) {
        return -1;
    }
    synchronizer_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = type;
    msg.sender = sender;
    msg.receiver = receiver;
    msg.target = receiver;
    msg.round = round;
    msg.payload = payload;
    msg.payload_size = payload_size;
    msg.timestamp = (double)round;
    msg.hop_count = 1;

    int rc = msg_queue_enqueue(&net->nodes[receiver].incoming, &msg);
    if (rc == 0) {
        net->total_messages_sent++;
    }
    return rc;
}

int node_broadcast(network_t *net, node_id_t sender, msg_type_t type,
                   round_t round) {
    if (!net || sender < 0 || sender >= net->num_nodes) return -1;
    node_t *node = &net->nodes[sender];
    int sent = 0;
    for (neighbor_entry_t *nb = node->neighbors; nb != NULL; nb = nb->next) {
        if (node_send_message(net, sender, nb->neighbor_id, type, round,
                              NULL, 0) == 0) {
            sent++;
        }
    }
    return sent;
}

/*============================================================================
 * Node Internal Operations
 *============================================================================*/

/**
 * Remove all messages of a given type and round from the incoming queue.
 * Returns the number removed.
 */
int node_consume_messages(node_t *node, msg_type_t type, round_t round) {
    int removed = 0;
    msg_queue_entry_t *prev = NULL;
    msg_queue_entry_t *curr = node->incoming.head;
    while (curr) {
        if (curr->msg.type == type && curr->msg.round == round) {
            msg_queue_entry_t *to_free = curr;
            if (prev) {
                prev->next = curr->next;
            } else {
                node->incoming.head = curr->next;
            }
            curr = curr->next;
            if (to_free == node->incoming.tail) {
                node->incoming.tail = prev;
            }
            free(to_free);
            node->incoming.count--;
            removed++;
        } else {
            prev = curr;
            curr = curr->next;
        }
    }
    return removed;
}

/*============================================================================
 * Network Creation and Destruction
 *============================================================================*/

network_t *network_create(int n, synchro_type_t synchro_type) {
    if (n <= 0 || n > NODE_ID_MAX) return NULL;
    network_t *net = (network_t *)calloc(1, sizeof(network_t));
    if (!net) return NULL;

    net->num_nodes = n;
    net->num_edges = 0;
    net->synchro_type = synchro_type;
    net->is_directed = 0;
    net->diameter = 0;
    net->total_messages_sent = 0;
    net->total_rounds_executed = 0;
    net->total_time_units = 0.0;
    net->max_degree = 0;
    net->min_degree = 0;

    net->nodes = (node_t *)calloc((size_t)n, sizeof(node_t));
    if (!net->nodes) {
        free(net);
        return NULL;
    }

    for (int i = 0; i < n; i++) {
        net->nodes[i].id = (node_id_t)i;
        net->nodes[i].state = NODE_UNSAFE;
        net->nodes[i].current_round = 0;
        net->nodes[i].safe_count = 0;
        net->nodes[i].ack_count = 0;
        net->nodes[i].degree = 0;
        net->nodes[i].neighbors = NULL;
        msg_queue_init(&net->nodes[i].incoming, 0);
        msg_queue_init(&net->nodes[i].outgoing, 0);
        /* Beta fields */
        net->nodes[i].bfs_parent = NODE_ID_NONE;
        net->nodes[i].bfs_children_count = 0;
        net->nodes[i].bfs_children = NULL;
        net->nodes[i].safe_children = 0;
        net->nodes[i].depth = -1;
        /* Gamma fields */
        net->nodes[i].cluster_id = NODE_ID_NONE;
        net->nodes[i].cluster_size = 0;
        net->nodes[i].cluster_leader = NODE_ID_NONE;
        net->nodes[i].is_cluster_leader = 0;
        net->nodes[i].intercluster_nbrs = NULL;
        net->nodes[i].intercluster_count = 0;
        /* Application data */
        net->nodes[i].local_data = 0;
        net->nodes[i].local_sensor = 0.0;
    }
    return net;
}

/**
 * Add a neighbor to a node's adjacency list (internal helper).
 */
static int node_add_neighbor(node_t *node, node_id_t neighbor_id,
                              double weight) {
    /* Check if already exists */
    for (neighbor_entry_t *nb = node->neighbors; nb != NULL; nb = nb->next) {
        if (nb->neighbor_id == neighbor_id) {
            nb->edge_weight = weight;
            return 0; /* Updated existing */
        }
    }
    neighbor_entry_t *entry = (neighbor_entry_t *)malloc(sizeof(neighbor_entry_t));
    if (!entry) return -1;
    entry->neighbor_id = neighbor_id;
    entry->edge_weight = weight;
    entry->next = node->neighbors;
    node->neighbors = entry;
    node->degree++;
    return 0;
}

/**
 * Remove a neighbor from a node's adjacency list.
 */
static int node_remove_neighbor(node_t *node, node_id_t neighbor_id) {
    neighbor_entry_t *prev = NULL;
    neighbor_entry_t *curr = node->neighbors;
    while (curr) {
        if (curr->neighbor_id == neighbor_id) {
            if (prev) {
                prev->next = curr->next;
            } else {
                node->neighbors = curr->next;
            }
            free(curr);
            node->degree--;
            return 0;
        }
        prev = curr;
        curr = curr->next;
    }
    return -1; /* Not found */
}

int network_add_edge(network_t *net, node_id_t u, node_id_t v,
                     weight_t weight) {
    if (!net || u < 0 || u >= net->num_nodes ||
        v < 0 || v >= net->num_nodes || u == v) {
        return -1;
    }
    int existed = network_has_edge(net, u, v);
    if (node_add_neighbor(&net->nodes[u], v, weight) != 0) return -1;
    if (node_add_neighbor(&net->nodes[v], u, weight) != 0) return -1;

    if (!existed) {
        net->num_edges++;
    }
    /* Update degree stats */
    if (net->nodes[u].degree > net->max_degree)
        net->max_degree = net->nodes[u].degree;
    if (net->nodes[v].degree > net->max_degree)
        net->max_degree = net->nodes[v].degree;
    if (net->min_degree == 0 || net->nodes[u].degree < net->min_degree)
        net->min_degree = net->nodes[u].degree;
    if (net->nodes[v].degree < net->min_degree)
        net->min_degree = net->nodes[v].degree;
    return 0;
}

int network_remove_edge(network_t *net, node_id_t u, node_id_t v) {
    if (!net || u < 0 || u >= net->num_nodes ||
        v < 0 || v >= net->num_nodes) {
        return -1;
    }
    int r1 = node_remove_neighbor(&net->nodes[u], v);
    int r2 = node_remove_neighbor(&net->nodes[v], u);
    if (r1 == 0 && r2 == 0) {
        net->num_edges--;
        return 0;
    }
    return -1;
}

int network_has_edge(const network_t *net, node_id_t u, node_id_t v) {
    if (!net || u < 0 || u >= net->num_nodes ||
        v < 0 || v >= net->num_nodes) {
        return 0;
    }
    for (neighbor_entry_t *nb = net->nodes[u].neighbors; nb; nb = nb->next) {
        if (nb->neighbor_id == v) return 1;
    }
    return 0;
}

int network_node_degree(const network_t *net, node_id_t node) {
    if (!net || node < 0 || node >= net->num_nodes) return -1;
    return net->nodes[node].degree;
}

void network_destroy(network_t *net) {
    if (!net) return;
    for (int i = 0; i < net->num_nodes; i++) {
        /* Free neighbors */
        neighbor_entry_t *nb = net->nodes[i].neighbors;
        while (nb) {
            neighbor_entry_t *next = nb->next;
            free(nb);
            nb = next;
        }
        /* Free message queues */
        msg_queue_destroy(&net->nodes[i].incoming);
        msg_queue_destroy(&net->nodes[i].outgoing);
        /* Free beta fields */
        if (net->nodes[i].bfs_children) {
            free(net->nodes[i].bfs_children);
        }
        /* Free gamma fields */
        if (net->nodes[i].intercluster_nbrs) {
            free(net->nodes[i].intercluster_nbrs);
        }
    }
    free(net->nodes);
    free(net);
}

/*============================================================================
 * Network Diameter Computation (BFS from each node)
 *============================================================================*/

/**
 * BFS distance from a single source (returns max distance = eccentricity).
 * Uses a simple queue-based BFS.
 */
static int bfs_eccentricity(const network_t *net, node_id_t source) {
    int n = net->num_nodes;
    if (n == 0) return 0;

    int *dist = (int *)malloc((size_t)n * sizeof(int));
    int *queue = (int *)malloc((size_t)n * sizeof(int));
    if (!dist || !queue) {
        free(dist);
        free(queue);
        return -1;
    }
    for (int i = 0; i < n; i++) dist[i] = -1;

    int head = 0, tail = 0;
    dist[source] = 0;
    queue[tail++] = source;

    while (head < tail) {
        int u = queue[head++];
        for (neighbor_entry_t *nb = net->nodes[u].neighbors; nb; nb = nb->next) {
            if (dist[nb->neighbor_id] == -1) {
                dist[nb->neighbor_id] = dist[u] + 1;
                queue[tail++] = nb->neighbor_id;
            }
        }
    }

    int max_dist = 0;
    for (int i = 0; i < n; i++) {
        if (dist[i] > max_dist) max_dist = dist[i];
        if (dist[i] == -1) {
            /* Disconnected graph */
            max_dist = -1;
            break;
        }
    }
    free(dist);
    free(queue);
    return max_dist;
}

int network_compute_diameter(network_t *net) {
    if (!net) return -1;
    int diameter = 0;
    for (int i = 0; i < net->num_nodes; i++) {
        int ecc = bfs_eccentricity(net, (node_id_t)i);
        if (ecc < 0) {
            net->diameter = -1; /* Disconnected */
            return -1;
        }
        if (ecc > diameter) diameter = ecc;
    }
    net->diameter = diameter;
    return diameter;
}

void network_print_summary(const network_t *net) {
    if (!net) { printf("Network: NULL\n"); return; }
    printf("============================================\n");
    printf("Network Summary\n");
    printf("============================================\n");
    printf("  Nodes:           %d\n", net->num_nodes);
    printf("  Edges:           %d\n", net->num_edges);
    printf("  Synchronizer:    %s\n",
           net->synchro_type == SYNCHRO_ALPHA ? "Alpha" :
           net->synchro_type == SYNCHRO_BETA  ? "Beta"  : "Gamma");
    printf("  Directed:        %s\n", net->is_directed ? "Yes" : "No");
    printf("  Diameter:        %d\n", net->diameter);
    printf("  Max Degree:      %d\n", net->max_degree);
    printf("  Min Degree:      %d\n", net->min_degree);
    printf("  Total Msgs Sent: %zu\n", net->total_messages_sent);
    printf("  Rounds Executed: %zu\n", net->total_rounds_executed);
    printf("  Total Time:      %.2f\n", net->total_time_units);
    printf("============================================\n");
}

/*============================================================================
 * Complexity Tracking
 *============================================================================*/

void synchro_reset_counters(network_t *net) {
    if (!net) return;
    net->total_messages_sent = 0;
    net->total_rounds_executed = 0;
    net->total_time_units = 0.0;
}

size_t synchro_get_message_complexity(const network_t *net) {
    if (!net) return 0;
    return net->total_messages_sent;
}

double synchro_get_time_complexity(const network_t *net) {
    if (!net) return 0.0;
    return net->total_time_units;
}

void synchro_print_complexity_report(const network_t *net) {
    if (!net) return;
    printf("--------------------------------------------\n");
    printf("Complexity Report (%s Synchronizer)\n",
           net->synchro_type == SYNCHRO_ALPHA ? "Alpha" :
           net->synchro_type == SYNCHRO_BETA  ? "Beta"  : "Gamma");
    printf("--------------------------------------------\n");
    printf("  Theoretical bounds (per round):\n");
    switch (net->synchro_type) {
        case SYNCHRO_ALPHA:
            printf("    Messages: O(|E|) = O(%d)\n", net->num_edges);
            printf("    Time:     O(1)\n");
            break;
        case SYNCHRO_BETA:
            printf("    Messages: O(|V|) = O(%d)\n", net->num_nodes);
            printf("    Time:     O(|V|) = O(%d)\n", net->num_nodes);
            break;
        case SYNCHRO_GAMMA:
            printf("    Messages: O(k|V|) balanced with time O(|V|/k)\n");
            break;
    }
    printf("  Experimental results:\n");
    printf("    Total rounds:    %zu\n", net->total_rounds_executed);
    printf("    Total messages:  %zu\n", net->total_messages_sent);
    if (net->total_rounds_executed > 0) {
        printf("    Avg msg/round:   %.1f\n",
               (double)net->total_messages_sent /
               (double)net->total_rounds_executed);
    }
    printf("    Total time:      %.2f\n", net->total_time_units);
    printf("--------------------------------------------\n");
}

/*============================================================================
 * Graph Generators
 *============================================================================*/

network_t *network_gen_complete(int n, synchro_type_t st) {
    network_t *net = network_create(n, st);
    if (!net) return NULL;
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            network_add_edge(net, i, j, 1.0);
        }
    }
    network_compute_diameter(net);
    return net;
}

network_t *network_gen_cycle(int n, synchro_type_t st) {
    network_t *net = network_create(n, st);
    if (!net) return NULL;
    for (int i = 0; i < n; i++) {
        network_add_edge(net, i, (i + 1) % n, 1.0);
    }
    network_compute_diameter(net);
    return net;
}

network_t *network_gen_grid(int rows, int cols, synchro_type_t st) {
    int n = rows * cols;
    network_t *net = network_create(n, st);
    if (!net) return NULL;
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            int u = r * cols + c;
            if (c + 1 < cols) {
                network_add_edge(net, u, r * cols + (c + 1), 1.0);
            }
            if (r + 1 < rows) {
                network_add_edge(net, u, (r + 1) * cols + c, 1.0);
            }
        }
    }
    network_compute_diameter(net);
    return net;
}

network_t *network_gen_erdos_renyi(int n, double p, synchro_type_t st,
                                   unsigned int seed) {
    network_t *net = network_create(n, st);
    if (!net) return NULL;
    srand(seed);
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            double r = (double)rand() / (double)RAND_MAX;
            if (r < p) {
                network_add_edge(net, i, j, 1.0);
            }
        }
    }
    network_compute_diameter(net);
    return net;
}

network_t *network_gen_star(int n, synchro_type_t st) {
    network_t *net = network_create(n, st);
    if (!net) return NULL;
    for (int i = 1; i < n; i++) {
        network_add_edge(net, 0, i, 1.0);
    }
    network_compute_diameter(net);
    return net;
}

network_t *network_gen_barabasi_albert(int n, int m0, int m,
                                        synchro_type_t st, unsigned int seed) {
    /* Start with a complete graph of m0 nodes */
    network_t *net = network_create(n, st);
    if (!net) return NULL;
    if (m0 > n) m0 = n;
    if (m > m0) m = m0;

    /* Initialize seed graph as K_{m0} */
    for (int i = 0; i < m0; i++) {
        for (int j = i + 1; j < m0; j++) {
            network_add_edge(net, i, j, 1.0);
        }
    }

    int *degree_sum = (int *)calloc((size_t)n, sizeof(int));
    for (int i = 0; i < m0; i++) {
        degree_sum[i] = net->nodes[i].degree;
    }

    srand(seed);
    int total_degree = m0 * (m0 - 1); /* Sum of degrees for K_{m0} */

    /* Add nodes one by one */
    for (int i = m0; i < n; i++) {
        int added = 0;
        /* Build list of candidate targets */
        int *targets = (int *)malloc((size_t)i * sizeof(int));
        int t_idx = 0;
        for (int j = 0; j < i; j++) {
            if (added >= m) break;
            if (!network_has_edge(net, i, j)) {
                targets[t_idx++] = j;
            }
        }

        /* Preferential attachment: select m targets */
        while (added < m && t_idx > 0) {
            double r = (double)rand() / (double)RAND_MAX;
            double cumulative = 0.0;
            int chosen = -1;
            for (int k = 0; k < t_idx; k++) {
                int target = targets[k];
                cumulative += (double)net->nodes[target].degree /
                              (double)(total_degree > 0 ? total_degree : 1);
                if (r <= cumulative) {
                    chosen = target;
                    break;
                }
            }
            if (chosen < 0) chosen = targets[rand() % t_idx];

            if (!network_has_edge(net, i, chosen)) {
                network_add_edge(net, i, chosen, 1.0);
                total_degree += 2; /* Both endpoints gain a degree */
                /* Remove chosen from targets */
                for (int k = 0; k < t_idx; k++) {
                    if (targets[k] == chosen) {
                        targets[k] = targets[--t_idx];
                        break;
                    }
                }
                added++;
            } else {
                /* Already connected, remove and retry */
                for (int k = 0; k < t_idx; k++) {
                    if (targets[k] == chosen) {
                        targets[k] = targets[--t_idx];
                        break;
                    }
                }
            }
        }
        free(targets);
    }
    free(degree_sum);
    network_compute_diameter(net);
    return net;
}

network_t *network_gen_binary_tree(int height, synchro_type_t st) {
    /* Number of nodes in a complete binary tree of given height */
    int n = (1 << (height + 1)) - 1;
    network_t *net = network_create(n, st);
    if (!net) return NULL;
    /* Connect parent i to children 2i+1 and 2i+2 */
    for (int i = 0; i < n; i++) {
        int left = 2 * i + 1;
        int right = 2 * i + 2;
        if (left < n) network_add_edge(net, i, left, 1.0);
        if (right < n) network_add_edge(net, i, right, 1.0);
    }
    network_compute_diameter(net);
    return net;
}

network_t *network_gen_watts_strogatz(int n, int k, double p,
                                       synchro_type_t st, unsigned int seed) {
    network_t *net = network_create(n, st);
    if (!net) return NULL;
    if (k % 2 != 0) k++; /* Ensure k is even */

    /* Create ring lattice: each node connected to k nearest neighbors */
    int half_k = k / 2;
    for (int i = 0; i < n; i++) {
        for (int j = 1; j <= half_k; j++) {
            int neighbor = (i + j) % n;
            if (!network_has_edge(net, i, neighbor)) {
                network_add_edge(net, i, neighbor, 1.0);
            }
        }
    }

    /* Rewire edges with probability p */
    srand(seed);
    for (int i = 0; i < n; i++) {
        for (int j = 1; j <= half_k; j++) {
            double r = (double)rand() / (double)RAND_MAX;
            if (r < p) {
                int old_neighbor = (i + j) % n;
                /* Choose a new random target not already connected */
                int new_target;
                int attempts = 0;
                do {
                    new_target = rand() % n;
                    attempts++;
                } while ((new_target == i ||
                          network_has_edge(net, i, new_target)) &&
                         attempts < 100);
                if (attempts < 100 && new_target != i) {
                    network_remove_edge(net, i, old_neighbor);
                    network_add_edge(net, i, new_target, 1.0);
                }
            }
        }
    }
    network_compute_diameter(net);
    return net;
}

/*============================================================================
 * Node Incoming Message Processing
 *============================================================================*/

void node_process_incoming(network_t *net, node_t *node) {
    if (!net || !node) return;
    /* Process messages relevant to the current synchronizer type */
    synchronizer_msg_t msg;
    while (msg_queue_peek(&node->incoming, &msg) == 0) {
        int consumed = 0;
        if (msg.round < node->current_round) {
            /* Stale message, discard */
            msg_queue_dequeue(&node->incoming, &msg);
            consumed = 1;
        }
        if (!consumed) break; /* Leave remaining messages for next round */
    }
}
