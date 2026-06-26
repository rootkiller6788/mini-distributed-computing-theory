/**
 * distributed_algorithms.c — Synchronous Distributed Algorithms
 *
 * Implements canonical distributed algorithms designed to run on top
 * of the α, β, γ synchronizers. Each algorithm is expressed as a
 * synchronous round protocol that the synchronizer calls once per node
 * per simulated round.
 *
 * Knowledge coverage:
 *   L5: Algorithms — BFS, Leader Election (LeLann-Chang-Roberts),
 *       Broadcast, Convergecast, GHS Minimum Spanning Tree
 *   L6: Canonical Problems — spanning tree, leader election,
 *       distributed consensus, sensor aggregation
 *   L7: Applications — wireless sensor network aggregation,
 *       flood-set consensus for distributed databases
 *   L8: Advanced — self-stabilizing synchronizer
 */

#include "synchro.h"
#include "distributed_algorithms.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*============================================================================
 * BFS Protocol
 *
 * Each node has local_data used as follows:
 *   local_data == 0 → undiscovered
 *   local_data == 1 → discovered (and bfs_parent set)
 *
 * Round semantics:
 *   Round 0: Root marks itself discovered (local_data = 1)
 *   Round r > 0: Nodes discovered in round r-1 send MSG_BFS_SEARCH to
 *     all neighbors. Unvisited neighbors mark themselves discovered,
 *     set the sender as bfs_parent, and record their depth.
 *============================================================================*/

/**
 * State tracking for BFS: we use the node's local_data as a discovery flag
 * and bfs_parent for the tree structure. depth stores the BFS depth.
 */

int bfs_protocol_round(network_t *net, node_t *node, round_t round) {
    if (!net || !node) return -1;

    if (round == 0) {
        /* Round 0: root marks itself */
        if (node->id == 0) {
            node->local_data = 1;          /* discovered */
            node->bfs_parent = NODE_ID_NONE;
            node->depth = 0;
        }
        return 0;
    }

    /* Rounds 1+: nodes discovered in previous round broadcast */
    if (node->local_data == 1 && node->depth == round - 1) {
        /* This node was discovered in the previous round — explore */
        for (neighbor_entry_t *nb = node->neighbors; nb; nb = nb->next) {
            node_send_message(net, node->id, nb->neighbor_id,
                              MSG_BFS_SEARCH, round, NULL, 0);
        }
    }

    /* Process incoming BFS_SEARCH messages */
    if (node->local_data == 0) {
        /* Undiscovered: check for MSG_BFS_SEARCH from any neighbor */
        msg_queue_entry_t *entry = node->incoming.head;
        while (entry) {
            if (entry->msg.type == MSG_BFS_SEARCH &&
                entry->msg.round == round) {
                node->local_data = 1;
                node->bfs_parent = entry->msg.sender;
                node->depth = (int)round;
                break; /* First discoverer becomes parent (BFS property) */
            }
            entry = entry->next;
        }
    }

    return 0;
}

/**
 * Compute bfs_children arrays from bfs_parent relationships.
 * Must be called after any algorithm that sets bfs_parent.
 * This is needed for convergecast/broadcast protocols that use
 * the children lists to propagate messages down/up the tree.
 *
 * Complexity: O(|V|).
 */
void bfs_build_children_arrays(network_t *net) {
    if (!net) return;
    int n = net->num_nodes;

    /* Free existing children arrays */
    for (int i = 0; i < n; i++) {
        if (net->nodes[i].bfs_children) {
            free(net->nodes[i].bfs_children);
            net->nodes[i].bfs_children = NULL;
        }
        net->nodes[i].bfs_children_count = 0;
    }

    /* Count children for each node */
    for (int i = 0; i < n; i++) {
        node_id_t p = net->nodes[i].bfs_parent;
        if (p != NODE_ID_NONE && p >= 0 && p < n) {
            net->nodes[p].bfs_children_count++;
        }
    }

    /* Allocate children arrays */
    for (int i = 0; i < n; i++) {
        int cnt = net->nodes[i].bfs_children_count;
        if (cnt > 0) {
            net->nodes[i].bfs_children =
                (node_id_t *)malloc((size_t)cnt * sizeof(node_id_t));
            net->nodes[i].bfs_children_count = 0; /* Reset for fill pass */
        }
    }

    /* Fill children arrays */
    for (int i = 0; i < n; i++) {
        node_id_t p = net->nodes[i].bfs_parent;
        if (p != NODE_ID_NONE && p >= 0 && p < n) {
            node_t *parent = &net->nodes[p];
            parent->bfs_children[parent->bfs_children_count] = (node_id_t)i;
            parent->bfs_children_count++;
        }
    }
}

