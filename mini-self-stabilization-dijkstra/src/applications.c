/**
 * @file applications.c
 * @brief Real-world applications of self-stabilization theory
 *
 * Implements application-level patterns that use self-stabilization
 * principles to solve practical distributed systems problems:
 *   - Network routing table recovery after topology changes
 *   - Sensor network time synchronization
 *   - Fault-tolerant leader election
 *   - Distributed mutual exclusion for cloud services
 *   - Self-healing spanning tree construction
 *
 * Reference:
 *   Dolev, S. "Self-Stabilization." MIT Press, 2000, Chapters 5-8.
 *   Gartner, F.C. "A Survey of Self-Stabilizing Spanning-Tree
 *     Construction Algorithms." EPFL Technical Report, 2003.
 *   Herman, T. "Self-Stabilization Bibliography: Access Guide."
 *     Chicago Journal of Theoretical Computer Science, 2002.
 *
 * Course Mappings:
 *   MIT 6.841 — Applications in distributed systems
 *   Princeton COS 551 — Self-stabilizing network protocols
 *   Cambridge Part III — Practical stabilization
 *
 * Knowledge Levels: L7 (Applications)
 *
 * Knowledge Points (each function = 1 independent knowledge point):
 *
 * L7: ss_app_routing_recovery — self-stabilizing routing table repair
 * L7: ss_app_leader_election — self-stabilizing leader election
 * L7: ss_app_time_sync — sensor network time synchronization
 * L7: ss_app_mutual_exclusion_cloud — cloud-scale mutual exclusion
 * L7: ss_app_spanning_tree — self-healing spanning tree
 * L7: ss_app_blockchain_consensus — blockchain stabilization pattern
 */

#include "dijkstra_ring.h"
#include "self_stabilization.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

/* ── L7 Application 1: Network Routing Table Recovery ─────────────────── */

/**
 * @brief Simulated network node with a routing table.
 *
 * In a network, each node maintains a routing table mapping
 * destinations to next-hop neighbors. After a topology change
 * (link failure, node addition), routing tables may become
 * inconsistent, leading to routing loops or black holes.
 *
 * Self-stabilization provides a framework for protocols that
 * automatically repair routing tables without global reset.
 */
typedef struct {
    int32_t node_id;              /**< Unique node identifier */
    int32_t num_neighbors;        /**< Number of direct neighbors */
    int32_t *neighbor_ids;        /**< Neighbor node IDs */
    int32_t *routing_table;       /**< next_hop[dst] = neighbor to forward to */
    int32_t *distance;            /**< dist[dst] = estimated distance to dst */
    int32_t network_size;         /**< Total nodes in network */
    int32_t correct_next_hop;     /**< Ground truth next hop (for validation) */
} network_node_t;

/**
 * ss_app_routing_recovery — Demonstrate self-stabilizing routing recovery.
 *
 * Simulates a network where each node runs a distributed Bellman-Ford
 * distance-vector routing protocol. After injecting a fault (corrupting
 * routing tables), the system self-stabilizes back to correct routes.
 *
 * The self-stabilization property ensures that from any corrupted
 * routing state, the distance-vector algorithm converges to correct
 * shortest-path routes within finite time, without external intervention.
 *
 * This is the principle behind protocols like RIP (Routing Information
 * Protocol) and BGP path recovery.
 *
 * Knowledge: Self-stabilizing routing — one of the most impactful
 *   applications of self-stabilization. Internet routing protocols
 *   rely on convergence from arbitrary states after failures.
 *   The "count-to-infinity" problem in RIP is a classic example of
 *   what happens when convergence is too slow.
 *
 * @param num_nodes      Number of nodes in the network
 * @param fault_seed     Seed for deterministic fault injection
 * @return               Number of rounds to converge, or -1 if failed
 */
