# Knowledge Graph — mini-byzantine-agreement-lower-bound

## L1: Definitions

| # | Definition | Type | Location |
|---|-----------|------|----------|
| 1 | Byzantine failure | `ba_fault_type_t` enum | `include/byzantine_agreement.h` |
| 2 | Oral vs Signed message model | `ba_message_model_t` enum | `include/byzantine_agreement.h` |
| 3 | Synchronous/Asynchronous/Partial-sync | `ba_sync_model_t` enum | `include/byzantine_agreement.h` |
| 4 | Byzantine agreement properties (Agreement, Validity, Termination) | `ba_properties_t` struct | `include/byzantine_agreement.h` |
| 5 | f-resilient system | `ba_system_t` struct | `include/byzantine_agreement.h` |
| 6 | Process state and message | `ba_process_t`, `ba_message_t` | `include/byzantine_agreement.h` |
| 7 | Consensus (Agreement, Validity, Termination) | `Agreement`, `Validity`, `Termination` (Prop) | `src/byzantine_agreement.lean` |
| 8 | FaultType (Correct/Byzantine) | `FaultType` inductive | `src/byzantine_agreement.lean` |
| 9 | Configuration and Decision | `SystemConfig`, `Decision` | `src/byzantine_agreement.lean` |
| 10 | ByzantineAgreement predicate | `ByzantineAgreement` (Prop) | `src/byzantine_agreement.lean` |

## L2: Core Concepts

| # | Concept | Implementation | Location |
|---|---------|---------------|----------|
| 1 | Impossibility result (f < N/3) | `ba_is_achievable()` | `src/byzantine_agreement_core.c` |
| 2 | Consensus family taxonomy | `consensus_family_t` enum | `include/consensus_types.h` |
| 3 | Fault tolerance thresholds | `consensus_get_threshold()` | `src/consensus_types.c` |
| 4 | Protocol reductions | `consensus_has_reduction()` | `src/consensus_types.c` |
| 5 | Synchrony assumptions | `timing_model_t` enum | `include/consensus_types.h` |
| 6 | Failure detectors | `failure_detector_t` struct | `include/consensus_types.h` |
| 7 | CAP theorem connection | `cap_evaluate()` | `src/consensus_types.c` |
| 8 | Bivalence (FLP) | `Valence` inductive | `src/byzantine_agreement.lean` |
| 9 | Quorum intersection property | `QuorumIntersectionProperty` | `src/byzantine_agreement.lean` |
| 10 | FLP bivalent initial config | `flp_bivalent_initial_exists` | `src/byzantine_agreement.lean` |

## L3: Mathematical Structures

| # | Structure | Implementation | Location |
|---|-----------|---------------|----------|
| 1 | Message automaton tuple (Q, q0, Σ_in, Σ_out, δ, λ) | `msg_automaton_t` struct | `include/msg_automaton.h` |
| 2 | Automaton configuration | `automaton_config_t` struct | `include/msg_automaton.h` |
| 3 | Automaton execution trace | `automaton_execution_t` struct | `include/msg_automaton.h` |
| 4 | EIG tree (N-ary tree) | `eig_tree_t`, `eig_node_t` | `include/eig_algorithm.h` |
| 5 | Indistinguishability relation | `Indistinguishable` (Prop) | `src/byzantine_agreement.lean` |
| 6 | Message history filtering | `History.filterFor` | `src/byzantine_agreement.lean` |
| 7 | Signed message chain | `SignedMessage` structure | `src/byzantine_agreement.lean` |
| 8 | Quorum (Finset PID) | `Quorum N` abbreviation | `src/byzantine_agreement.lean` |

## L4: Fundamental Laws

| # | Theorem | Statement | Location |
|---|---------|-----------|----------|
| 1 | LSP82 Theorem 1 (N=3, f=1 impossibility) | No protocol achieves BA with N=3, f=1 oral | `src/byzantine_agreement_core.c`, `src/byzantine_agreement.lean` |
| 2 | f < N/3 lower bound (oral messages) | BA possible iff f < N/3 | `ba_is_achievable()` |
| 3 | FLP impossibility (asynchronous) | Deterministic consensus impossible with 1 crash | `src/byzantine_agreement_core.c` |
| 4 | Dolev-Strong bound (signed) | f < N/2 required with strong validity | `src/byzantine_agreement_core.c` |
| 5 | Simulation argument | Extends N=3 to general N | `lsp82_simulation_argument()` |
| 6 | Quorum intersection theorem | 2f+1 quorums intersect when f<N/3 | `quorum_intersection_2f1` in Lean |
| 7 | Dolev-Reischuk message bound | Ω(N²) messages required | `message_complexity_lower_bound` in Lean |
| 8 | Indistinguishability → same decision | Key lemma for impossibility proofs | `indistinguishable_implies_same_decision` in Lean |