void bfs_reset(network_t *net) {
    if (!net) return;
    for (int i = 0; i < net->num_nodes; i++) {
        net->nodes[i].local_data = 0;
        /* Note: Do NOT clear bfs_parent or depth here — these are
         * managed by the synchronizer (Beta init sets up the tree).
         * The BFS protocol will write to them during execution (for
         * Alpha/Beta/Gamma), but clearing them would destroy the
         * synchronizer's internal tree structure. */
    }
}

int bfs_run(network_t *net, node_id_t root) {
    if (!net) return -1;
    bfs_reset(net);
    /* Pre-mark root node */
    if (root >= 0 && root < net->num_nodes) {
        net->nodes[root].local_data = 1;
        net->nodes[root].bfs_parent = NODE_ID_NONE;
        net->nodes[root].depth = 0;
    }
    /* Reset all other nodes */
    for (int i = 0; i < net->num_nodes; i++) {
        if (i != root) {
            net->nodes[i].local_data = 0;
            /* Don't clear bfs_parent/depth if synchronizer already set them */
        }
    }

    int max_rounds = net->num_nodes + 1;
    synchro_reset_counters(net);
    int rounds = synchro_run_protocol(net, bfs_protocol_round, max_rounds);
    return rounds;
}

void bfs_print_tree(const network_t *net) {
    if (!net) return;
    printf("BFS Tree (root = node 0):\n");
    printf("  Node  Parent  Depth\n");
    printf("  ----  ------  -----\n");
    for (int i = 0; i < net->num_nodes; i++) {
        printf("  %4d  %6d  %5d\n", i, net->nodes[i].bfs_parent,
               net->nodes[i].depth);
    }
}

int bfs_tree_depth(const network_t *net) {
    if (!net) return -1;
    int max_d = 0;
    for (int i = 0; i < net->num_nodes; i++) {
        if (net->nodes[i].depth > max_d) max_d = net->nodes[i].depth;
    }
    return max_d;
}

int bfs_verify_tree(const network_t *net, node_id_t root) {
    if (!net) return 0;
    /* Check: every node except root has a parent */
    for (int i = 0; i < net->num_nodes; i++) {
        if (i == root) {
            if (net->nodes[i].bfs_parent != NODE_ID_NONE) return 0;
        } else {
            if (net->nodes[i].local_data == 1 &&
                net->nodes[i].bfs_parent == NODE_ID_NONE) return 0;
        }
    }
    /* Check: no cycles (parent depth < child depth) */
    for (int i = 0; i < net->num_nodes; i++) {
        if (net->nodes[i].bfs_parent != NODE_ID_NONE) {
            int p = net->nodes[i].bfs_parent;
            if (net->nodes[p].depth >= net->nodes[i].depth) return 0;
        }
    }
    return 1;
}

int network_reachable_count(const network_t *net, node_id_t root) {
    (void)root; /* root parameter reserved for directed-graph reachability */
    if (!net) return -1;
    int count = 0;
    for (int i = 0; i < net->num_nodes; i++) {
        /* A node is reachable if it has been assigned a valid depth
         * (either by the synchronizer's tree or the BFS algorithm) */
        if (net->nodes[i].depth >= 0) count++;
    }
    return count;
}

/*============================================================================
 * Leader Election — LeLann-Chang-Roberts (Ring)
 *
 * Each node starts with local_data = its own ID.
 * In each round, nodes forward the largest ID they've seen.
 * When a node receives its own ID back, it becomes the leader.
 *
 * Assumes the network is a ring (each node has degree 2).
 *============================================================================*/

