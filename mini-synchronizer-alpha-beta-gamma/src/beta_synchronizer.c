/**
 * beta_synchronizer.c — Beta (β) Synchronizer Implementation
 *
 * Reference: Baruch Awerbuch, "Complexity of Network Synchronization"
 *            Journal of the ACM, Vol. 32, No. 4, pp. 804-823, 1985.
 *
 * The Beta synchronizer uses a BFS spanning tree for convergecast/broadcast
 * rather than all-to-all edge communication. This reduces message complexity
 * at the cost of increased time complexity.
 *
 * Knowledge coverage:
 *   L1: Beta synchronizer definition, BFS tree, convergecast, broadcast
 *   L2: Tree-based synchronization vs edge-based tradeoff
 *   L3: Spanning tree, parent/child relationships, tree depth
 *   L4: Theorem: Beta uses 2(|V|-1) = O(|V|) messages and O(depth) time/round
 *   L5: Algorithm — BFS construction, convergecast, broadcast
 *
 * Algorithm:
 *   Preprocessing: Build a BFS spanning tree rooted at node 0.
 *     Each node learns its parent, children, and depth.
 *
 *   Per synchronous round:
 *     Phase 1 (Local): Each node executes the synchronous protocol,
 *       then enters SAFE state.
 *     Phase 2 (Convergecast — bottom-up):
 *       - Leaf nodes (no children): when locally SAFE, send MSG_SAFE_SUBTREE
 *         to parent.
 *       - Internal nodes: wait until (a) locally SAFE AND
 *         (b) MSG_SAFE_SUBTREE received from ALL children,
 *         then send MSG_SAFE_SUBTREE to parent.
 *       - Root: wait until (a) locally SAFE AND
 *         (b) MSG_SAFE_SUBTREE from all children.
 *     Phase 3 (Broadcast — top-down):
 *       - Root: when conditions met, send MSG_NEXT_PULSE to all children.
 *       - Internal nodes: when MSG_NEXT_PULSE received from parent,
 *         forward to all children and increment round.
 *       - Leaf nodes: when MSG_NEXT_PULSE received from parent,
 *         increment round.
 *
 * Message complexity per round: 2(|V|-1) = O(|V|)
 *   (|V|-1 for convergecast + |V|-1 for broadcast)
 * Time complexity per round: O(depth) = O(|V|)
 */

#include "synchro.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*============================================================================
 * BFS Tree Construction (for Beta Synchronizer)
 *
 * Builds a BFS spanning tree rooted at node 0 using a queue-based approach
 * on the network graph. This runs in the "preprocessing" phase before
 * any synchronous rounds are simulated.
 *
 * After construction, each node knows:
 *   - bfs_parent: its parent in the tree (NODE_ID_NONE for root)
 *   - bfs_children[]: array of its children
 *   - bfs_children_count: number of children
 *   - depth: distance from root
 *
 * Complexity: O(|V| + |E|) time
 *============================================================================*/

