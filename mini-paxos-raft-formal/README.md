# mini-paxos-raft-formal — Paxos/Raft Consensus & Formal Verification

**Distributed consensus protocols (Paxos, Raft) with formal TLA+ model checking.**

---

## Module Status: COMPLETE ✅

- **L1-L6**: Complete
- **L7**: Complete (5 applications: Chubby, Spanner, etcd, AWS TLA+, ZooKeeper)
- **L8**: Complete (8 advanced topics: Byzantine Paxos, PBFT, cross-fault, etc.)
- **L9**: Partial (4 topics documented: blockchain consensus, Flexible Paxos, Vertical Paxos, verified consensus)

**Code**: `include/` + `src/` ≥ 5400 lines (threshold: 3000)

---

## Knowledge Coverage Summary

| Level | Name | Status | Items |
|-------|------|--------|-------|
| **L1** | Definitions | ✅ Complete | 15 core definitions |
| **L2** | Core Concepts | ✅ Complete | 10 concepts |
| **L3** | Math Structures | ✅ Complete | 8 structures |
| **L4** | Fundamental Laws | ✅ Complete | 9 theorems |
| **L5** | Algorithms/Methods | ✅ Complete | 10 algorithms |
| **L6** | Canonical Problems | ✅ Complete | 5 problems |
| **L7** | Applications | ✅ Complete | 5 applications |
| **L8** | Advanced Topics | ✅ Complete | 8 topics |
| **L9** | Research Frontiers | ⚠️ Partial | 4 documented |

**Score: 17/18** — COMPLETE

---

## Core Definitions

| Term | Definition |
|------|-----------|
| **Consensus** | Agreement + Validity + Termination over proposed values |
| **Ballot (Paxos)** | Totally ordered proposal number, unique per proposer |
| **Term (Raft)** | Logical clock epoch, incremented on election |
| **Quorum** | Intersecting subset system; majority = ⌊n/2⌋+1 |
| **Log Entry** | (term, command) pair, indexed 1..n |
| **FLP Bivalence** | Configuration from which both 0 and 1 are reachable |

## Core Theorems

| Theorem | Statement | Reference |
|---------|-----------|-----------|
| **Paxos Safety** | If v chosen at ballot n, only v proposed at m > n | Lamport (1998, 2001) |
| **FLP Impossibility** | Deterministic consensus impossible in async with ≥1 crash | Fischer, Lynch, Paterson (1985) |
| **Raft Election Safety** | At most one leader per term | Ongaro & Ousterhout (2014) |
| **Raft Log Matching** | Same index+term ⇒ identical prefixes | Ongaro & Ousterhout (2014) |
| **Raft Leader Completeness** | Committed entries in all future leaders' logs | Ongaro & Ousterhout (2014) |
| **Quorum Intersection** | Any two majorities intersect (pigeonhole principle) | Malkhi & Reiter (1998) |
| **Byzantine Bound** | n ≥ 3f+1 for Byzantine consensus | Lamport et al. (1982) |

## Core Algorithms

| Algorithm | Description |
|-----------|-------------|
| **Basic Paxos** | Two-phase consensus (Prepare/Promise, Accept/Accepted) |
| **Multi-Paxos** | Leader-optimized Paxos for log replication |
| **Raft Leader Election** | Randomized timeout-based leader election |
| **Raft Log Replication** | AppendEntries with consistency check and commit advancement |
| **BFS Model Checking** | State-space exploration for TLA+ invariant verification |
| **Byzantine Paxos** | Paxos with Byzantine fault tolerance (n=3f+1) |

## Nine-School Curriculum Mapping

| School | Key Course | Topics |
|--------|-----------|--------|
| MIT | 6.824/6.5840 Distributed Systems | Raft, Paxos, FLP, SMR |
| Stanford | CS244b Distributed Systems | Consensus, Byzantine faults |
| Berkeley | CS262a Adv. Computer Systems | Raft, log replication |
| CMU | 15-440 Distributed Systems | Two-phase commit, Paxos |
| Princeton | COS 418 Distributed Systems | Raft, safety proofs |
| Caltech | CS 142 Distributed Computing | FLP, formal models |
| Cambridge | Part II Distributed Systems | Paxos, Byzantine generals |
| Oxford | Distributed Systems | Consensus, TLA+ |
| ETH | 263-3800 Distributed Computing | Model checking, TLA+ |

---

## File Structure

```
mini-paxos-raft-formal/
├── Makefile              # make test / make examples / make all
├── README.md             # This file
├── include/              # 6 header files (1270+ lines)
│   ├── paxos_raft_types.h    — Core type definitions
│   ├── consensus_model.h     — Quorum theory, FLP model
│   ├── paxos_core.h          — Paxos algorithm API
│   ├── raft_core.h           — Raft algorithm API
│   ├── tla_encoding.h        — TLA+ specification structures
│   └── invariant_checker.h   — Inductive invariant checking
├── src/                  # 11 C implementation files (4100+ lines)
│   ├── quorum_system.c       — Quorum mathematics
│   ├── paxos_basic.c         — Basic Paxos (Synod)
│   ├── paxos_multi.c         — Multi-Paxos
│   ├── raft_leader_election.c— Raft leader election
│   ├── raft_log_replication.c— Raft log replication
│   ├── raft_simulator.c      — Cluster simulation
│   ├── safety_proof.c        — Safety proof implementations
│   ├── invariant_checker.c   — Invariant checking engine
│   ├── tla_model.c           — TLA+ model checker (BFS)
│   ├── flp_impossibility.c   — FLP theorem constructions
│   └── byzantine_paxos.c     — Byzantine Paxos & PBFT
├── tests/                # 4 test suites
│   ├── test_quorum.c
│   ├── test_paxos.c
│   ├── test_raft.c
│   └── test_safety.c
├── examples/             # 3 end-to-end examples
│   ├── synod_example.c       — Basic Paxos consensus
│   ├── raft_cluster_example.c— Raft cluster simulation
│   └── kv_store_example.c    — KV store (etcd-like)
├── docs/                 # Documentation
│   ├── knowledge-graph.md
│   ├── coverage-report.md
│   ├── gap-report.md
│   ├── course-alignment.md
│   └── course-tree.md
├── demos/
└── benches/
```

---

## Build & Test

```bash
make all       # Build all tests and examples
make test      # Run all test suites
make examples  # Run all examples
make clean     # Remove build artifacts
make lines     # Count lines
```

---

## References

1. Lamport, "The Part-Time Parliament", ACM TOCS (1998)
2. Lamport, "Paxos Made Simple", ACM SIGACT News (2001)
3. Ongaro & Ousterhout, "In Search of an Understandable Consensus Algorithm", USENIX ATC (2014)
4. Fischer, Lynch, Paterson, "Impossibility of Distributed Consensus with One Faulty Process", JACM (1985)
5. Lamport, "Specifying Systems", Addison-Wesley (2002)
6. Castro & Liskov, "Practical Byzantine Fault Tolerance", OSDI (1999)
7. Lamport, "Byzantizing Paxos by Refinement", DISC (2011)
8. Malkhi & Reiter, "Byzantine Quorum Systems", STOC (1998)
9. Newcombe et al., "How Amazon Web Services Uses Formal Methods", CACM (2015)
