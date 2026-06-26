/**
 * distributed_algorithms.h — Synchronous Distributed Algorithms
 *
 * These algorithms are designed as synchronous protocols that can
 * be executed on top of any of the α, β, γ synchronizers.
 *
 * Knowledge coverage:
 *   L5: Algorithms — BFS, Leader Election, Broadcast, Convergecast, MST
 *   L6: Canonical Problems — spanning tree, consensus, counting
 *   L7: Applications — routing, sensor aggregation
 */

#ifndef DISTRIBUTED_ALGORITHMS_H
#define DISTRIBUTED_ALGORITHMS_H

#include "synchro.h"

/*============================================================================
 * L6: Distributed Breadth-First Search (BFS)
 *============================================================================*/

/**
 * Synchronous BFS protocol (one round).
 * In round 0, root marks itself as discovered.
 * In each subsequent round r, nodes discovered at round r-1 send
 * MSG_BFS_SEARCH to all neighbors. Unvisited neighbors mark themselves
 * as discovered and set the sender as their BFS parent.
 *
 * Theorem: After O(diameter) rounds, every node reachable from the root
 * has a shortest path to the root recorded in bfs_parent.
 *
 * @param net    Network
 * @param node   Node executing the protocol
 * @param round  Current synchronous round
 * @return 0 to continue, -1 if BFS is complete
 */
int bfs_protocol_round(network_t *net, node_t *node, round_t round);

/**
 * Reset BFS state on all nodes (clears discovery flags only).
 * Does NOT clear bfs_parent/depth to preserve synchronizer tree.
 */
void bfs_reset(network_t *net);

/**
 * Rebuild bfs_children arrays from bfs_parent relationships.
 * Must be called after BFS or any algorithm that populates bfs_parent,
 * before using convergecast/broadcast protocols.
 */
void bfs_build_children_arrays(network_t *net);

/**
 * Run BFS from a given root using the active synchronizer.
 * Returns the number of rounds (BFS depth).
 */
int bfs_run(network_t *net, node_id_t root);

/**
 * Print the BFS tree structure.
 */
void bfs_print_tree(const network_t *net);

/*============================================================================
 * L6: Distributed Leader Election (on rings and general graphs)
 *============================================================================*/

/**
 * LeLann-Chang-Roberts leader election on a ring.
 *
 * Algorithm: Each node sends its ID around the ring. When a node
 * receives an ID greater than its own, it passes it on. When a node
 * receives its own ID back, it knows it is the leader (max ID).
 *
 * Message complexity: O(n^2) worst case, O(n log n) average.
 *
 * The protocol is executed one round at a time via synchronizer.
 *
 * @param net    Network (must be a ring)
 * @param node   Node
 * @param round  Current round
 * @return 0 to continue, -1 to report leader found (local_data = leader ID)
 */
int leader_election_le_lann_round(network_t *net, node_t *node, round_t round);

/**
 * Reset leader election state.
 */
void leader_election_reset(network_t *net);

/**
 * Run leader election using LeLann protocol via synchronizer.
 * On completion, each node's local_data contains the leader ID.
 */
int leader_election_run(network_t *net);

/**
 * Print the leader election results.
 */
void leader_election_print_result(const network_t *net);

/*============================================================================
 * L6: Distributed Broadcast and Convergecast
 *============================================================================*/

/**
 * Broadcast protocol: a message from the root propagates down a spanning tree.
 * Each round, nodes that have received the broadcast forward it to their
 * children in the tree.
 *
 * @param net    Network with a spanning tree already computed
 * @param node   Node
 * @param round  Current round
 * @return 0 to continue, -1 if broadcast complete
 */
int broadcast_protocol_round(network_t *net, node_t *node, round_t round);

/**
 * Convergecast protocol: data aggregates from leaves to the root.
 * In each round, leaf nodes send their data to their parent. Internal nodes
 * aggregate (sum) data from children and forward up.
 *
 * @param net    Network
 * @param node   Node
 * @param round  Current round
 * @return 0 to continue, -1 if convergecast complete (root has total)
 */