int32_t ss_app_routing_recovery(int32_t num_nodes, uint32_t fault_seed)
{
    network_node_t *nodes;
    int32_t i, j, round, converged;
    int32_t max_rounds;

    if (num_nodes < 2 || num_nodes > 100) return -1;

    nodes = (network_node_t *)calloc((size_t)num_nodes,
                                      sizeof(network_node_t));
    if (!nodes) return -1;

    /* Initialize a simple line topology (node i connected to i-1 and i+1) */
    for (i = 0; i < num_nodes; i++) {
        nodes[i].node_id = i;
        nodes[i].num_neighbors = (i == 0 || i == num_nodes - 1) ? 1 : 2;
        nodes[i].neighbor_ids = (int32_t *)calloc(
            (size_t)nodes[i].num_neighbors, sizeof(int32_t));
        nodes[i].routing_table = (int32_t *)calloc(
            (size_t)num_nodes, sizeof(int32_t));
        nodes[i].distance = (int32_t *)calloc(
            (size_t)num_nodes, sizeof(int32_t));
        nodes[i].network_size = num_nodes;

        if (i > 0) nodes[i].neighbor_ids[0] = i - 1;
        if (i < num_nodes - 1) {
            nodes[i].neighbor_ids[nodes[i].num_neighbors - 1] = i + 1;
        }

        /* Initialize routing table to correct values */
        for (j = 0; j < num_nodes; j++) {
            if (j < i) {
                nodes[i].routing_table[j] = i - 1; /* Forward left */
                nodes[i].distance[j] = i - j;
            } else if (j > i) {
                nodes[i].routing_table[j] = i + 1; /* Forward right */
                nodes[i].distance[j] = j - i;
            } else {
                nodes[i].routing_table[j] = i;     /* Self */
                nodes[i].distance[j] = 0;
            }
        }
    }

    /* Inject faults: corrupt routing tables using fault_seed */
    uint32_t rng = fault_seed;
    int32_t faults = num_nodes / 2 + 1;
    for (i = 0; i < faults; i++) {
        rng = (uint32_t)(((uint64_t)1103515245ULL * rng + 12345ULL)
                         & 0x7FFFFFFFULL);
        int32_t node = (int32_t)(rng % (uint32_t)num_nodes);
        rng = (uint32_t)(((uint64_t)1103515245ULL * rng + 12345ULL)
                         & 0x7FFFFFFFULL);
        int32_t dst = (int32_t)(rng % (uint32_t)num_nodes);
        rng = (uint32_t)(((uint64_t)1103515245ULL * rng + 12345ULL)
                         & 0x7FFFFFFFULL);
        int32_t bad_hop = (int32_t)(rng % (uint32_t)num_nodes);

        if (dst != node) {
            nodes[node].routing_table[dst] = bad_hop;
            nodes[node].distance[dst] = num_nodes * 10; /* Inflated */
        }
    }

    /* Self-stabilizing convergence: distributed Bellman-Ford */
    max_rounds = num_nodes * num_nodes * 2;
    for (round = 0; round < max_rounds; round++) {
        /* Each node updates its distance estimates based on neighbors */
        int32_t updated = 0;

        for (i = 0; i < num_nodes; i++) {
            for (j = 0; j < num_nodes; j++) {
                if (j == i) continue;
                int32_t best_dist = num_nodes * 20;
                int32_t best_hop = -1;
                int32_t n;

                for (n = 0; n < nodes[i].num_neighbors; n++) {
                    int32_t nb = nodes[i].neighbor_ids[n];
                    int32_t dist_via_nb = 1 + nodes[nb].distance[j];
                    if (dist_via_nb < best_dist) {
                        best_dist = dist_via_nb;
                        best_hop = nb;
                    }
                }

                if (best_dist < nodes[i].distance[j]) {
                    nodes[i].distance[j] = best_dist;
                    nodes[i].routing_table[j] = best_hop;
                    updated++;
                }
            }
        }

        if (updated == 0) {
            converged = 1;
            break; /* Converged */
        }
    }

    /* Verify correctness */
    converged = 1;
    for (i = 0; i < num_nodes && converged; i++) {
        for (j = 0; j < num_nodes; j++) {
            if (j == i) continue;
            /* On a line, correct distance is |i-j| */
            int32_t correct_dist = (j > i) ? (j - i) : (i - j);
            if (nodes[i].distance[j] != correct_dist) {
                converged = 0;
                break;
            }
        }
    }

    /* Cleanup */
    for (i = 0; i < num_nodes; i++) {
        free(nodes[i].neighbor_ids);
        free(nodes[i].routing_table);
        free(nodes[i].distance);
    }
    free(nodes);

    return converged ? round : -1;
}

/* ── L7 Application 2: Self-Stabilizing Leader Election ────────────────── */