int leader_election_le_lann_round(network_t *net, node_t *node, round_t round) {
    if (!net || !node) return -1;

    if (round == 0) {
        /* Initialize: each node knows its own ID, sends to clockwise neighbor */
        node->local_data = (int)node->id;
        /* Send to first neighbor (direction 0 in the ring) */
        if (node->neighbors) {
            node_send_message(net, node->id, node->neighbors->neighbor_id,
                              MSG_APPLICATION, round,
                              &node->local_data, sizeof(int));
        }
        return 0;
    }

    /* Check for MSG_APPLICATION from neighbors containing candidate IDs */
    int max_seen = node->local_data;
    msg_queue_entry_t *entry = node->incoming.head;
    while (entry) {
        if (entry->msg.type == MSG_APPLICATION &&
            entry->msg.round == round &&
            entry->msg.payload) {
            int candidate = *(int *)entry->msg.payload;
            if (candidate > max_seen) max_seen = candidate;
        }
        entry = entry->next;
    }

    /* If we received our own ID back, we are the leader */
    if (max_seen == node->id && node->local_data == node->id) {
        node->local_data = node->id; /* Leader detected */
        return -1; /* Signal completion for this node */
    }

    node->local_data = max_seen;
    /* Forward to clockwise neighbor */
    if (node->neighbors) {
        node_send_message(net, node->id, node->neighbors->neighbor_id,
                          MSG_APPLICATION, round + 1,
                          &node->local_data, sizeof(int));
    }

    return 0;
}

void leader_election_reset(network_t *net) {
    if (!net) return;
    for (int i = 0; i < net->num_nodes; i++) {
        net->nodes[i].local_data = (int)(net->nodes[i].id);
    }
}

int leader_election_run(network_t *net) {
    if (!net) return -1;
    leader_election_reset(net);
    synchro_reset_counters(net);
    int max_rounds = net->num_nodes * 2; /* Worst case: n rounds */
    int rounds = synchro_run_protocol(net, leader_election_le_lann_round,
                                       max_rounds);
    return rounds;
}

void leader_election_print_result(const network_t *net) {
    if (!net) return;
    printf("Leader Election Results:\n");
    int leader = -1;
    for (int i = 0; i < net->num_nodes; i++) {
        printf("  Node %d: value=%d", i, net->nodes[i].local_data);
        if (net->nodes[i].local_data == i) {
            printf(" ← LEADER");
            leader = i;
        }
        printf("\n");
    }
    if (leader >= 0) {
        printf("  Consensus leader: Node %d\n", leader);
    } else {
        printf("  No leader elected.\n");
    }
}

/*============================================================================
 * Broadcast Protocol
 *
 * Runs on a pre-computed BFS tree. The root (node 0) initiates the
 * broadcast. Each node forwards the broadcast value to all its children.
 *
 * local_data carries the broadcast value.
 *============================================================================*/

int broadcast_protocol_round(network_t *net, node_t *node, round_t round) {
    if (!net || !node) return -1;

    if (round == 0 && node->id == 0) {
        /* Root initiates broadcast */
        node->local_data = 42; /* The broadcast value */
        for (int c = 0; c < node->bfs_children_count; c++) {
            node_send_message(net, node->id, node->bfs_children[c],
                              MSG_APPLICATION, round,
                              &node->local_data, sizeof(int));
        }
        return 0;
    }

    /* Non-root: check for MSG_APPLICATION from parent */
    if (node->local_data == 0) {
        msg_queue_entry_t *entry = node->incoming.head;
        while (entry) {
            if (entry->msg.type == MSG_APPLICATION &&
                entry->msg.sender == node->bfs_parent &&
                entry->msg.payload) {
                node->local_data = *(int *)entry->msg.payload;
                /* Forward to children */
                for (int c = 0; c < node->bfs_children_count; c++) {
                    node_send_message(net, node->id,
                                      node->bfs_children[c],
                                      MSG_APPLICATION, round,
                                      &node->local_data, sizeof(int));
                }
                return 0;
            }
            entry = entry->next;
        }
    }

    /* Check if broadcast is complete */
    return 0;
}

/*============================================================================
 * Convergecast (Sum) Protocol
 *
 * Each node starts with contribution 1. Leaves send to parent. Internal
 * nodes wait for ALL children's contributions, sum them with their own,
 * then send upstream exactly once.
 *
 * Uses local_sensor as a flag: value > 0 means "not yet sent",
 * value == -1.0 means "already sent to parent". This prevents
 * duplicate sends that would cause unbounded accumulation.
 *
 * Round semantics:
 *   Round 0: Each node sets local_data=1, local_sensor=1.0 (not sent).
 *            Leaves send to parent with round=1, set local_sensor=-1.0.
 *   Round r>0: Internal nodes drain child messages (any round),
 *            accumulate, and when all children have reported,
 *            send to parent and set local_sensor=-1.0.
 *============================================================================*/

