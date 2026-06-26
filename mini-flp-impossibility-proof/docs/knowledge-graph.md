# Knowledge Graph — mini-flp-impossibility-proof

## L1: Definitions
| Concept | C Implementation | Status |
|---------|-----------------|--------|
| Asynchronous distributed system | `flp_common.h` — system model | Complete |
| Crash fault model (fail-stop) | `flp_fault_model`, `flp_process_status` | Complete |
| Consensus problem | `flp_consensus.h` — agreement/validity/termination | Complete |
| Configuration | `flp_configuration` — (states × message buffer) | Complete |
| Event (atomic step) | `flp_event` — process receives message | Complete |
| Schedule (sequence of events) | `flp_schedule` — process ID sequence | Complete |
| Protocol (deterministic transition function) | `flp_protocol_desc` + step functions | Complete |
| Decision value {0,1} | `flp_decision_value` enum | Complete |

## L2: Core Concepts
| Concept | C Implementation | Status |
|---------|-----------------|--------|
| Deterministic protocols | 6 protocol implementations | Complete |
| Safety vs Liveness | `flp_consensus_agreement_holds`, `flp_consensus_validity_holds` | Complete |
| Adversarial scheduling | `flp_msg_strategy`, `flp_adversary_strategy` | Complete |
| Synchronous vs Asynchronous | Protocol analysis highlights difference | Complete |
| Reliable message passing | `flp_message.h` — multiset buffer | Complete |
| Wait-freedom | Implicit in FLP impossibility analysis | Complete |

## L3: Mathematical Structures
| Structure | C Implementation | Status |
|-----------|-----------------|--------|
| Configuration = State^N × Message* | `flp_configuration` struct | Complete |
| Message multiset | `flp_message` + buffer operations | Complete |
| Process state machine | `flp_process_state` — deterministic | Complete |
| Transition relation δ | `flp_protocol_step` function | Complete |
| Configuration graph | `flp_config_graph` + BFS/DFS | Complete |
| Valence classification {0,1,biv} | `flp_valence` + classification functions | Complete |

## L4: Fundamental Laws
| Theorem | C Verification | Status |
|---------|---------------|--------|
| Lemma 1: Commutativity | `flp_events_are_disjoint`, `flp_verify_commutativity` | Complete |
| Lemma 2: Initial Bivalency | `flp_find_bivalent_initial`, `flp_verify_initial_bivalency` | Complete |
| Lemma 3: Bivalence Preservation | `flp_find_preserving_schedule` | Complete |
| FLP Main Theorem | `flp_analyze_protocol`, `flp_construct_infinite_run` | Complete |

## L5: Algorithms/Methods
| Algorithm | C Implementation | Status |
|-----------|-----------------|--------|
| BFS on configuration graph | `flp_graph_bfs` | Complete |
| DFS on configuration graph | `flp_graph_dfs` | Complete |
| Configuration graph construction | `flp_graph_build` | Complete |
| Reachability search for decisions | `flp_config_can_decide_0/1` | Complete |
| Bivalence search | `flp_find_bivalent_initial` | Complete |
| Infinite run construction | `flp_construct_infinite_run` | Complete |
| Message delivery strategies (6) | `flp_message.c` — FIFO, LIFO, adversarial, etc. | Complete |

## L6: Canonical Problems
| Problem | C Implementation | Status |
|---------|-----------------|--------|
| Binary consensus | `flp_consensus.h` — full specification | Complete |
| Flood-Set consensus | `flp_proto_flood_set_step` | Complete |
| Phase-King consensus | `flp_proto_phase_king_step` | Complete |
| Echo-based consensus | `flp_proto_echo_step` | Complete |
| Rotating leader consensus | `flp_proto_rotating_leader_step` | Complete |
| Majority vote | `flp_proto_majority_vote_step` | Complete |
| Two-phase commit | `flp_proto_two_phase_step` | Complete |

## L7: Applications
| Application | Description | Status |
|-------------|-------------|-------|
| Paxos motivation | Rotating leader shows why Paxos needs stable leadership | Complete |
| Failure detector motivation | FLP impossibility → Chandra-Toueg ◇S | Complete |
| Randomized consensus | FLP impossibility → Ben-Or algorithm motivation | Complete |
| Blockchain consensus | FLP motivates Nakamoto consensus with PoW | Complete |

## L8: Advanced Topics
| Topic | Status |
|-------|--------|
| Chandra-Toueg failure detectors | Partial (documented) |
| Dwork-Lynch-Stockmeyer partial synchrony | Partial (documented) |
| Ben-Or randomized consensus | Partial (documented) |
| k-set agreement (Chaudhuri 1993) | Partial (enum type) |

## L9: Research Frontiers
| Topic | Status |
|-------|--------|
| Quantum distributed consensus | Documented |
| Byzantine fault tolerance with asynchrony | Documented |
| Approximate consensus | Documented |