/**
 * ss_app_leader_election — Self-stabilizing leader election on a ring.
 *
 * Leader election is the problem of designating a single node as
 * the "leader" in a distributed system. A self-stabilizing leader
 * election algorithm converges to a unique leader from any initial
 * configuration, even after leader failure.
 *
 * This implementation uses Dijkstra's token ring as the basis:
 * the machine holding the token is the leader. Since the ring
 * self-stabilizes to exactly one token, exactly one leader emerges.
 *
 * Knowledge: Self-stabilizing leader election — a fundamental
 *   building block for fault-tolerant distributed systems.
 *   Applications include primary-backup replication (where the
 *   leader is the primary), distributed coordination (ZooKeeper,
 *   etcd), and master election in database clusters.
 *
 * @param n           Number of machines in the ring
 * @param max_rounds  Maximum election rounds
 * @param seed        Random seed for initial state
 * @return            Leader ID after convergence, or -1 if failed
 */
int32_t ss_app_leader_election(int32_t n, int32_t max_rounds, uint32_t seed)
{
    ring_configuration_t config;
    int32_t steps, leader_id, i;

    if (n < 2 || n > 128) return -1;

    if (ring_init(&config, n, 3, ALGORITHM_3STATE) < 0) return -1;
    ring_randomize_states(&config, seed);

    /* Converge to legitimate state (single token) */
    steps = ring_converge_to_legitimate(&config, max_rounds, NULL, 0);

    if (steps < 0) {
        ring_destroy(&config);
        return -1;
    }

    /* The leader is the machine holding the token */
    leader_id = -1;
    for (i = 0; i < n; i++) {
        privilege_t p = ring_compute_privilege(&config, i);
        if (p != PRIVILEGE_NONE) {
            leader_id = i;
            break;
        }
    }

    ring_destroy(&config);
    return leader_id;
}

/* ── L7 Application 3: Sensor Network Time Synchronization ─────────────── */

/**
 * @brief Sensor node state for time synchronization.
 *
 * In a wireless sensor network, nodes need synchronized clocks
 * for coordinated sensing, TDMA scheduling, and power management.
 */
typedef struct {
    int32_t  node_id;
    int64_t  logical_clock;        /**< Lamport-style logical clock */
    double   drift_rate;           /**< Clock drift rate (ppm) */
    int32_t  num_neighbors;
    int32_t *neighbor_ids;
    int64_t *neighbor_clocks;      /**< Latest known neighbor clock values */
    bool     synchronized;         /**< True if within sync tolerance */
} sensor_node_t;

/**
 * ss_app_time_sync — Self-stabilizing time synchronization.
 *
 * Simulates a sensor network where each node maintains a logical
 * clock that drifts over time. Nodes periodically exchange clock
 * values with neighbors and adjust toward the maximum (or average).
 *
 * After a fault (e.g., a node's clock is corrupted), the network
 * self-stabilizes to a synchronized state where all clocks agree
 * within a small tolerance.
 *
 * This models the Firefly synchronization algorithm and the
 * Gradient Time Synchronization Protocol (GTSP) used in wireless
 * sensor networks like NASA's Mars sensor webs and IoT deployments.
 *
 * Knowledge: Self-stabilizing clock synchronization — essential
 *   for wireless sensor networks, distributed data collection,
 *   and coordinated actuation. The "firefly" model (Mirollo &
 *   Strogatz, 1990) inspires many practical protocols.
 *
 * @param num_nodes    Number of sensor nodes
 * @param tolerance    Maximum allowed clock difference
 * @param max_rounds   Maximum synchronization rounds
 * @param seed         Random seed
 * @return             Rounds to synchronize, or -1 if failed
 */
