# Course Tree — Prerequisite Dependencies

## Prerequisites

This module depends on knowledge from the following areas:

```
Graph Theory
  └── Undirected graphs, adjacency lists, BFS, spanning trees
       └── network.c: graph operations, BFS construction
  
Distributed Computing Basics
  └── Asynchronous message passing model
  └── FIFO channels
  └── Node state machines
       └── msg_queue_t, node_state_t
  
Algorithm Analysis
  └── Asymptotic notation: O(|V|), O(|E|)
  └── Message complexity vs. time complexity
       └── synchro_print_complexity_report()
```

## Internal Dependencies

```
network.c (graph + message queues)
  ├── alpha_synchronizer.c (edge-based sync)
  ├── beta_synchronizer.c (needs BFS tree from network.c)
  ├── gamma_synchronizer.c (needs both alpha + beta primitives)
  └── distributed_algorithms.c (needs all three synchronizers + network)
```

## Forward Dependencies

This module is a prerequisite for:

```
Distributed Algorithm Theory
  ├── Understanding synchronizers is foundational for:
  │   ├── Consensus protocols (Paxos, Raft)
  │   ├── Byzantine agreement
  │   ├── Self-stabilizing systems
  │   └── Wireless sensor networks
  │
Complexity Theory
  ├── Communication complexity
  ├── Distributed computing lower bounds
  └── Message complexity as a complexity measure

Practical Systems
  ├── Apache ZooKeeper (synchronization service)
  ├── Google Spanner (TrueTime distributed clock)
  ├── Distributed databases (2PC, Paxos-based)
  └── Blockchain consensus (Proof-of-Work round simulation)
```

## L9 Research Frontiers

| Topic | Description | Status |
|-------|-------------|--------|
| Quantum distributed synchronizers | Using quantum entanglement for synchronization with fewer messages | Documented |
| Energy-efficient synchronizers | Battery-powered IoT devices minimizing sync overhead | Documented |
| Dynamic network synchronizers | Networks with churn (nodes joining/leaving) | Documented |
| Byzantine fault-tolerant synchronizers | Resisting malicious nodes during sync | Documented |

## Learning Path

1. Start with `network.c` → understand graph model and message passing
2. Read `alpha_synchronizer.c` → simplest synchronizer, O(|E|) messages
3. Read `beta_synchronizer.c` → tree-based, O(|V|) messages, tree construction
4. Read `gamma_synchronizer.c` → combines both, cluster-based balancing
5. Read `distributed_algorithms.c` → BFS, leader election, consensus on synchronizers
6. Run `make test` → 30 test cases demonstrating correctness and complexity bounds
7. Study examples → end-to-end demonstrations of each synchronizer type