## L5: Algorithms/Methods

| # | Algorithm | Implementation | Location |
|---|-----------|---------------|----------|
| 1 | EIG (Exponential Information Gathering) | `eig_tree_*()`, `eig_run_protocol()` | `src/eig_implementation.c` |
| 2 | Recursive majority rule | `eig_majority()`, `eig_tree_resolve()` | `src/eig_implementation.c` |
| 3 | Phase King algorithm | `phase_king_*()` | `src/phase_protocols.c` |
| 4 | Ben-Or randomized consensus | `benor_*()` | `src/phase_protocols.c` |
| 5 | Dolev-Strong authenticated algorithm | `ds_*()` | `src/phase_protocols.c` |
| 6 | Paxos (crash-stop consensus) | `paxos_*()` | `src/consensus_types.c` |
| 7 | PBFT (Practical BFT) | `pbft_*()` | `src/consensus_types.c` |
| 8 | Failure detector implementation | `failure_detector_*()` | `src/consensus_types.c` |
| 9 | Automaton step simulation | `automaton_step()` | `src/msg_automaton.c` |
| 10 | Bivalence classification | `automaton_classify_valence()` | `src/msg_automaton.c` |

## L6: Canonical Problems

| # | Problem | Example | Location |
|---|---------|---------|----------|
| 1 | N=3, f=1 impossibility | 3-process counterexample | `examples/example_n3_f1_impossible.c` |
| 2 | N=4, f=1 EIG success | 4-process EIG with 1 Byzantine | `examples/example_eig_n4_f1.c` |
| 3 | Blockchain consensus analysis | Nakamoto, Tendermint, HTLC | `examples/example_blockchain_consensus.c` |
| 4 | EIG canonical N=4,f=1 | `eig_canonical_n4_f1_demo()` | `src/eig_implementation.c` |
| 5 | EIG threshold N=7,f=2 | `eig_canonical_n7_f2_validate()` | `src/eig_implementation.c` |

## L7: Applications

| # | Application | Implementation | Location |
|---|-------------|---------------|----------|
| 1 | CAP theorem analysis | `cap_evaluate()` | `src/consensus_types.c` |
| 2 | Nakamoto consensus (Bitcoin) | `nakamoto_attack_success_probability()` | `src/blockchain_consensus.c` |
| 3 | Tendermint consensus (Cosmos) | `tendermint_*()` | `src/blockchain_consensus.c` |
| 4 | HTLC atomic swaps | `htlc_atomic_swap_execute()` | `src/blockchain_consensus.c` |
| 5 | Gasper finality gadget (Ethereum 2.0) | `gasper_*()` | `src/blockchain_consensus.c` |

## L8: Advanced Topics

| # | Topic | Implementation | Location |
|---|-------|---------------|----------|
| 1 | Economic vs mathematical finality | `finality_analyze()` | `src/blockchain_consensus.c` |
| 2 | VRF-based leader election (Algorand) | `vrf_leader_election()` | `src/blockchain_consensus.c` |
| 3 | Quantum Byzantine agreement bound | `quantum_byzantine_bound` theorem | `src/byzantine_agreement.lean` |
| 4 | Hybrid consensus (Nakamoto + BFT) | `consensus_feasibility_analysis()` | `src/blockchain_consensus.c` |
| 5 | Accountable safety (Casper FFG) | `gasper_accountable_safety_bound()` | `src/blockchain_consensus.c` |

## L9: Research Frontiers

| # | Topic | Status | Location |
|---|-------|--------|----------|
| 1 | Post-quantum Byzantine agreement | Documented | `docs/gap-report.md` |
| 2 | Blockchain scalability trilemma | Connected to CAP | `docs/course-alignment.md` |
| 3 | Meta-consensus (consensus about consensus) | Future work | `docs/gap-report.md` |
| 4 | Asynchronous BFT without randomness | FLP-hard → documented | `docs/course-alignment.md` |
