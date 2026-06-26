# Course Tree — Prerequisite Dependencies

## Prerequisites for this module

```
Graph Theory (basic)
  └── Graph representations (adjacency matrix, edge lists)
      └── LOCAL Model + Port Numbering  ← THIS MODULE
          ├── Breadth-First Search (BFS)
          ├── Greedy algorithms
          ├── Probability (for randomized algorithms)
          └── Ramsey Theory (for lower bounds)
```

## What this module enables

```
LOCAL Model + Port Numbering
  ├── CONGEST Model (bandwidth-limited LOCAL)
  ├── Distributed Graph Algorithms (advanced)
  ├── Byzantine Agreement / Consensus
  ├── Self-Stabilizing Algorithms
  ├── SLOCAL Model (research frontier, L9)
  └── Quantum Distributed Computing (research frontier, L9)
```

## Internal dependency tree

```
L1: LOCAL model definition
  └── L2: Synchronous execution, neighborhoods
      ├── L3: Graph structures, port numbering on specific topologies
      │   └── L4: Lower bounds via neighborhood indistinguishability
      │       ├── L5: Algorithms (Cole-Vishkin, Luby, BFS)
      │       │   └── L6: Canonical problems (coloring, MIS, matching)
      │       │       ├── L7: Applications (leader election, verification)
      │       │       └── L8: Advanced (covering graphs, Ramsey)
      │       └── L8: Speedup simulation, round elimination
      └── L9: Research frontiers
```
