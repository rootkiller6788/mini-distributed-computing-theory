# mini-byzantine-agreement-lower-bound

**Byzantine Agreement Lower Bound — f < N/3 Impossibility Theorem**

Part of `mini-distributed-computing-theory` in the
[mini-theory-of-computation](https://github.com/nano-everything/mini-theory-of-computation)
project.

---

## Module Status: COMPLETE ✅

| Level | Name | Status |
|-------|------|--------|
| L1 | Definitions | **Complete** |
| L2 | Core Concepts | **Complete** |
| L3 | Mathematical Structures | **Complete** |
| L4 | Fundamental Laws | **Complete** |
| L5 | Algorithms/Methods | **Complete** |
| L6 | Canonical Problems | **Complete** |
| L7 | Applications | **Complete** (5 applications) |
| L8 | Advanced Topics | **Complete** (5 topics) |
| L9 | Research Frontiers | **Partial** (documented) |

**Total: 17/18 — COMPLETE**

---

## Core Definitions

- **Byzantine Fault**: A process that exhibits arbitrary (adversarial) behavior
- **Oral Message Model**: Messages cannot be forged but faulty processes can lie about relayed content
- **Signed Message Model**: Messages carry unforgeable signatures
- **Byzantine Agreement**: Agreement + Validity + Termination for all correct processes
- **f-Resilient**: A protocol that tolerates up to f Byzantine faults among N processes

## Core Theorems

### 1. LSP82 Lower Bound (Lamport, Shostak, Pease 1982)

> **Theorem 1**: In the synchronous oral message model with N processes,
> Byzantine agreement is achievable **if and only if** f < N/3.

- **Necessity**: f ≥ N/3 ⇒ impossible (proved via 3-scenario indistinguishability)
- **Sufficiency**: f < N/3 ⇒ achievable via EIG algorithm (f+1 rounds)

### 2. FLP Impossibility (Fischer, Lynch, Paterson 1985)

> **Theorem**: In the asynchronous model, deterministic consensus is
> impossible with even a single crash fault.

- Proof technique: Bivalence + "hooking" to construct infinite non-deciding execution

### 3. Dolev-Strong Upper Bound (1983)

> **Theorem**: With unforgeable digital signatures, Byzantine agreement
> is achievable for any f < N in f+1 rounds.

### 4. Quorum Intersection Property

> For any two quorums of size 2f+1 in a system of N = 3f+1 processes,
> their intersection has size at least f+1 ≥ 1.

### 5. Dolev-Reischuk Message Lower Bound (1985)

> Any deterministic Byzantine agreement protocol requires Ω(N²)
> messages in the worst case.

## Core Algorithms

| Algorithm | Model | Fault Bound | Rounds | Location |
|-----------|-------|-------------|--------|----------|
| EIG (Exponential Information Gathering) | Sync, Oral | f < N/3 | f+1 | `src/eig_implementation.c` |
| Phase King | Sync, Oral | f < N/4 | 2(f+1) | `src/phase_protocols.c` |
| Ben-Or Randomized | Async, Oral | f < N/5 (Byz) | E[O(1)] | `src/phase_protocols.c` |
| Dolev-Strong Signed | Sync, Signed | f < N | f+1 | `src/phase_protocols.c` |
| Paxos | Async, Crash | f < N/2 | 2 | `src/consensus_types.c` |
| PBFT | Sync/Partial | f < N/3 | 3 | `src/consensus_types.c` |

## Classic Problems

1. **N=3, f=1 Impossibility**: The base case of the LSP82 proof
2. **N=4, f=1 EIG Success**: Matching upper bound demonstration
3. **Nakamoto Double-Spend**: Probabilistic Byzantine agreement in Bitcoin
4. **Tendermint Consensus**: BFT in Cosmos blockchain
5. **HTLC Atomic Swaps**: 2-party agreement without consensus

## Nine-School Curriculum Mapping

| School | Key Course | Contribution |
|--------|-----------|-------------|
| **MIT** | 6.852 Distributed Algorithms | EIG algorithm, lower bound proof technique |
| **Stanford** | CS355 Distributed Computing | Circuit-analogy for lower bounds |
| **Berkeley** | CS294 Distributed Computing | FLP and CAP theorem |
| **CMU** | 15-712 Distributed Systems | Paxos/PBFT practical implementations |
| **Princeton** | COS 551 Advanced Complexity | Cryptographic relaxations of bounds |
| **Caltech** | CS 154 Limits of Computation | Quantum/physics perspective |
| **Cambridge** | Part III Advanced Complexity | Indistinguishability proof technique |
| **Oxford** | Advanced Complexity Theory | Quantum Byzantine agreement |
| **ETH** | 263-4650 Advanced Complexity | Formal verification of distributed protocols |

## Quick Start

```bash
# Build and run tests
make test

# Build and run examples
make run-examples

# Clean build artifacts
make clean
```

## Code Structure

```
mini-byzantine-agreement-lower-bound/
├── Makefile
├── README.md                          ← This file
├── include/
│   ├── byzantine_agreement.h           ← Core types and API
│   ├── consensus_types.h              ← Consensus taxonomy
│   ├── eig_algorithm.h                ← EIG tree and algorithm
│   ├── impossibility_proof.h          ← Lower bound proof structures
│   ├── msg_automaton.h               ← Message automaton model
│   └── phase_protocols.h             ← Phase King, Ben-Or, DS
├── src/
│   ├── byzantine_agreement_core.c     ← Core logic + LSP82 proof
│   ├── blockchain_consensus.c         ← L7/L8 applications
│   ├── consensus_types.c              ← Consensus types + Paxos/PBFT
│   ├── eig_implementation.c           ← EIG algorithm full impl
│   ├── msg_automaton.c               ← Automaton simulation
│   ├── phase_protocols.c             ← Phase King, Ben-Or, DS
│   └── byzantine_agreement.lean      ← Lean 4 formalization
├── tests/
│   └── test_byzantine_agreement.c     ← Full test suite
├── examples/
│   ├── example_n3_f1_impossible.c     ← LSP82 proof walkthrough
│   ├── example_eig_n4_f1.c           ← EIG success demonstration
│   └── example_blockchain_consensus.c ← Blockchain connection
├── demos/
├── benches/
└── docs/
    ├── knowledge-graph.md             ← L1-L9 knowledge map
    ├── coverage-report.md             ← Detailed coverage assessment
    ├── gap-report.md                  ← Missing items + priorities
    ├── course-alignment.md            ← Nine-school curriculum map
    └── course-tree.md                 ← Prerequisites + dependencies
```

## References

- Lamport, Shostak, Pease (1982). "The Byzantine Generals Problem." *ACM TOPLAS*.
- Fischer, Lynch, Paterson (1985). "Impossibility of Distributed Consensus with One Faulty Process." *JACM*.
- Dolev, Strong (1983). "Authenticated Algorithms for Byzantine Agreement." *SIAM J. Computing*.
- Lynch (1996). *Distributed Algorithms*. Morgan Kaufmann.
- Castro, Liskov (1999). "Practical Byzantine Fault Tolerance." *OSDI*.
- Nakamoto (2008). "Bitcoin: A Peer-to-Peer Electronic Cash System."
- Buchman (2016). "Tendermint: Byzantine Fault Tolerance in the Age of Blockchains."
- Buterin, Griffith (2017). "Casper the Friendly Finality Gadget."
