/**
 * gamma_synchronizer.c — Gamma (γ) Synchronizer Implementation
 *
 * Reference: Baruch Awerbuch, "Complexity of Network Synchronization"
 *            Journal of the ACM, Vol. 32, No. 4, pp. 804-823, 1985.
 *
 * The Gamma synchronizer combines Alpha and Beta into a hierarchical
 * approach. The network is partitioned into clusters; within each cluster
 * a Beta synchronizer is used, while Alpha is used between clusters.
 * This balances message and time complexity.
 *
 * Knowledge coverage:
 *   L1: Gamma synchronizer definition, cluster, inter-cluster edge
 *   L2: Hierarchical synchronization, multi-level tradeoff
 *   L3: Graph partitioning, cluster trees
 *   L4: Theorem: With cluster size k, Γ uses O(k|V|) messages and
 *       O(|V|/k) time per round. Choosing k = log|V| gives O(|V|log|V|)
 *       messages and O(|V|/log|V|) time.
 *   L5: Algorithm — greedy clustering, intra-cluster Beta, inter-cluster Alpha
 *
 * Algorithm Overview:
 *   Preprocessing:
 *     1. Partition nodes into clusters of approximate size k.
 *        A greedy BFS-based partitioning is used: pick an unassigned node
 *        as cluster leader, BFS out to add up to k nodes.
 *     2. Within each cluster, build a BFS spanning tree (Beta).
 *     3. Identify inter-cluster edges (edges between nodes in different clusters).
 *
 *   Per synchronous round:
 *     1. Local execution (same as Alpha/Beta)
 *     2. Intra-cluster synchronization (Beta within each cluster)
 *     3. Inter-cluster synchronization (Alpha between clusters)
 *        - Cluster leaders exchange safe signals across inter-cluster edges
 *     4. Broadcast next-pulse within clusters
 *
 *   Message complexity: O(|V|·k) per round
 *   Time complexity: O(cluster_depth) per round
 */

#include "synchro.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*============================================================================
 * Cluster Partitioning
 *
 * Greedy BFS-based clustering: repeatedly pick an unassigned node as a
 * cluster seed, then BFS outward adding nodes until the cluster reaches
 * size k or runs out of reachable unassigned nodes.
 *============================================================================*/

typedef struct {
    node_id_t leader;
    int       size;
    node_id_t *members;    /* Node IDs in this cluster */
    int       *assigned;   /* Temporary: which nodes are assigned */
} cluster_t;

static int partition_clusters(network_t *net, int k) {
    int n = net->num_nodes;
    if (n == 0) return -1;
    if (k <= 0) k = (int)(log((double)n) + 1.0);
    if (k < 1) k = 1;
    if (k > n) k = n;

    /* Reset cluster assignments */
    for (int i = 0; i < n; i++) {
        net->nodes[i].cluster_id = NODE_ID_NONE;
        net->nodes[i].is_cluster_leader = 0;
        net->nodes[i].cluster_leader = NODE_ID_NONE;
        net->nodes[i].cluster_size = 0;
    }

    int *assigned = (int *)calloc((size_t)n, sizeof(int));
    int *queue = (int *)malloc((size_t)n * sizeof(int));
    if (!assigned || !queue) {
        free(assigned);
        free(queue);
        return -1;
    }

    node_id_t cluster_id = 0;

    for (int i = 0; i < n; i++) {
        if (assigned[i]) continue;

        /* Start a new cluster with node i as leader */
        int head = 0, tail = 0;
        queue[tail++] = i;
        assigned[i] = 1;
        net->nodes[i].cluster_id = cluster_id;
        net->nodes[i].is_cluster_leader = 1;
        net->nodes[i].cluster_leader = (node_id_t)i;
        int cluster_size_count = 1;

        /* BFS to add more nodes until cluster reaches size k */
        while (head < tail && cluster_size_count < k) {
            int u = queue[head++];
            for (neighbor_entry_t *nb = net->nodes[u].neighbors;
                 nb && cluster_size_count < k; nb = nb->next) {
                int v = nb->neighbor_id;
                if (!assigned[v]) {
                    assigned[v] = 1;
                    net->nodes[v].cluster_id = cluster_id;
                    net->nodes[v].cluster_leader = (node_id_t)i;
                    net->nodes[v].is_cluster_leader = 0;
                    queue[tail++] = v;
                    cluster_size_count++;
                }
            }
        }

        /* Record the cluster size on all members */
        /* Iterate all nodes to mark cluster sizes */
        for (int v = 0; v < n; v++) {
            if (net->nodes[v].cluster_id == cluster_id) {
                net->nodes[v].cluster_size = cluster_size_count;
            }
        }

        cluster_id++;
    }

    /* Count inter-cluster edges for each node */
    for (int i = 0; i < n; i++) {
        node_t *node = &net->nodes[i];
        if (node->intercluster_nbrs) {
            free(node->intercluster_nbrs);
            node->intercluster_nbrs = NULL;
        }
        node->intercluster_count = 0;

        /* First pass: count */
        for (neighbor_entry_t *nb = node->neighbors; nb; nb = nb->next) {
            if (net->nodes[nb->neighbor_id].cluster_id != node->cluster_id) {
                node->intercluster_count++;
            }
        }

        /* Allocate and populate */
        if (node->intercluster_count > 0) {
            node->intercluster_nbrs = (node_id_t *)malloc(
                (size_t)node->intercluster_count * sizeof(node_id_t));
            int idx = 0;
            for (neighbor_entry_t *nb = node->neighbors; nb; nb = nb->next) {
                if (net->nodes[nb->neighbor_id].cluster_id != node->cluster_id) {
                    node->intercluster_nbrs[idx++] = nb->neighbor_id;
                }
            }
        }
    }

    free(assigned);
    free(queue);
    return (int)cluster_id; /* Return number of clusters */
}

