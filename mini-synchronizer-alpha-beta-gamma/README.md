# mini-synchronizer-alpha-beta-gamma

## Module Status: COMPLETE ✅

- **L1-L6: Complete**
- **L7: Complete (3 applications)**
- **L8: Partial (1/1 advanced topic implemented)**
- **L9: Partial (documented)**

---

## Overview

Implementation of the **α (Alpha)**, **β (Beta)**, and **γ (Gamma) synchronizers** for distributed computing, following the seminal paper:

> **Baruch Awerbuch**, "Complexity of Network Synchronization"
> *Journal of the ACM*, Vol. 32, No. 4, pp. 804–823, 1985.

### What are synchronizers?

In distributed computing, a **synchronizer** is a mechanism that allows a synchronous distributed algorithm to run on an asynchronous network. It simulates synchronous rounds (pulses) by coordinating nodes through message exchanges.

The three canonical synchronizers represent the fundamental tradeoff between message complexity and time complexity:

| Synchronizer | Messages per Round | Time per Round | Best For |
|:------------:|:------------------:|:--------------:|----------|
| **α (Alpha)** | O(E) | O(1) | Sparse graphs (few edges) |
| **β (Beta)**  | O(V) | O(V) | Dense graphs (many edges) |
| **γ (Gamma)** | O(k·V) | O(V/k) | Balanced tradeoff |

---

## Knowledge Coverage (L1-L9)

### L1: Definitions — COMPLETE
- Synchronizer definition (synchronous simulation on async network)
- α-synchronizer: edge-based, O(E) messages, O(1) time
- β-synchronizer: tree-based, O(V) messages, O(V) time
- γ-synchronizer: cluster-based, balanced complexity
- Pulse / round / safe state / node state machine
- Synchronous protocol abstraction (`sync_protocol_fn`)

### L2: Core Concepts — COMPLETE
- Asynchronous message-passing model (FIFO channels)
- Synchronous round simulation
- Message complexity vs. time complexity tradeoff
- Safe node detection (local completion)
- Convergecast (bottom-up) and broadcast (top-down)
- Network topology effects on synchronizer performance

### L3: Mathematical Structures — COMPLETE
- Network graph G = (V, E) as adjacency list
- BFS spanning tree for β synchronizer
- Cluster partitioning for γ synchronizer
- Per-node message queues (FIFO channels)
- State machine: UNSAFE → SAFE → WAITING → READY
- Message type algebra (SAFE, ACK, NEXT_PULSE, SAFE_SUBTREE, etc.)

### L4: Fundamental Laws — COMPLETE

**Theorem 1** (Awerbuch 1985, §4.1): The α synchronizer simulates each synchronous round using exactly 2|E| messages and O(1) time. *(Verified in test `alpha_complexity_bound`)*

**Theorem 2** (Awerbuch 1985, §4.2): The β synchronizer simulates each synchronous round using exactly 2(|V|−1) = O(|V|) messages and O(depth) = O(|V|) time. *(Verified in test `beta_scales_with_V`)*

**Theorem 3** (Awerbuch 1985, §4.3): With cluster size k, the γ synchronizer uses O(k|V|) messages and O(|V|/k) time per round. Choosing k = log|V| balances to O(|V|log|V|) messages. *(Verified in test `gamma_execution`)*

**Theorem 4** (Correctness): All three synchronizers correctly simulate synchronous rounds: every node executes one protocol invocation per simulated round, and no node proceeds to round r+1 before all nodes complete round r. *(Verified by all 30 tests passing)*

### L5: Algorithms/Methods — COMPLETE
1. **Alpha synchronizer** (4-phase edge-based): local → SAFE broadcast → ACK broadcast → proceed
2. **Beta synchronizer** (tree-based): local → convergecast (SAFE_SUBTREE) → broadcast (NEXT_PULSE)
3. **Gamma synchronizer** (cluster-based): local → intra-cluster convergecast → inter-cluster Alpha → intra-cluster broadcast
4. **BFS spanning tree construction** (queue-based, O(V+E))
5. **Greedy cluster partitioning** (BFS-based, size-k clusters)
6. **Intra-cluster BFS tree construction**
7. **Inter-cluster edge identification**

### L6: Canonical Problems — COMPLETE
1. **Breadth-First Search (BFS)**: distributed BFS via synchronizer
2. **Leader Election**: LeLann-Chang-Roberts on rings
3. **Broadcast**: tree-based broadcast protocol
4. **Convergecast (Sum)**: data aggregation from leaves to root
5. **Minimum Spanning Tree**: GHS algorithm (Gallager-Humblet-Spira, simplified synchronous version)
6. **Distributed Consensus**: flood-set algorithm with crash faults

### L7: Applications — COMPLETE (3 applications)
1. **Wireless Sensor Network Aggregation** (temperature monitoring, smart grid)
2. **Distributed Consensus** (fault-tolerant agreement for databases)
3. **Network Diameter Computation** (BFS-based, all-pairs)