static int build_bfs_tree(network_t *net) {
    int n = net->num_nodes;
    if (n == 0) return -1;

    /* Reset all BFS fields */
    for (int i = 0; i < n; i++) {
        net->nodes[i].bfs_parent = NODE_ID_NONE;
        net->nodes[i].bfs_children_count = 0;
        if (net->nodes[i].bfs_children) {
            free(net->nodes[i].bfs_children);
            net->nodes[i].bfs_children = NULL;
        }
        net->nodes[i].depth = -1;
    }

    /* BFS queue (simple array-based) */
    int *queue = (int *)malloc((size_t)n * sizeof(int));
    int *visited = (int *)calloc((size_t)n, sizeof(int));
    if (!queue || !visited) {
        free(queue);
        free(visited);
        return -1;
    }

    int head = 0, tail = 0;
    node_id_t root = 0;

    /* Root initialization */
    queue[tail++] = root;
    visited[root] = 1;
    net->nodes[root].depth = 0;
    net->nodes[root].bfs_parent = NODE_ID_NONE;

    /* BFS traversal */
    while (head < tail) {
        int u = queue[head++];
        node_t *node_u = &net->nodes[u];

        for (neighbor_entry_t *nb = node_u->neighbors; nb; nb = nb->next) {
            int v = nb->neighbor_id;
            if (!visited[v]) {
                visited[v] = 1;
                net->nodes[v].bfs_parent = (node_id_t)u;
                net->nodes[v].depth = node_u->depth + 1;
                queue[tail++] = v;
            }
        }
    }

    /* Count children for each node (second pass) */
    for (int i = 0; i < n; i++) {
        if (net->nodes[i].bfs_parent != NODE_ID_NONE) {
            int p = net->nodes[i].bfs_parent;
            net->nodes[p].bfs_children_count++;
        }
    }

    /* Allocate children arrays and populate (third pass) */
    for (int i = 0; i < n; i++) {
        int count = net->nodes[i].bfs_children_count;
        if (count > 0) {
            net->nodes[i].bfs_children =
                (node_id_t *)malloc((size_t)count * sizeof(node_id_t));
            net->nodes[i].bfs_children_count = 0; /* Reset for population */
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
 * Beta Initialization
 *============================================================================*/

int synchro_beta_init(network_t *net) {
    if (!net) return -1;

    /* Build the BFS spanning tree */
    if (build_bfs_tree(net) != 0) return -1;

    /* Initialize synchronizer state */
    for (int i = 0; i < net->num_nodes; i++) {
        net->nodes[i].state = NODE_UNSAFE;
        net->nodes[i].current_round = 0;
        net->nodes[i].safe_children = 0;
    }
    return 0;
}

/*============================================================================
 * Beta: Execute One Synchronous Round
 *
 * The Beta synchronizer processes nodes based on their position in the tree.
 * We process bottom-up (convergecast) then top-down (broadcast).
 *============================================================================*/

int beta_execute_round(network_t *net, sync_protocol_fn protocol) {
    if (!net || net->synchro_type != SYNCHRO_BETA) return -1;

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
     * Phase 2: Convergecast (bottom-up)
     *
     * Process nodes in decreasing order of depth so that children
     * send their safe signals before parents process them.
     *------------------------------------------------------*/

    /* Find maximum depth */
    int max_depth = 0;
    for (int i = 0; i < n; i++) {
        if (net->nodes[i].depth > max_depth)
            max_depth = net->nodes[i].depth;
    }

    /* Process level by level from leaves to root */
    for (int d = max_depth; d >= 0; d--) {
        for (int i = 0; i < n; i++) {
            node_t *node = &net->nodes[i];
            if (node->depth != d || node->state == NODE_CRASHED) continue;

            /* Reset safe children count at the start of convergecast */
            node->safe_children = 0;

            /* Count MSG_SAFE_SUBTREE from children */
            msg_queue_entry_t *entry = node->incoming.head;
            while (entry) {
                if (entry->msg.type == MSG_SAFE_SUBTREE &&
                    entry->msg.round == node->current_round) {
                    /* Verify sender is a child */
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
                /* This is the root: if all children safe AND self safe,
                 * proceed to broadcast */
                if (node->safe_children >= node->bfs_children_count &&
                    node->state == NODE_SAFE) {
                    /* Root is now ready for broadcast (Phase 3) */
                    node->state = NODE_WAITING; /* Signal: root is ready */
                }
            } else {
                /* Non-root node: if all children safe AND self safe,
                 * send MSG_SAFE_SUBTREE to parent */
                if (node->safe_children >= node->bfs_children_count &&
                    node->state == NODE_SAFE) {
                    node_send_message(net, node->id, node->bfs_parent,
                                      MSG_SAFE_SUBTREE, node->current_round,
                                      NULL, 0);
                    node->state = NODE_WAITING;
                }
            }
        }

        /* Process incoming messages between depth levels */
        for (int i = 0; i < n; i++) {
            node_process_incoming(net, &net->nodes[i]);
        }
    }

    /*------------------------------------------------------
     * Phase 3: Broadcast (top-down)
     *
     * Process from root to leaves. The root initiates the broadcast.
     *------------------------------------------------------*/
    for (int d = 0; d <= max_depth; d++) {
        for (int i = 0; i < n; i++) {
            node_t *node = &net->nodes[i];
            if (node->depth != d || node->state == NODE_CRASHED) continue;

            if (node->bfs_parent == NODE_ID_NONE) {
                /* Root: broadcast MSG_NEXT_PULSE to all children */
                if (node->state == NODE_WAITING) {
                    node->state = NODE_READY;
                    for (int c = 0; c < node->bfs_children_count; c++) {
                        node_send_message(net, node->id,
                                          node->bfs_children[c],
                                          MSG_NEXT_PULSE,
                                          node->current_round, NULL, 0);
                    }
                }
            } else {
                /* Non-root: check if MSG_NEXT_PULSE received from parent */
                msg_queue_entry_t *entry = node->incoming.head;
                int got_pulse = 0;
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
                    /* Forward broadcast to children */
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

        /* Process messages between depth levels */
        for (int i = 0; i < n; i++) {
            node_process_incoming(net, &net->nodes[i]);
        }
    }

    /*------------------------------------------------------
     * Cleanup: Advance all READY nodes to next round
     *------------------------------------------------------*/
    int proceeded = 0;
    for (int i = 0; i < n; i++) {
        node_t *node = &net->nodes[i];
        if (node->state == NODE_READY) {
            /* Clean up old round messages */
            node_consume_messages(node, MSG_SAFE_SUBTREE, node->current_round);
            node_consume_messages(node, MSG_NEXT_PULSE, node->current_round);
            node->current_round++;
            node->state = NODE_UNSAFE;
            node->safe_children = 0;
            proceeded++;
        }
    }

    net->total_rounds_executed++;
    /* Time: O(depth) per round */
    net->total_time_units += (double)(max_depth + 1);

    return proceeded;
}
