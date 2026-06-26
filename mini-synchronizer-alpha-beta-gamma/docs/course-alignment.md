# Course Alignment — Nine-School Curriculum Mapping

## MIT — 6.046/18.410 Design and Analysis of Algorithms

| Chapter | Topic | This Module |
|---------|-------|-------------|
| Distributed Algorithms | Synchronizers (Awerbuch) | α, β, γ full implementation |
| Message Complexity | O(E) vs O(V) tradeoff | `synchro_print_complexity_report()` |
| Leader Election | LeLann-Chang-Roberts | `leader_election_le_lann_round()` |
| BFS/Spanning Tree | Distributed BFS | `bfs_protocol_round()` |

## MIT — 6.841 Advanced Complexity Theory

| Topic | Coverage |
|-------|----------|
| Communication complexity in distributed computing | Message count tracking |
| Lower bounds for synchronization | Documented in knowledge-graph.md L8 |

## Stanford — CS254 Computational Complexity

| Topic | This Module |
|-------|-------------|
| Network complexity | α/β/γ as complexity tradeoff examples |
| Message lower bounds | Theoretical bounds documented |
| Simulation arguments | Synchronous-on-asynchronous simulation |

## Berkeley — CS278 Computational Complexity

| Topic | This Module |
|-------|-------------|
| Distributed computing theory | Full synchronizer framework |
| Self-stabilization | `self_stabilizing_synchro_round()` |
| Fault tolerance | Crash fault model, consensus |

## CMU — 15-855 Graduate Complexity

| Topic | This Module |
|-------|-------------|
| Synchronizers in distributed algorithms | Core module topic |
| GHS Minimum Spanning Tree | `mst_ghs_protocol_round()` |
| Distributed consensus | `consensus_flood_set_round()` |

## Princeton — COS 522 Computational Complexity

| Topic | This Module |
|-------|-------------|
| Cryptography & distributed computing | Application in sensor networks |
| Secure multi-party computation | Documented extension |

## Caltech — CS 154 Limits of Computation

| Topic | This Module |
|-------|-------------|
| Distributed systems theory | Synchronizer formal model |
| Impossibility results | FLP impossibility context documented |

## Cambridge — Part II Complexity Theory

| Topic | This Module |
|-------|-------------|
| Asynchronous computation | Full async model implementation |
| Synchronous simulation | All three Awerbuch synchronizers |

## Oxford — Advanced Complexity Theory

| Topic | This Module |
|-------|-------------|
| Distributed algorithms complexity | Message/time tradeoff analysis |
| Quantum distributed computing | Documented in L9 |

## ETH — 252-0400 Logic & Computation

| Topic | This Module |
|-------|-------------|
| Formal verification of distributed protocols | 30 test assertions covering correctness |
| State machine models | `node_state_t` with formal state transitions |