int32_t ss_app_time_sync(int32_t num_nodes, int64_t tolerance,
                          int32_t max_rounds, uint32_t seed)
{
    sensor_node_t *nodes;
    int32_t i, round;
    int32_t synchronized_round = -1;
    uint32_t rng = seed;

    if (num_nodes < 2 || num_nodes > 100) return -1;

    nodes = (sensor_node_t *)calloc((size_t)num_nodes,
                                     sizeof(sensor_node_t));
    if (!nodes) return -1;

    /* Initialize nodes with random clock values (desynchronized) */
    for (i = 0; i < num_nodes; i++) {
        nodes[i].node_id = i;
        rng = (uint32_t)(((uint64_t)1103515245ULL * rng + 12345ULL)
                         & 0x7FFFFFFFULL);
        nodes[i].logical_clock = (int64_t)(rng % 10000); /* Up to 10000 drift */
        /* Drift rates: ±50 ppm randomly assigned */
        nodes[i].drift_rate = ((double)((int32_t)(rng % 101)) - 50.0) * 1e-6;

        /* Ring topology: each node connected to predecessor and successor */
        nodes[i].num_neighbors = 2;
        nodes[i].neighbor_ids = (int32_t *)calloc(2, sizeof(int32_t));
        nodes[i].neighbor_ids[0] = (i == 0) ? (num_nodes - 1) : (i - 1);
        nodes[i].neighbor_ids[1] = (i == num_nodes - 1) ? 0 : (i + 1);
        nodes[i].neighbor_clocks = (int64_t *)calloc(2, sizeof(int64_t));
        nodes[i].synchronized = false;
    }

    /* Synchronization rounds */
    for (round = 0; round < max_rounds; round++) {
        /* Each node simulates clock drift */
        for (i = 0; i < num_nodes; i++) {
            /* Drift: clock += 1 + drift * clock */
            nodes[i].logical_clock += 1;
            if (nodes[i].drift_rate != 0.0) {
                nodes[i].logical_clock +=
                    (int64_t)((double)nodes[i].logical_clock
                              * nodes[i].drift_rate);
            }
        }

        /* Each node reads neighbors' clocks */
        for (i = 0; i < num_nodes; i++) {
            nodes[i].neighbor_clocks[0] =
                nodes[nodes[i].neighbor_ids[0]].logical_clock;
            nodes[i].neighbor_clocks[1] =
                nodes[nodes[i].neighbor_ids[1]].logical_clock;
        }

        /* Each node adjusts toward the maximum clock seen */
        for (i = 0; i < num_nodes; i++) {
            int64_t max_seen = nodes[i].logical_clock;
            int32_t n;
            for (n = 0; n < nodes[i].num_neighbors; n++) {
                if (nodes[i].neighbor_clocks[n] > max_seen) {
                    max_seen = nodes[i].neighbor_clocks[n];
                }
            }
            /* Move halfway toward max (conservative adjustment) */
            nodes[i].logical_clock =
                (nodes[i].logical_clock + max_seen) / 2;
        }

        /* Check synchronization */
        int64_t min_clock = nodes[0].logical_clock;
        int64_t max_clock = nodes[0].logical_clock;
        for (i = 1; i < num_nodes; i++) {
            if (nodes[i].logical_clock < min_clock)
                min_clock = nodes[i].logical_clock;
            if (nodes[i].logical_clock > max_clock)
                max_clock = nodes[i].logical_clock;
        }

        if ((max_clock - min_clock) <= tolerance) {
            synchronized_round = round;
            break;
        }
    }

    /* Cleanup */
    for (i = 0; i < num_nodes; i++) {
        free(nodes[i].neighbor_ids);
        free(nodes[i].neighbor_clocks);
    }
    free(nodes);

    return synchronized_round;
}

/* ── L7 Application 4: Cloud-Scale Mutual Exclusion ────────────────────── */

/**
 * ss_app_mutual_exclusion_cloud — Distributed mutual exclusion for
 * cloud services using self-stabilizing token passing.
 *
 * In a cloud environment, multiple server instances need exclusive
 * access to a shared resource (e.g., a database partition, a
 * configuration update). A self-stabilizing token ring provides
 * fault-tolerant mutual exclusion: the server holding the token
 * has exclusive access, and the system recovers automatically
 * from token loss, duplication, or server failures.
 *
 * This pattern is used in:
 *   - ZooKeeper leader election (Apache)
 *   - etcd distributed locks (CoreOS)
 *   - Chubby lock service (Google)
 *   - AWS DynamoDB lock client
 *
 * Knowledge: Cloud mutual exclusion — in modern distributed systems,
 *   self-stabilizing mutual exclusion enables automatic recovery
 *   from partition events, crash failures, and operator errors
 *   without manual intervention.
 *
 * @param n            Number of server instances
 * @param fault_count  Number of simulated faults
 * @param max_rounds   Maximum rounds per fault recovery
 * @param seed         Random seed
 * @return             Number of successful recoveries
 */
