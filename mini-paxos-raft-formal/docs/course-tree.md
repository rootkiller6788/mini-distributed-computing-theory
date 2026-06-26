# Course Tree — mini-paxos-raft-formal

## Prerequisites (Dependencies)

```
mini-complexity-foundations/
  └─ Definitions: P, NP, reduction, Turing machines (L1-L3)
  └─ No direct dependency on specific sub-modules

mini-paxos-raft-formal/
  ├─ Depends on: basic discrete math (set theory, combinatorics)
  ├─ Depends on: basic logic (predicates, induction)
  └─ Self-contained within distributed computing theory
```

## Knowledge Dependency Graph

```
Set Theory & Combinatorics
  ├─ Quorum Systems (intersecting set families)
  │   └─ Majority Quorum Lemma
  │
  └─ Graph Theory
      └─ State Space Exploration (graph traversal)

Logic & Proof Theory
  ├─ Predicate Logic
  │   └─ Invariant Formulation (state predicates)
  │
  ├─ Induction
  │   └─ Inductive Invariant Proofs
  │       ├─ Paxos Safety (ballot induction)
  │       └─ Raft Safety (AppendEntries induction)
  │
  └─ Temporal Logic
      └─ TLA+ Specifications

Distributed Systems
  ├─ System Models
  │   ├─ Synchronous (known bounds)
  │   ├─ Asynchronous (FLP impossibility)
  │   └─ Partially synchronous (GST model)
  │
  ├─ Fault Models
  │   ├─ Crash-stop (Paxos, Raft)
  │   └─ Byzantine (Byzantine Paxos, PBFT)
  │
  └─ Consensus Protocols
      ├─ Paxos (Lamport)
      │   ├─ Basic Paxos (Synod)
      │   └─ Multi-Paxos
      │
      └─ Raft (Ongaro & Ousterhout)
          ├─ Leader Election
          ├─ Log Replication
          └─ Safety (4 properties)

Formal Methods
  └─ Model Checking
      ├─ State Space Exploration (BFS)
      ├─ Invariant Checking
      └─ Counterexample Generation
```

## Forward Dependencies (What depends on this module)

```
mini-paxos-raft-formal/
  └─ Provides foundation for:
      ├─ Distributed databases (Spanner, CockroachDB)
      ├─ Coordination services (etcd, ZooKeeper, Consul)
      ├─ Blockchain consensus (Tendermint, HotStuff)
      └─ Verified distributed systems (IronFleet, Verdi)
```