/*============================================================================
 * Build intra-cluster BFS trees
 *
 * Within each cluster, build a BFS tree rooted at the cluster leader.
 *============================================================================*/

static int build_intra_cluster_trees(network_t *net) {
    int n = net->num_nodes;
    /* Reset BFS fields for all nodes */
    for (int i = 0; i < n; i++) {
        net->nodes[i].bfs_parent = NODE_ID_NONE;
        if (net->nodes[i].bfs_children) {
            free(net->nodes[i].bfs_children);
            net->nodes[i].bfs_children = NULL;
        }
        net->nodes[i].bfs_children_count = 0;
        net->nodes[i].depth = -1;
    }

    /* For each cluster leader, run intra-cluster BFS */
    int *queue = (int *)malloc((size_t)n * sizeof(int));
    int *visited = (int *)calloc((size_t)n, sizeof(int));
    if (!queue || !visited) {
        free(queue);
        free(visited);
        return -1;
    }

    for (int i = 0; i < n; i++) {
        if (!net->nodes[i].is_cluster_leader) continue;
        if (visited[i]) continue;

        node_id_t cluster = net->nodes[i].cluster_id;

        int head = 0, tail = 0;
        queue[tail++] = i;
        visited[i] = 1;
        net->nodes[i].depth = 0;

        while (head < tail) {
            int u = queue[head++];
            for (neighbor_entry_t *nb = net->nodes[u].neighbors;
                 nb; nb = nb->next) {
                int v = nb->neighbor_id;
                /* Only traverse within the same cluster */
                if (!visited[v] &&
                    net->nodes[v].cluster_id == cluster) {
                    visited[v] = 1;
                    net->nodes[v].bfs_parent = (node_id_t)u;
                    net->nodes[v].depth = net->nodes[u].depth + 1;
                    queue[tail++] = v;
                }
            }
        }
    }

    /* Count and allocate children arrays */
    for (int i = 0; i < n; i++) {
        net->nodes[i].bfs_children_count = 0;
    }
    for (int i = 0; i < n; i++) {
        node_id_t p = net->nodes[i].bfs_parent;
        if (p != NODE_ID_NONE) {
            net->nodes[p].bfs_children_count++;
        }
    }
    for (int i = 0; i < n; i++) {
        int cnt = net->nodes[i].bfs_children_count;
        if (cnt > 0) {
            net->nodes[i].bfs_children =
                (node_id_t *)malloc((size_t)cnt * sizeof(node_id_t));
            net->nodes[i].bfs_children_count = 0; /* Reset for fill */
        }
    }
    for (int i = 0; i < n; i++) {
        node_id_t p = net->nodes[i].bfs_parent;
        if (p != NODE_ID_NONE) {
            node_t *parent = &net->nodes[p];
            parent->bfs_children[parent->bfs_children_count] = (node_id_t)i;
            parent->bfs_children_count++;
        }
    }

    free(queue);
    free(visited);
    return 0;
}

