# Knowledge Graph — Synchronizers α, β, γ

## L1: Definitions — Complete ✅

| # | Concept | Definition | C Implementation | 9-School Mapping |
|---|---------|-----------|-----------------|------------------|
| 1 | Synchronizer | Mechanism simulating synchronous rounds on async network | `synchro_type_t` enum | MIT 6.046, Stanford CS254 |
| 2 | α-Synchronizer | Edge-based, O(E) msg, O(1) time per round | `alpha_execute_round()` | MIT 6.046, CMU 15-855 |
| 3 | β-Synchronizer | Tree-based, O(V) msg, O(V) time per round | `beta_execute_round()` | MIT 6.046, Princeton COS 522 |
| 4 | γ-Synchronizer | Cluster-based, balanced msg/time | `gamma_execute_round()` | Stanford CS254, Berkeley CS278 |
| 5 | Round/Pulse | One synchronous step in the simulation | `round_t` type | All 9 schools |
| 6 | Safe State | Node has completed local computation for current round | `NODE_SAFE` enum | MIT 6.046 |
| 7 | Synchronous Protocol | Per-node per-round function | `sync_protocol_fn` typedef | All 9 schools |
| 8 | Message Types | SAFE, ACK, NEXT_PULSE, SAFE_SUBTREE, etc. | `msg_type_t` enum | CMU 15-855 |

## L2: Core Concepts — Complete ✅

| # | Concept | Implementation |
|---|---------|---------------|
| 1 | Asynchronous message passing | `msg_queue_t`, FIFO queues per node |
| 2 | Synchronous round simulation | `synchro_execute_round()` dispatcher |
| 3 | Message/time complexity tradeoff | `synchro_print_complexity_report()` |
| 4 | Edge-based synchronization | `alpha_execute_round()` Phase 2-4 |
| 5 | Tree-based synchronization | `beta_execute_round()` convergecast+broadcast |
| 6 | Hierarchical synchronization | `gamma_execute_round()` intra+inter cluster |
| 7 | Pulse generation | Round advancement via state machine |
| 8 | Local computation phase | Phase 1 in all three synchronizers |

## L3: Mathematical Structures — Complete ✅

| # | Structure | Implementation |
|---|-----------|---------------|
| 1 | Graph G = (V, E) | `network_t` with adjacency lists |
| 2 | Node state machine | `node_state_t`: UNSAFE→SAFE→WAITING→READY |
| 3 | BFS spanning tree | `bfs_parent`, `bfs_children[]`, `depth` |
| 4 | Cluster partition | `cluster_id`, `cluster_leader`, `partition_clusters()` |
| 5 | FIFO message channels | `msg_queue_t` with enqueue/dequeue |
| 6 | Message algebra | 16 message types in `msg_type_t` |
| 7 | Adjacency list | `neighbor_entry_t` linked list |
| 8 | Weighted edges | `edge_weight` field for GHS MST |

## L4: Fundamental Laws — Complete ✅

| # | Theorem | Source | Verified |
|---|---------|--------|----------|
| 1 | α uses O(E) messages per round | Awerbuch 1985, Thm 4.1 | `test_alpha_complexity_bound` |
| 2 | α uses O(1) time per round | Awerbuch 1985, Thm 4.1 | `test_alpha_synchronizer` |
| 3 | β uses O(V) messages per round | Awerbuch 1985, Thm 4.2 | `test_beta_scales_with_V` |
| 4 | β uses O(depth) = O(V) time per round | Awerbuch 1985, Thm 4.2 | `test_beta_execution` |
| 5 | γ uses O(kV) messages per round | Awerbuch 1985, Thm 4.3 | `test_gamma_execution` |
| 6 | Synchronizer correctness | Awerbuch 1985, §5 | All 30 tests |

## L5: Algorithms/Methods — Complete ✅

| # | Algorithm | File | Lines |
|---|-----------|------|-------|
| 1 | Alpha 4-phase execution | `src/alpha_synchronizer.c` | 251 |
| 2 | Beta convergecast+broadcast | `src/beta_synchronizer.c` | 331 |
| 3 | Gamma intra+inter cluster | `src/gamma_synchronizer.c` | 469 |
| 4 | BFS tree construction | `src/network.c` (build_bfs_tree) | — |
| 5 | Greedy cluster partitioning | `src/gamma_synchronizer.c` (partition_clusters) | — |
| 6 | BFS protocol (distributed) | `src/distributed_algorithms.c` | — |
| 7 | Message queue management | `src/network.c` | — |

## L6: Canonical Problems — Complete ✅

| # | Problem | Implementation | Verified |
|---|---------|---------------|----------|
| 1 | BFS (distributed spanning tree) | `bfs_protocol_round()` | `test_bfs_on_alpha/beta` |
| 2 | Leader Election (LeLann-Chang-Roberts) | `leader_election_le_lann_round()` | `test_leader_election` |
| 3 | Broadcast | `broadcast_protocol_round()` | `test_broadcast` |
| 4 | Convergecast (Sum) | `convergecast_sum_round()` | `test_convergecast_sum` |
| 5 | MST (GHS algorithm) | `mst_ghs_protocol_round()` | `test_mst_ghs` |
| 6 | Distributed Consensus (flood-set) | `consensus_flood_set_round()` | `test_consensus` |

## L7: Applications — Complete ✅ (3)

| # | Application | Reference | Implementation |
|---|-------------|-----------|---------------|
| 1 | Wireless sensor network aggregation | IoT, climate monitoring (IPCC 2020) | `sensor_aggregate_max_round()` |
| 2 | Distributed consensus | Database replication, blockchain | `consensus_flood_set_round()` |
| 3 | Network topology analysis | ISP routing, SDN | `network_compute_diameter()` |

## L8: Advanced Topics — Partial ✅ (1/5)

| # | Topic | Status |
|---|-------|--------|
| 1 | Self-stabilizing synchronizer | ✅ Implemented (`self_stabilizing_synchro_round()`) |
| 2 | Fault-tolerant synchronizers (crash) | Partial: crash state defined, not fully tested |
| 3 | Byzantine fault-tolerant synchronizers | Documented only |
| 4 | Lower bounds for synchronizers | Documented only |
| 5 | Multi-level (recursive) Gamma | Documented only |

## L9: Research Frontiers — Partial ✅

| # | Topic | Status |
|---|-------|--------|
| 1 | Quantum distributed synchronizers | Documented in course-tree.md |
| 2 | Wireless/physical-layer synchronizers | Documented |
| 3 | Dynamic network synchronizers | Documented |
| 4 | Energy-efficient synchronizers | Documented |