int convergecast_sum_round(network_t *net, node_t *node, round_t round) {
    if (!net || !node) return -1;

    /* Round 0: initialize */
    if (round == 0) {
        node->local_data = 1;      /* Own contribution */
        node->local_sensor = 1.0;  /* Not yet sent flag */
    }

    /* If already sent to parent, don't send again */
    if (node->local_sensor < 0.0) return 0;

    /* Process ALL pending MSG_APPLICATION from children.
     * Consume them to avoid double-counting on future rounds. */
    int sum_from_children = 0;
    int distinct_children_heard = 0;

    /* Track which children have reported (by index into bfs_children) */
    int *child_heard = (int *)calloc(
        (size_t)(node->bfs_children_count > 0 ? node->bfs_children_count : 1),
        sizeof(int));

    msg_queue_entry_t *prev = NULL;
    msg_queue_entry_t *entry = node->incoming.head;
    while (entry) {
        int consumed = 0;
        if (entry->msg.type == MSG_APPLICATION && entry->msg.payload) {
            /* Check if sender is a child and we haven't heard from them */
            for (int c = 0; c < node->bfs_children_count; c++) {
                if (node->bfs_children[c] == entry->msg.sender &&
                    !child_heard[c]) {
                    sum_from_children += *(int *)entry->msg.payload;
                    child_heard[c] = 1;
                    distinct_children_heard++;
                    /* Remove consumed message from queue */
                    if (prev) prev->next = entry->next;
                    else node->incoming.head = entry->next;
                    msg_queue_entry_t *to_free = entry;
                    entry = entry->next;
                    if (to_free == node->incoming.tail)
                        node->incoming.tail = prev;
                    free(to_free);
                    node->incoming.count--;
                    consumed = 1;
                    break;
                }
            }
        }
        if (!consumed) { prev = entry; entry = entry->next; }
    }
    free(child_heard);

    /* Accumulate children's values */
    if (sum_from_children > 0) {
        node->local_data += sum_from_children;
    }

    /* If all children have reported (or no children = leaf), send to parent */
    if (distinct_children_heard >= node->bfs_children_count &&
        node->local_sensor > 0.0) {
        if (node->bfs_parent != NODE_ID_NONE) {
            int val = node->local_data;
            node_send_message(net, node->id, node->bfs_parent,
                              MSG_APPLICATION, round + 1,
                              &val, sizeof(int));
            node->local_sensor = -1.0; /* Mark as sent */
        }
        /* Root: no parent, just accumulate. local_data holds total. */
    }

    return 0;
}

/*============================================================================
 * GHS Minimum Spanning Tree (Simplified Synchronous Version)
 *
 * GHS (Gallager-Humblet-Spira, 1983) constructs an MST in O(n log n)
 * messages. This is a simplified synchronous version.
 *
 * local_data: fragment ID (initially node's own ID)
 * MST edges are tracked by marking the edge as selected.
 *
 * In each round:
 *   1. Each fragment finds its minimum-weight outgoing edge (MWOE)
 *   2. Fragments merge via their MWOEs
 *   3. Fragment IDs are updated to the minimum ID in the merged fragment
 *
 * The algorithm terminates when only one fragment remains.
 *============================================================================*/

/**
 * Per-fragment state stored in local_data: the fragment ID.
 * We also use depth to store the "level" of the fragment (GHS level).
 * local_sensor stores the minimum outgoing edge weight found.
 */

void mst_ghs_init(network_t *net) {
    if (!net) return;
    for (int i = 0; i < net->num_nodes; i++) {
        net->nodes[i].local_data = (int)(net->nodes[i].id); /* Fragment ID = own ID */
        net->nodes[i].depth = 0;                              /* Level 0 */
        net->nodes[i].local_sensor = WEIGHT_INF;              /* No MWOE yet */
    }
}