/*============================================================================
 * Gamma Initialization
 *============================================================================*/

int synchro_gamma_init(network_t *net, int cluster_size) {
    if (!net) return -1;

    /* Step 1: Partition into clusters */
    int num_clusters = partition_clusters(net, cluster_size);
    if (num_clusters < 0) return -1;

    /* Step 2: Build intra-cluster BFS trees */
    if (build_intra_cluster_trees(net) != 0) return -1;

    /* Step 3: Initialize synchronizer state */
    for (int i = 0; i < net->num_nodes; i++) {
        net->nodes[i].state = NODE_UNSAFE;
        net->nodes[i].current_round = 0;
        net->nodes[i].safe_children = 0;
        net->nodes[i].safe_count = 0;
        net->nodes[i].ack_count = 0;
    }
    return 0;
}

/*============================================================================
 * Gamma: Execute One Synchronous Round
 *
 * Three phases:
 *   1. Local protocol execution
 *   2. Intra-cluster Beta convergecast (bottom-up within each cluster)
 *   3. Inter-cluster Alpha synchronization (between cluster leaders)
 *   4. Broadcast next-pulse (top-down within each cluster)
 *============================================================================*/

int gamma_execute_round(network_t *net, sync_protocol_fn protocol) {
    if (!net || net->synchro_type != SYNCHRO_GAMMA) return -1;

    int n = net->num_nodes;
    if (n == 0) return -1;

    /*------------------------------------------------------
     * Phase 1: Local — execute protocol at each node
     *------------------------------------------------------*/
    for (int i = 0; i < n; i++) {
        node_t *node = &net->nodes[i];
        if (node->state == NODE_CRASHED) continue;
        protocol(net, node, node->current_round);
        node->state = NODE_SAFE;
    }

    /*------------------------------------------------------
     * Phase 2: Intra-cluster convergecast (Beta-like, bottom-up)
     *------------------------------------------------------*/
    int max_depth = 0;
    for (int i = 0; i < n; i++) {
        if (net->nodes[i].depth > max_depth)
            max_depth = net->nodes[i].depth;
    }

    for (int d = max_depth; d >= 0; d--) {
        for (int i = 0; i < n; i++) {
            node_t *node = &net->nodes[i];
            if (node->depth != d || node->state == NODE_CRASHED) continue;

            /* Count MSG_SAFE_SUBTREE from intra-cluster children */
            node->safe_children = 0;
            msg_queue_entry_t *entry = node->incoming.head;
            while (entry) {
                if (entry->msg.type == MSG_SAFE_SUBTREE &&
                    entry->msg.round == node->current_round) {
                    for (int c = 0; c < node->bfs_children_count; c++) {
                        if (node->bfs_children[c] == entry->msg.sender) {
                            node->safe_children++;
                            break;
                        }
                    }
                }
                entry = entry->next;
            }

            if (node->bfs_parent == NODE_ID_NONE) {
                /* Cluster root (leader): ready for inter-cluster phase */
                if (node->safe_children >= node->bfs_children_count &&
                    node->state == NODE_SAFE) {
                    node->state = NODE_WAITING;
                }
            } else {
                /* Non-root: send MSG_SAFE_SUBTREE to intra-cluster parent */
                if (node->safe_children >= node->bfs_children_count &&
                    node->state == NODE_SAFE) {
                    node_send_message(net, node->id, node->bfs_parent,
                                      MSG_SAFE_SUBTREE,
                                      node->current_round, NULL, 0);
                    node->state = NODE_WAITING;
                }
            }
        }
    }

    /*------------------------------------------------------
     * Phase 3: Inter-cluster Alpha synchronization
     *
     * Cluster leaders exchange MSG_SAFE and MSG_ACK with leaders
     * of neighboring clusters via inter-cluster edges.
     *------------------------------------------------------*/

    /* 3a: Cluster leaders send MSG_SAFE to inter-cluster neighbors */
    for (int i = 0; i < n; i++) {
        node_t *node = &net->nodes[i];
        if (!node->is_cluster_leader || node->state != NODE_WAITING) continue;

        for (int j = 0; j < node->intercluster_count; j++) {
            node_id_t nb = node->intercluster_nbrs[j];
            /* Send MSG_INTERCLUSTER (acts as MSG_SAFE across clusters) */
            node_send_message(net, node->id, nb,
                              MSG_INTERCLUSTER, node->current_round,
                              NULL, 0);
        }
    }

    /* 3b: Cluster leaders process inter-cluster messages */
    for (int i = 0; i < n; i++) {
        node_t *node = &net->nodes[i];
        if (!node->is_cluster_leader || node->state != NODE_WAITING) continue;

        /* Count inter-cluster ACKs from neighbors */
        int inter_safe = 0;
        msg_queue_entry_t *entry = node->incoming.head;
        while (entry) {
            if (entry->msg.type == MSG_INTERCLUSTER &&
                entry->msg.round == node->current_round) {
                /* Check if sender is an inter-cluster neighbor */
                for (int j = 0; j < node->intercluster_count; j++) {
                    if (node->intercluster_nbrs[j] == entry->msg.sender) {
                        inter_safe++;
                        break;
                    }
                }
            }
            entry = entry->next;
        }

        if (inter_safe >= node->intercluster_count) {
            /* All inter-cluster neighbors are safe → cluster leader is READY */
            node->state = NODE_READY;
        }
    }

    /*------------------------------------------------------
     * Phase 4: Intra-cluster broadcast (top-down)
     *------------------------------------------------------*/
    for (int d = 0; d <= max_depth; d++) {
        for (int i = 0; i < n; i++) {
            node_t *node = &net->nodes[i];
            if (node->depth != d || node->state == NODE_CRASHED) continue;

            if (node->is_cluster_leader) {
                if (node->state == NODE_READY) {
                    /* Broadcast MSG_NEXT_PULSE to intra-cluster children */
                    for (int c = 0; c < node->bfs_children_count; c++) {
                        node_send_message(net, node->id,
                                          node->bfs_children[c],
                                          MSG_NEXT_PULSE,
                                          node->current_round, NULL, 0);
                    }
                }
            } else {
                /* Non-leader: check for MSG_NEXT_PULSE from parent */
                int got_pulse = 0;
                msg_queue_entry_t *entry = node->incoming.head;
                while (entry) {
                    if (entry->msg.type == MSG_NEXT_PULSE &&
                        entry->msg.sender == node->bfs_parent &&
                        entry->msg.round == node->current_round) {
                        got_pulse = 1;
                        break;
                    }
                    entry = entry->next;
                }

                if (got_pulse && node->state == NODE_WAITING) {
                    node->state = NODE_READY;
                    for (int c = 0; c < node->bfs_children_count; c++) {
                        node_send_message(net, node->id,
                                          node->bfs_children[c],
                                          MSG_NEXT_PULSE,
                                          node->current_round, NULL, 0);
                    }
                }
            }
        }
    }

    /*------------------------------------------------------
     * Cleanup: Advance all READY nodes
     *------------------------------------------------------*/
    int proceeded = 0;
    for (int i = 0; i < n; i++) {
        node_t *node = &net->nodes[i];
        if (node->state == NODE_READY || node->state == NODE_SAFE ||
            node->state == NODE_WAITING) {
            /* Clean up old round messages */
            node_consume_messages(node, MSG_SAFE_SUBTREE, node->current_round);
            node_consume_messages(node, MSG_NEXT_PULSE, node->current_round);
            node_consume_messages(node, MSG_INTERCLUSTER, node->current_round);
            node_consume_messages(node, MSG_SAFE, node->current_round);
            node_consume_messages(node, MSG_ACK, node->current_round);
            node->current_round++;
            node->state = NODE_UNSAFE;
            node->safe_children = 0;
            node->safe_count = 0;
            node->ack_count = 0;
            proceeded++;
        }
    }

    net->total_rounds_executed++;
    net->total_time_units += (double)(max_depth + 1);

    return proceeded;
}
