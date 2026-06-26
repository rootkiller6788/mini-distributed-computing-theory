# Course Alignment — mini-paxos-raft-formal

This module aligns with the distributed systems and formal methods curricula from nine world-class institutions.

| School | Course | Key Topics Covered |
|--------|--------|-------------------|
| **MIT** | 6.824 Distributed Systems (now 6.5840) | Raft, Paxos, FLP impossibility, state machine replication |
| **Stanford** | CS244b Distributed Systems | Consensus algorithms, Byzantine fault tolerance |
| **Berkeley** | CS262a Advanced Topics in Computer Systems | Raft protocol, log replication, membership changes |
| **CMU** | 15-440 Distributed Systems | Two-phase commit, Paxos, distributed consensus |
| **Princeton** | COS 418 Distributed Systems | Raft, leader election, safety proofs |
| **Caltech** | CS 142 Distributed Computing | FLP theorem, consensus impossibility, formal models |
| **Cambridge** | Part II Distributed Systems | Paxos protocol, Byzantine generals, quorum systems |
| **Oxford** | Distributed Systems | Consensus algorithms, TLA+ formal specification |
| **ETH** | 263-3800 Distributed Computing | Model checking, TLA+, formal verification of protocols |

## Reference Papers Implemented

| Paper | Implementation |
|-------|---------------|
| Lamport, "The Part-Time Parliament" (1998) | `paxos_basic.c` |
| Lamport, "Paxos Made Simple" (2001) | `paxos_basic.c`, `safety_proof.c` |
| Ongaro & Ousterhout, "In Search of an Understandable Consensus Algorithm" (2014) | `raft_leader_election.c`, `raft_log_replication.c` |
| Fischer, Lynch, Paterson, "Impossibility of Distributed Consensus" (1985) | `flp_impossibility.c` |
| Lamport, "Specifying Systems" (2002) | `tla_model.c`, `tla_encoding.h` |
| Castro & Liskov, "Practical Byzantine Fault Tolerance" (1999) | `byzantine_paxos.c` |
| Lamport, "Byzantizing Paxos by Refinement" (2011) | `byzantine_paxos.c` |
| Malkhi & Reiter, "Byzantine Quorum Systems" (1998) | `quorum_system.c` |