int mst_ghs_protocol_round(network_t *net, node_t *node, round_t round) {
    if (!net || !node) return -1;

    int my_fragment = node->local_data;

    /* Step 1: Find minimum-weight edge to a neighbor in a different fragment */
    weight_t min_weight = WEIGHT_INF;
    node_id_t min_neighbor = NODE_ID_NONE;

    for (neighbor_entry_t *nb = node->neighbors; nb; nb = nb->next) {
        node_t *neighbor_node = &net->nodes[nb->neighbor_id];
        if (neighbor_node->local_data != my_fragment) {
            if (nb->edge_weight < min_weight) {
                min_weight = nb->edge_weight;
                min_neighbor = nb->neighbor_id;
            }
        }
    }

    node->local_sensor = min_weight;

    /* Step 2: Exchange MWOE proposals with neighbors */
    if (min_neighbor != NODE_ID_NONE) {
        double payload_data[2] = {(double)my_fragment, min_weight};
        node_send_message(net, node->id, min_neighbor,
                          MSG_APPLICATION, round,
                          payload_data, sizeof(payload_data));
    }

    /* Step 3: Process proposals — if two nodes propose each other,
     * they merge fragments */
    msg_queue_entry_t *entry = node->incoming.head;
    while (entry) {
        if (entry->msg.type == MSG_APPLICATION &&
            entry->msg.payload &&
            entry->msg.payload_size >= (uint32_t)(2 * sizeof(double))) {
            double *data = (double *)entry->msg.payload;
            int other_fragment = (int)data[0];
            double other_weight = data[1];

            /* If the other node proposed us as its MWOE, merge */
            if (other_fragment != my_fragment &&
                other_weight <= node->local_sensor) {
                /* Merge: adopt the smaller fragment ID */
                int new_fragment = (my_fragment < other_fragment)
                                   ? my_fragment : other_fragment;
                node->local_data = new_fragment;
            }
        }
        entry = entry->next;
    }

    return 0;
}

void mst_ghs_print(const network_t *net) {
    if (!net) return;
    printf("GHS MST Fragment IDs:\n");
    for (int i = 0; i < net->num_nodes; i++) {
        printf("  Node %d: fragment=%d\n", i, net->nodes[i].local_data);
    }
}

/*============================================================================
 * Sensor Network Aggregation — Maximum Sensor Reading
 *
 * Each node has local_sensor as a random sensor reading (e.g., temperature).
 * Using a convergecast on the BFS tree, compute the maximum reading.
 *============================================================================*/

void sensor_init_random(network_t *net, unsigned int seed) {
    if (!net) return;
    srand(seed);
    for (int i = 0; i < net->num_nodes; i++) {
        net->nodes[i].local_sensor = (double)(rand() % 1000) / 10.0;
        /* Store reading in local_data as 2-decimal fixed point for transmission */
        net->nodes[i].local_data = (int)(net->nodes[i].local_sensor * 100.0);
    }
}

int sensor_aggregate_max_round(network_t *net, node_t *node, round_t round) {
    if (!net || !node) return -1;

    /* Leaves report immediately */
    if (node->bfs_children_count == 0) {
        if (node->bfs_parent != NODE_ID_NONE) {
            int val = node->local_data;
            node_send_message(net, node->id, node->bfs_parent,
                              MSG_APPLICATION, round,
                              &val, sizeof(int));
        }
        return 0;
    }

    /* Internal node: find max from children */
    int max_from_children = node->local_data;
    int children_heard = 0;
    msg_queue_entry_t *entry = node->incoming.head;
    while (entry) {
        if (entry->msg.type == MSG_APPLICATION && entry->msg.payload) {
            int is_child = 0;
            for (int c = 0; c < node->bfs_children_count; c++) {
                if (node->bfs_children[c] == entry->msg.sender) {
                    is_child = 1;
                    break;
                }
            }
            if (is_child) {
                int child_val = *(int *)entry->msg.payload;
                if (child_val > max_from_children) max_from_children = child_val;
                children_heard++;
            }
        }
        entry = entry->next;
    }

    node->local_data = max_from_children;

    if (children_heard >= node->bfs_children_count && node->bfs_parent != NODE_ID_NONE) {
        node_send_message(net, node->id, node->bfs_parent,
                          MSG_APPLICATION, round,
                          &node->local_data, sizeof(int));
    }

    return 0;
}

/*============================================================================
 * Flood-Set Consensus Protocol
 *
 * Each node starts with a proposed value (0 or 1) stored in local_data.
 * In each round, nodes exchange their current set of seen values.
 * After diameter+1 rounds, all non-faulty nodes have the same set.
 * The consensus decision is the minimum value in the set.
 *
 * This implements crash-fault-tolerant consensus in the synchronous model.
 *============================================================================*/

void consensus_init_random(network_t *net, unsigned int seed) {
    if (!net) return;
    srand(seed);
    for (int i = 0; i < net->num_nodes; i++) {
        net->nodes[i].local_data = rand() % 2; /* Proposal: 0 or 1 */
    }
}