int32_t ss_app_mutual_exclusion_cloud(int32_t n, int32_t fault_count,
                                       int32_t max_rounds, uint32_t seed)
{
    ring_configuration_t config;
    int32_t i, recoveries = 0;
    uint32_t rng = seed;

    if (n < 2 || n > 128 || fault_count < 1) return -1;

    if (ring_init(&config, n, 3, ALGORITHM_3STATE) < 0) return -1;

    /* Start in legitimate state */
    for (i = 0; i < n; i++) {
        ring_set_state(&config, i, (i == 0) ? 0 : 0);
    }
    /* Create token at bottom machine */
    ring_set_state(&config, n - 1, 0); /* S[N-1] == S[0] gives bottom priv */
    /* Actually, let's set up a proper legitimate state */
    /* All machines state 0, bottom has token because S[0] == S[N-1] */
    /* After bottom executes: S[0] becomes 1, breaking equality */
    ring_step_3state(&config, 0); /* Now all 0 except S[0]=1 */
    /* Now machine 1 has privilege (S[1]=0 != S[0]=1), machine 1 copies S[0] */
    ring_step_3state(&config, 1); /* S[1]=1 */
    /* Continue propagating... actually let's just randomize and converge */

    ring_randomize_states(&config, rng);
    int32_t steps = ring_converge_to_legitimate(&config, max_rounds, NULL, 0);
    if (steps < 0) {
        ring_destroy(&config);
        return 0;
    }
    recoveries++;

    /* Inject faults and verify recovery */
    for (i = 1; i < fault_count; i++) {
        rng = (uint32_t)(((uint64_t)1103515245ULL * rng + 12345ULL)
                         & 0x7FFFFFFFULL);

        /* Corrupt a random machine's state */
        int32_t victim = (int32_t)(rng % (uint32_t)n);
        rng = (uint32_t)(((uint64_t)1103515245ULL * rng + 12345ULL)
                         & 0x7FFFFFFFULL);
        int32_t bad_state = (int32_t)(rng % (uint32_t)config.num_states);
        ring_set_state(&config, victim, bad_state);

        /* Attempt recovery */
        steps = ring_converge_to_legitimate(&config, max_rounds, NULL, 0);
        if (steps >= 0) recoveries++;
    }

    ring_destroy(&config);
    return recoveries;
}

/* ── L7 Application 5: Self-Healing Spanning Tree ──────────────────────── */

/**
 * @brief Spanning tree node for self-stabilizing tree construction.
 */
typedef struct {
    int32_t node_id;
    int32_t parent_id;         /**< Current parent in spanning tree */
    int32_t root_id;           /**< Believed root of the tree */
    int32_t distance_to_root;  /**< Hop distance to believed root */
    int32_t num_neighbors;
    int32_t *neighbor_ids;
} spanning_tree_node_t;

/**
 * ss_app_spanning_tree — Self-stabilizing spanning tree construction.
 *
 * Constructs a breadth-first-search (BFS) spanning tree in a
 * distributed network. The algorithm is self-stabilizing: from
 * any initial parent assignments, it converges to a valid BFS tree
 * rooted at a designated root node.
 *
 * The algorithm (based on Dolev, 2000, Chapter 5):
 *   - Root node: always sets parent = self, distance = 0
 *   - Non-root nodes: periodically check neighbors; select as parent
 *     the neighbor with smallest distance to root, then set own
 *     distance = parent.distance + 1
 *
 * This self-healing property is critical for network layer protocols:
 * after a node failure, the spanning tree automatically repairs
 * without global recomputation.
 *
 * Applications: Ethernet spanning tree protocol (STP), wireless
 * mesh network routing, data aggregation trees in sensor networks.
 *
 * Knowledge: Self-stabilizing spanning trees — the spanning tree
 *   is a fundamental communication structure in networks. Self-
 *   stabilization ensures that the tree automatically recovers
 *   from topology changes, a property essential for robust network
 *   infrastructure (IEEE 802.1D Spanning Tree Protocol).
 *
 * @param num_nodes    Number of nodes (node 0 is root)
 * @param max_rounds   Maximum rounds
 * @param seed         Random seed for initial state
 * @return             Rounds to converge, or -1 if failed
 */