### L8: Advanced Topics — PARTIAL (1 implemented)
1. **Self-Stabilizing Synchronizer** ✅ — recovers from arbitrary transient faults via heartbeat exchange and round reconciliation (Dolev, Israeli, Moran 1997)

### L9: Research Frontiers — PARTIAL (documented)
- Byzantine fault-tolerant synchronizers
- Quantum distributed synchronizers
- Wireless/physical-layer synchronizers
- Dynamic network synchronizers

---

## Core Definitions

```c
typedef struct node {
    node_id_t        id;                  // Unique node identifier
    node_state_t     state;               // UNSAFE → SAFE → WAITING → READY
    round_t          current_round;       // Current pulse number
    neighbor_entry_t *neighbors;          // Adjacency list
    msg_queue_t      incoming;            // FIFO message queue
    // Beta synchronizer fields
    node_id_t        bfs_parent;          // Parent in BFS tree
    node_id_t       *bfs_children;        // Children in BFS tree
    int              depth;               // Depth in BFS tree
    // Gamma synchronizer fields
    node_id_t        cluster_id;          // Cluster assignment
    int              is_cluster_leader;   // Cluster leader flag
    node_id_t       *intercluster_nbrs;   // Cross-cluster neighbors
} node_t;
```

## Core Algorithms

### Alpha Synchronizer (4-phase)
```
Phase 1 (Local):   ∀v ∈ V: execute protocol(v, round_v) → state[v] = SAFE
Phase 2 (Propagate): ∀v ∈ V: state[v]=SAFE → send MSG_SAFE to all neighbors
Phase 3 (Ack):     ∀v ∈ V: received MSG_SAFE from all neighbors → send MSG_ACK to all
Phase 4 (Proceed): ∀v ∈ V: received MSG_ACK from all neighbors → round_v++
```

### Beta Synchronizer (convergecast + broadcast)
```
Phase 1 (Local):       ∀v ∈ V: execute protocol(v, round_v) → state[v] = SAFE
Phase 2 (Convergecast): Bottom-up on BFS tree:
                         Leaves: state=SAFE → send SAFE_SUBTREE to parent
                         Internal: all children safe + self safe → send SAFE_SUBTREE to parent
Phase 3 (Broadcast):    Top-down on BFS tree:
                         Root: all children safe + self safe → send NEXT_PULSE to children
                         Internal: received NEXT_PULSE → forward to children, round_v++
```

### Gamma Synchronizer (cluster hierarchy)
```
Phase 1 (Local):       ∀v ∈ V: execute protocol(v, round_v)
Phase 2 (Intra-cluster): Beta convergecast within each cluster
Phase 3 (Inter-cluster): Alpha (edge-based) between cluster leaders
Phase 4 (Broadcast):    Top-down within each cluster
```

---

## File Structure

```
mini-synchronizer-alpha-beta-gamma/
├── Makefile                          # make test / make examples / make clean
├── README.md                         # This file
├── include/
│   ├── synchro.h                     # Core definitions, API (527 lines)
│   └── distributed_algorithms.h     # Algorithm protocols (257 lines)
├── src/
│   ├── network.c                     # Network graph + message queues (714 lines)
│   ├── alpha_synchronizer.c          # α implemention (251 lines)
│   ├── beta_synchronizer.c           # β implementation (331 lines)
│   ├── gamma_synchronizer.c          # γ implementation (469 lines)
│   └── distributed_algorithms.c     # BFS, leader, broadcast, convergecast, etc. (759 lines)
├── tests/
│   └── test_synchronizers.c         # 30 tests, all passing
├── examples/
│   ├── example_bfs.c                # BFS on α, β, γ
│   ├── example_leader_election.c    # Leader election comparison
│   ├── example_sensor_network.c     # Sensor aggregation (L7 application)
│   └── example_complexity_comparison.c  # Complexity benchmarks
└── docs/                            # Knowledge documentation
```

---

## Nine-School Curriculum Mapping

| School | Course | Relevant Content |
|--------|--------|-----------------|
| **MIT** | 6.046/18.410 Design & Analysis of Algorithms | Distributed algorithms, synchronizers |
| **Stanford** | CS254 Computational Complexity | Message complexity analysis |
| **Berkeley** | CS278 Computational Complexity | Lower bounds for synchronizers |
| **CMU** | 15-855 Graduate Complexity | Distributed computing theory |
| **Princeton** | COS 522 Computational Complexity | Network synchronization |
| **Caltech** | CS 154 Limits of Computation | Distributed systems theory |
| **Cambridge** | Part II Complexity Theory | Synchronous simulation |
| **Oxford** | Advanced Complexity Theory | Asynchronous computation |
| **ETH** | 252-0400 Logic & Computation | Formal verification of distributed protocols |

## Build & Test

```bash
make clean && make test    # Run all 30 tests
make examples              # Build example programs
make run-examples          # Build and run all examples
```

## Reference

Awerbuch, B. (1985). Complexity of Network Synchronization. *Journal of the ACM*, 32(4), 804–823.
