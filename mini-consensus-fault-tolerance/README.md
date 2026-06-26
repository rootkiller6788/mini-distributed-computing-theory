# mini-consensus-fault-tolerance

Consensus and Fault Tolerance in Distributed Computing Theory.

## Module Status: COMPLETE

- L1-L6: Complete  
- L7: Complete (5 applications)  
- L8: Complete (6 advanced topics)  
- L9: Partial (documented, not implemented)  

## Nine-Layer Knowledge Coverage

| Level | Status | Key Items |
|-------|--------|-----------|
| L1 Definitions | Complete | 19: Byzantine, crash, consensus, quorum, view, log, block |
| L2 Core Concepts | Complete | 14: synchronous/async/partial-sync, FLP, quorum intersection, longest chain |
| L3 Math Structures | Complete | 16: quorum systems, replicated log, protocol state machines |
| L4 Fundamental Laws | Complete | 12: FLP, n>2f, n>3f, Paxos safety, Raft completeness, PBFT safety |
| L5 Algorithms | Complete | 17: Paxos, Multi-Paxos, Raft, PBFT, OM(m), SM(m), Nakamoto PoW |
| L6 Canonical Problems | Complete | 5: Byzantine Generals, Consensus, Log Replication, Blockchain |
| L7 Applications | Complete | 5: Bitcoin, distributed DB, cloud coordination, BFT blockchain |
| L8 Advanced Topics | Complete | 6: FLP analysis, selfish mining, B-Masking/FPP quorums, failure detectors |
| L9 Research Frontiers | Partial | 4 documented |

## Core Definitions

- **Consensus**: Agreement among distributed processes on a single value
- **Fault Tolerance**: System correctness despite component failures
- **Crash Fault**: Process halts permanently (fail-stop)
- **Byzantine Fault**: Arbitrary deviation from protocol (Lamport et al. 1982)
- **FLP Impossibility**: Deterministic consensus impossible in async model with >=1 crash (Fischer, Lynch, Paterson 1985)
- **Quorum**: Subset of processes whose intersection guarantees consistency
- **State Machine Replication**: Deterministic state machine replicated via ordered log (Schneider 1990)

## Core Theorems

| Theorem | Statement | Code |
|---------|-----------|------|
| FLP Impossibility (1985) | Async + crash + deterministic => impossible | flp_audit_run() |
| Byzantine bound | n > 3f needed for Byzantine consensus | BYZANTINE_TOLERANCE_BOUND |
| Crash bound | n > 2f needed for crash consensus | CRASH_TOLERANCE_BOUND |
| Paxos Safety | Chosen value is unique | paxos_verify_safety() |
| Raft Leader Completeness | Committed entries survive leader changes | raft_verify_leader_completeness() |
| PBFT Safety | 2f+1 quorum intersection prevents conflicts | pbft_verify_safety() |
| Oral Messages bound | n > 3m for m traitors (Lamport et al.) | oral_message_om() |
| Signed Messages bound | n > m with digital signatures (Dolev & Strong) | signed_message_agreement() |
| Nakamoto security | P(reorg) ~ (q/p)^z | nakamoto_reorg_probability() |

## Core Algorithms

1. **Basic Paxos** (Lamport 1998): Two-phase consensus with majority acceptors
2. **Multi-Paxos**: Amortized Phase 1 via distinguished leader
3. **Raft** (Ongaro & Ousterhout 2014): Leader election + log replication + commitment
4. **PBFT** (Castro & Liskov 1999): Three-phase (pre-prepare, prepare, commit) with 2f+1 quorums
5. **OM(m)** (Lamport et al. 1982): Recursive oral messages for Byzantine agreement
6. **SM(m)** (Dolev & Strong 1983): Signed messages requiring n > m
7. **Nakamoto PoW** (2008): Proof-of-work with longest-chain rule
8. **Selfish mining** (Eyal & Sirer 2014): Strategic block withholding analysis

## Nine-School Course Mapping

| School | Course | Key Alignment |
|--------|--------|---------------|
| MIT | 6.824 | Raft, BFT, Bitcoin |
| Stanford | CS244B | Paxos, FLP, PBFT |
| CMU | 15-440 | Raft, Bitcoin, Quorums |
| Princeton | COS 518 | Consensus theory, BFT, Blockchain |
| Berkeley | CS 262B | Paxos, FLP, Byzantine |
| Cambridge | Part II | Paxos/Raft, Byzantine, Blockchain |
| Oxford | Advanced DC | Paxos, PBFT, Nakamoto |
| ETH | 263-3850 | Quorums, Impossibility, BFT |
| Caltech | CS 142 | Fault tolerance, SMR, Paxos |

## File Structure

```
mini-consensus-fault-tolerance/
  Makefile                         # make test, make examples
  README.md                        # This file
  include/                         # 7 header files
    consensus_types.h              # Core types, fault models
    consensus_model.h              # System models, FLP audit
    paxos.h                        # Paxos algorithm API
    raft.h                         # Raft algorithm API
    pbft.h                         # PBFT algorithm API
    quorum.h                       # Quorum system API
    nakamoto.h                     # Nakamoto consensus API
  src/                             # 8 source files
    consensus_types.c              # Core type implementations
    consensus_model.c              # System model implementations
    paxos.c                        # Paxos implementation (452 lines)
    raft.c                         # Raft implementation (338 lines)
    pbft.c                         # PBFT implementation (269 lines)
    quorum.c                       # Quorum system implementation (250 lines)
    nakamoto.c                     # Nakamoto consensus (340 lines)
    byzantine_agreement.c          # OM/SM algorithms (352 lines)
  tests/
    test_consensus.c               # 25 assert-based tests
  examples/
    example_paxos.c                # End-to-end Paxos demo
    example_raft.c                 # End-to-end Raft cluster
    example_nakamoto.c             # Blockchain simulation
  docs/
    knowledge-graph.md             # L1-L9 knowledge map
    coverage-report.md             # Coverage assessment
    gap-report.md                  # Remaining gaps
    course-alignment.md            # Nine-school mapping
    course-tree.md                 # Prerequisite tree
```

## Build and Test

```bash
make          # Build library, test, and examples
make test     # Run 25 assert-based tests
make examples # Build example binaries
make clean    # Remove build artifacts
```

## Line Count

include/ + src/ = 3250 lines (exceeds 3000 minimum per SKILL.md)

## References

1. Lamport, Shostak, Pease (1982) - The Byzantine Generals Problem
2. Fischer, Lynch, Paterson (1985) - Impossibility of Distributed Consensus
3. Dwork, Lynch, Stockmeyer (1988) - Consensus in the Presence of Partial Synchrony
4. Lamport (1998) - The Part-Time Parliament
5. Lamport (2001) - Paxos Made Simple
6. Castro & Liskov (1999) - Practical Byzantine Fault Tolerance
7. Ongaro & Ousterhout (2014) - In Search of an Understandable Consensus Algorithm
8. Nakamoto (2008) - Bitcoin: A Peer-to-Peer Electronic Cash System
9. Malkhi & Reiter (1998) - Byzantine Quorum Systems
10. Eyal & Sirer (2014) - Majority is not Enough: Bitcoin Mining is Vulnerable