int32_t ss_app_spanning_tree(int32_t num_nodes, int32_t max_rounds,
                              uint32_t seed)
{
    spanning_tree_node_t *nodes;
    int32_t i, round;
    uint32_t rng = seed;

    if (num_nodes < 2 || num_nodes > 100) return -1;

    nodes = (spanning_tree_node_t *)calloc((size_t)num_nodes,
                                            sizeof(spanning_tree_node_t));
    if (!nodes) return -1;

    /* Initialize with random parent assignments (corrupted state) */
    for (i = 0; i < num_nodes; i++) {
        nodes[i].node_id = i;
        nodes[i].root_id = 0; /* Node 0 is the root */
        rng = (uint32_t)(((uint64_t)1103515245ULL * rng + 12345ULL)
                         & 0x7FFFFFFFULL);
        nodes[i].parent_id = (i == 0) ? 0 : (int32_t)(rng % (uint32_t)i);
        nodes[i].distance_to_root = (i == 0) ? 0 : (int32_t)(rng % 100);
        nodes[i].num_neighbors = (i == 0 || i == num_nodes - 1) ? 1 : 2;
        nodes[i].neighbor_ids = (int32_t *)calloc(
            (size_t)nodes[i].num_neighbors, sizeof(int32_t));
        if (i > 0) nodes[i].neighbor_ids[0] = i - 1;
        if (i < num_nodes - 1) {
            nodes[i].neighbor_ids[nodes[i].num_neighbors - 1] = i + 1;
        }
    }

    /* Self-stabilizing BFS spanning tree construction */
    for (round = 0; round < max_rounds; round++) {
        int32_t changed = 0;

        for (i = 0; i < num_nodes; i++) {
            if (i == 0) {
                /* Root: always correct */
                if (nodes[i].parent_id != 0) {
                    nodes[i].parent_id = 0;
                    nodes[i].distance_to_root = 0;
                    changed++;
                }
                continue;
            }

            /* Find best parent: neighbor with smallest distance */
            int32_t best_parent = -1;
            int32_t best_dist = num_nodes + 100;
            int32_t n;

            for (n = 0; n < nodes[i].num_neighbors; n++) {
                int32_t nb = nodes[i].neighbor_ids[n];
                int32_t dist_via_nb = 1 + nodes[nb].distance_to_root;
                if (dist_via_nb < best_dist
                    && nodes[nb].distance_to_root < num_nodes) {
                    best_dist = dist_via_nb;
                    best_parent = nb;
                }
            }

            if (best_parent >= 0 &&
                (nodes[i].parent_id != best_parent ||
                 nodes[i].distance_to_root != best_dist)) {
                nodes[i].parent_id = best_parent;
                nodes[i].distance_to_root = best_dist;
                changed++;
            }
        }

        if (changed == 0) {
            /* Verify correctness */
            int32_t correct = 1;
            for (i = 0; i < num_nodes; i++) {
                if (i == 0) {
                    if (nodes[i].distance_to_root != 0) { correct = 0; break; }
                } else {
                    /* Distance should be exactly the shortest path */
                    if (nodes[i].distance_to_root != i) { correct = 0; break; }
                }
            }
            if (correct) {
                /* Cleanup and return */
                for (i = 0; i < num_nodes; i++) {
                    free(nodes[i].neighbor_ids);
                }
                free(nodes);
                return round;
            }
        }
    }

    /* Cleanup */
    for (i = 0; i < num_nodes; i++) {
        free(nodes[i].neighbor_ids);
    }
    free(nodes);
    return -1;
}

/* ── L7 Application 6: Blockchain Consensus Stabilization ──────────────── */

/**
 * ss_app_blockchain_consensus — Self-stabilization pattern for
 * blockchain consensus recovery.
 *
 * In proof-of-stake blockchain systems, validator sets may become
 * desynchronized after network partitions. A self-stabilizing
 * approach ensures that validators re-converge on a single chain
 * without hard forks.
 *
 * This function simulates a simplified blockchain validator ring
 * where each validator maintains a "view" (which block they consider
 * the tip). Using Dijkstra-like token propagation, validators
 * converge to consensus on the canonical chain.
 *
 * Knowledge: Blockchain self-stabilization — an emerging application
 *   of self-stabilization theory. As blockchain systems scale,
 *   self-stabilizing recovery from forks and network partitions
 *   becomes essential for safety and liveness (e.g., Ethereum's
 *   Gasper consensus, Tendermint recovery).
 *
 * @param num_validators Number of validators
 * @param max_rounds     Maximum consensus rounds
 * @param seed           Random seed
 * @return               Rounds to consensus, or -1 if failed
 */
int32_t ss_app_blockchain_consensus(int32_t num_validators,
                                     int32_t max_rounds, uint32_t seed)
{
    ring_configuration_t config;
    int32_t steps;

    if (num_validators < 2 || num_validators > 128) return -1;

    /* Use Dijkstra's ring as a consensus substrate:
       The token represents the "canonical block" being propagated.
       When all validators agree (single token), consensus is reached. */
    if (ring_init(&config, num_validators, 3, ALGORITHM_3STATE) < 0) return -1;

    ring_randomize_states(&config, seed);

    steps = ring_converge_to_legitimate(&config, max_rounds, NULL, 0);

    ring_destroy(&config);
    return steps; /* steps = rounds to consensus; -1 = failed */
}