int consensus_flood_set_round(network_t *net, node_t *node, round_t round) {
    if (!net || !node) return -1;

    /* Exchange current value with all neighbors */
    for (neighbor_entry_t *nb = node->neighbors; nb; nb = nb->next) {
        node_send_message(net, node->id, nb->neighbor_id,
                          MSG_APPLICATION, round,
                          &node->local_data, sizeof(int));
    }

    /* Update: if any neighbor has value 1, set to 1 */
    msg_queue_entry_t *entry = node->incoming.head;
    while (entry) {
        if (entry->msg.type == MSG_APPLICATION &&
            entry->msg.round == round &&
            entry->msg.payload) {
            int neighbor_val = *(int *)entry->msg.payload;
            if (neighbor_val == 1) {
                node->local_data = 1;
            }
        }
        entry = entry->next;
    }

    return 0;
}

int consensus_check(const network_t *net) {
    if (!net || net->num_nodes == 0) return 0;
    int first = net->nodes[0].local_data;
    for (int i = 1; i < net->num_nodes; i++) {
        if (net->nodes[i].local_data != first) return 0;
    }
    return 1;
}

void consensus_print(const network_t *net) {
    if (!net) return;
    printf("Consensus Results:\n");
    int v0 = 0, v1 = 0;
    for (int i = 0; i < net->num_nodes; i++) {
        printf("  Node %d: %d\n", i, net->nodes[i].local_data);
        if (net->nodes[i].local_data == 0) v0++;
        else v1++;
    }
    printf("  Total: 0=%d, 1=%d, Agreed=%s\n", v0, v1,
           consensus_check(net) ? "Yes" : "No");
}

/*============================================================================
 * Self-Stabilizing Synchronizer Protocol
 *
 * Implements a self-stabilizing version that recovers from arbitrary
 * transient faults. Uses periodic heartbeat exchange and round
 * reconciliation.
 *
 * Key idea: if a node is significantly behind its neighbors in round
 * number, it jumps forward to catch up (using the max round seen).
 * This ensures eventual convergence to a consistent state.
 *============================================================================*/

int self_stabilizing_synchro_round(network_t *net, node_t *node, round_t round) {
    if (!net || !node) return -1;

    /* Exchange heartbeat containing current round */
    for (neighbor_entry_t *nb = node->neighbors; nb; nb = nb->next) {
        node_send_message(net, node->id, nb->neighbor_id,
                          MSG_HEARTBEAT, round,
                          &node->current_round, sizeof(round_t));
    }

    /* Check neighbors' rounds and catch up if behind */
    round_t max_round = node->current_round;
    msg_queue_entry_t *entry = node->incoming.head;
    while (entry) {
        if (entry->msg.type == MSG_HEARTBEAT && entry->msg.payload) {
            round_t neighbor_round = *(round_t *)entry->msg.payload;
            if (neighbor_round > max_round) max_round = neighbor_round;
        }
        entry = entry->next;
    }

    if (max_round > node->current_round) {
        /* Catch up: jump to max_round (self-stabilization) */
        node->current_round = max_round;
        node->state = NODE_UNSAFE;
    }

    /* Reset detection: if we detect a RESET message, restart from 0 */
    entry = node->incoming.head;
    while (entry) {
        if (entry->msg.type == MSG_RESET) {
            node->current_round = 0;
            node->state = NODE_UNSAFE;
            node->local_data = 0;
            break;
        }
        entry = entry->next;
    }

    return 0;
}

void inject_random_faults(network_t *net, double fault_prob,
                           unsigned int seed) {
    if (!net) return;
    srand(seed);
    for (int i = 0; i < net->num_nodes; i++) {
        double r = (double)rand() / (double)RAND_MAX;
        if (r < fault_prob) {
            /* Corrupt state */
            net->nodes[i].current_round += (rand() % 10) - 5;
            if (net->nodes[i].current_round < 0)
                net->nodes[i].current_round = 0;
            net->nodes[i].state = (rand() % 2) ? NODE_SAFE : NODE_UNSAFE;
        }
    }
}

int synchro_check_stable(const network_t *net) {
    if (!net || net->num_nodes == 0) return 0;
    round_t first_round = net->nodes[0].current_round;
    for (int i = 1; i < net->num_nodes; i++) {
        if (net->nodes[i].current_round != first_round) return 0;
    }
    return 1;
}