int convergecast_sum_round(network_t *net, node_t *node, round_t round);

/*============================================================================
 * L6: Distributed Minimum Spanning Tree (Gallager-Humblet-Spira)
 *============================================================================*/

/**
 * GHS Minimum Spanning Tree protocol round.
 *
 * The Gallager-Humblet-Spira algorithm (1983) constructs a Minimum
 * Spanning Tree in O(n log n) messages, which is optimal.
 *
 * This is a simplified synchronous version that runs one "level" per
 * round. Fragments merge by selecting their minimum-weight outgoing edge.
 *
 * Message complexity: O(|E| log |V|).
 * Time complexity: O(|V| log |V|) rounds.
 *
 * @return 0 to continue, -1 if MST is complete
 */
int mst_ghs_protocol_round(network_t *net, node_t *node, round_t round);

/**
 * Initialize GHS MST state.
 */
void mst_ghs_init(network_t *net);

/**
 * Print the computed MST edges.
 */
void mst_ghs_print(const network_t *net);

/*============================================================================
 * L7: Distributed Sensor Network Aggregation
 *============================================================================*/

/**
 * Sensor aggregation protocol: each node has a sensor reading (local_sensor).
 * The goal is to compute the maximum reading across the network using
 * a convergecast-style tree aggregation.
 *
 * This demonstrates a real-world application of synchronizers in
 * wireless sensor networks (environmental monitoring, smart grids).
 */
int sensor_aggregate_max_round(network_t *net, node_t *node, round_t round);

/**
 * Initialize randomized sensor readings.
 */
void sensor_init_random(network_t *net, unsigned int seed);

/*============================================================================
 * L7: Distributed Consensus (flood-set algorithm)
 *============================================================================*/

/**
 * Flood-set consensus: each node starts with a proposed value (0 or 1).
 * In each round, nodes exchange their current decision value with neighbors.
 * After enough rounds (diameter + 1), all non-faulty nodes agree.
 *
 * This is the simplest fault-tolerant consensus algorithm for the
 * synchronous model with crash faults.
 */
int consensus_flood_set_round(network_t *net, node_t *node, round_t round);

/**
 * Initialize consensus with random proposals.
 */
void consensus_init_random(network_t *net, unsigned int seed);

/**
 * Check if consensus has been reached (all nodes have same value).
 */
int consensus_check(const network_t *net);

/**
 * Print consensus results.
 */
void consensus_print(const network_t *net);

/*============================================================================
 * L8: Self-Stabilizing Synchronizer
 *============================================================================*/

/**
 * Self-stabilizing synchronizer round.
 *
 * This implements a self-stabilizing variant that recovers from arbitrary
 * transient faults. Nodes periodically exchange heartbeat messages and
 * reset their state if inconsistency is detected.
 *
 * Reference: Dolev, Israeli, Moran (1997). "Self-stabilizing synchronizers."
 *
 * Key idea: periodic reset based on local consistency checking.
 * If a node detects its round is behind its neighbors, it catches up.
 *
 * @return 0 to continue, -1 if stable
 */
int self_stabilizing_synchro_round(network_t *net, node_t *node, round_t round);

/**
 * Inject random faults for testing self-stabilization.
 * @param fault_prob Probability of corrupting each node's state
 */
void inject_random_faults(network_t *net, double fault_prob, unsigned int seed);

/**
 * Check if the network has stabilized (all nodes at same round).
 */
int synchro_check_stable(const network_t *net);

/*============================================================================
 * Utility
 *============================================================================*/

/**
 * Compute the depth of the BFS tree (maximum distance from root).
 */
int bfs_tree_depth(const network_t *net);

/**
 * Verify that bfs_parent edges form a valid spanning tree.
 */
int bfs_verify_tree(const network_t *net, node_id_t root);

/**
 * Count the number of nodes reachable from a given root.
 */
int network_reachable_count(const network_t *net, node_id_t root);

#endif /* DISTRIBUTED_ALGORITHMS_H */
