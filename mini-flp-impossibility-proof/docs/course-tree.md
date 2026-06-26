# Course Tree — mini-flp-impossibility-proof

## Prerequisites
```
mini-flp-impossibility-proof
├── mini-consensus-fault-tolerance (sister module)
├── Distributed systems fundamentals
│   ├── Synchronous vs asynchronous models
│   ├── Message passing
│   └── Fault models (crash, Byzantine)
├── Mathematical logic
│   ├── Proof by contradiction
│   └── Lemma-theorem structure
└── Graph theory
    ├── BFS/DFS
    └── Configuration graphs
```

## Dependents
```
mini-flp-impossibility-proof
├── mini-paxos-raft-formal
│   └── FLP impossibility motivates Paxos
├── mini-byzantine-agreement-lower-bound
│   └── Extends FLP to Byzantine model
├── mini-synchronizer-alpha-beta-gamma
│   └── Synchronizers circumvent FLP
└── mini-self-stabilization-dijkstra
    └── Another impossibility-circumvention approach
```

## Knowledge Flow
```
Distributed System Model
  → Consensus Problem Definition
    → FLP Impossibility Theorem
      → Lemma 1 (Commutativity)
      → Lemma 2 (Initial Bivalency)
      → Lemma 3 (Bivalence Preservation)
        → Infinite Non-Deciding Run
          → FLP Theorem Proven
            → Failure Detectors (Chandra-Toueg)
            → Randomization (Ben-Or)
            → Partial Synchrony (Dwork-Lynch-Stockmeyer)
```
