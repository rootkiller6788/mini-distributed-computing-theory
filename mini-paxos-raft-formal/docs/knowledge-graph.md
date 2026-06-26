# Knowledge Graph — Paxos/Raft & Formal Verification

## L1: Definitions (Complete)

| # | Term | Definition | C Representation |
|---|------|-----------|-----------------|
| 1 | Consensus Problem | Agreement, Validity, Termination | `consensus_problem_t` |
| 2 | Ballot/Term | Total order for proposal numbers | `ballot_t`, `term_t` |
| 3 | Quorum | Intersecting set system; majority = ⌊n/2⌋+1 | `quorum_t`, `quorum_system_t` |
| 4 | Proposer | Initiates consensus in Paxos | `paxos_proposer_t` |
| 5 | Acceptor | Votes on proposals in Paxos | `paxos_acceptor_t` |
| 6 | Learner | Detects chosen values in Paxos | `paxos_learner_t` |
| 7 | Log Entry | Indexed command in Raft log | `raft_log_entry_t` |
| 8 | Raft Server State | Follower/Candidate/Leader | `raft_server_t` |
| 9 | RequestVote RPC | Raft election RPC | `raft_request_vote_args_t` |
| 10 | AppendEntries RPC | Raft replication RPC | `raft_append_entries_args_t` |
| 11 | TLA+ State | Assignment of values to variables | `tla_state_t` |
| 12 | Byzantine Fault | Arbitrary (malicious) failure | `fault_model_t` (FAULT_BYZANTINE) |
| 13 | Inductive Invariant | Predicate I: Init ⇒ I, I ∧ Next ⇒ I' | `inductive_invariant_t` |
| 14 | Configuration (FLP) | Global state of all processes + messages | `flp_configuration_t` |
| 15 | Valence (FLP) | 0-valent / 1-valent / bivalent | `valence_t` |

## L2: Core Concepts (Complete)

| # | Concept | Implementation |
|---|---------|---------------|
| 1 | Two-phase commit (Paxos) | Phase 1a/1b + Phase 2a/2b | `paxos_prepare`, `paxos_accept` |
| 2 | Leader election (Raft) | Randomized timeouts, majority vote | `raft_become_candidate`, `raft_tick` |
| 3 | Log replication (Raft) | AppendEntries with consistency check | `raft_handle_append_entries` |
| 4 | Safety vs Liveness | Safety: always; Liveness: partial synchrony | `invariant_checker.c` |
| 5 | Asynchronous system model | No bounds on message delay/process speed | `MODEL_ASYNCHRONOUS` |
| 6 | Partial synchrony | Eventually synchronous (GST) | `partial_synchrony_model_t` |
| 7 | Crash-stop vs Byzantine | Fault model classification | `fault_model_t` |
| 8 | Majority quorum | n/2+1 nodes | `quorum_is_valid` |
| 9 | State machine replication (SMR) | Consensus → replicated log → deterministic state machine | `kv_store_example.c` |
| 10 | Leader-based consensus | Distinguished leader for each term | `raft_become_leader` |

## L3: Mathematical Structures (Complete)

| # | Structure | Implementation |
|---|-----------|---------------|
| 1 | Quorum system (intersecting set family) | `quorum_generate_all`, `quorum_intersection_holds` |
| 2 | Total order (ballots/terms) | `ballot_t`, `term_t` with comparison |
| 3 | Log as sequence | `raft_log_entry_t` array |
| 4 | TLA+ action formulas | `tla_action_t`, `tla_specification_t` |
| 5 | Transition system | `system_state_t`, `transition_relation_t` |
| 6 | Inductive invariant structure | `inductive_invariant_t` |
| 7 | State hash (for model checking) | `tla_state_hash`, `tla_state_equal` |
| 8 | Byzantine quorum system (3f+1) | `byzantine_quorum_size`, `byzantine_total_nodes` |

## L4: Fundamental Laws (Complete)

| # | Theorem | Proof/Verification |
|---|---------|-------------------|
| 1 | **Paxos Safety**: If v chosen at ballot n, only v proposed at m > n | `paxos_safety_proof_full`, `paxos_verify_safety_invariant` |
| 2 | **FLP Impossibility**: Deterministic consensus impossible in async with ≥1 crash | `flp_is_bivalent_initial`, `flp_non_deciding_run_length` |
| 3 | **Raft Election Safety**: At most one leader per term | `inv_raft_election_safety`, `raft_verify_election_safety` |
| 4 | **Raft Log Matching**: Same index+term ⇒ identical prefixes | `raft_verify_log_matching`, `inv_raft_log_matching` |
| 5 | **Raft Leader Completeness**: Committed entries in all future leaders | `raft_verify_leader_completeness` |
| 6 | **Raft State Machine Safety**: Same index ⇒ same command | `raft_verify_state_machine_safety` |
| 7 | **Quorum Intersection Lemma**: Any two majorities intersect | `safety_intersecting_majorities_lemma` |
| 8 | **Byzantine Bound**: n ≥ 3f+1 for Byzantine consensus | `byzantine_nodes_sufficient` |
| 9 | **Safe Value Propagation**: Intersection ensures value continuity | `safety_value_propagation_lemma` |

## L5: Algorithms/Methods (Complete)

| # | Algorithm | Implementation |
|---|-----------|---------------|
| 1 | Basic Paxos (Synod) | `paxos_basic.c` |
| 2 | Multi-Paxos (leader optimization) | `paxos_multi.c` |
| 3 | Raft leader election | `raft_leader_election.c` |
| 4 | Raft log replication | `raft_log_replication.c` |
| 5 | Raft commit index advancement | `raft_advance_commit_index` |
| 6 | BFS model checking | `tla_model_check` |
| 7 | Inductive invariant checking | `invariant_check_inductive` |
| 8 | Byzantine Paxos Phase 1/2 | `byzantine_paxos_prepare_send/receive` |
| 9 | Raft cluster simulation | `raft_simulate_cluster` |
| 10 | Quorum generation (all combinations) | `quorum_generate_all` |

## L6: Canonical Problems (Complete)

| # | Problem | Example |
|---|---------|---------|
| 1 | Consensus in crash-fault model | `synod_example.c` |
| 2 | Leader election in partial synchrony | `raft_cluster_example.c` |
| 3 | Distributed key-value store (etcd-like) | `kv_store_example.c` |
| 4 | Log consistency | `test_raft.c` — log matching tests |
| 5 | Byzantine agreement | `byzantine_paxos.c` |

## L7: Applications (Complete)

| # | Application | Reference |
|---|-------------|-----------|
| 1 | Google Chubby (Paxos lock service) | `paxos_multi.c` — references Chandra et al. 2007 |
| 2 | Google Spanner (globally-distributed DB) | `paxos_multi.c` — references Corbett et al. 2012 |
| 3 | etcd (Kubernetes) | `kv_store_example.c` — Raft-based KV store |
| 4 | AWS use of TLA+ (S3, DynamoDB) | `tla_encoding.h` — references Newcombe et al. 2015 |
| 5 | ZooKeeper (Apache) | `kv_store_example.c` — referenced in comments |

## L8: Advanced Topics (Complete)

| # | Topic | Implementation |
|---|-------|---------------|
| 1 | Byzantine Paxos (n=3f+1, q=2f+1) | `byzantine_paxos.c` |
| 2 | PBFT comparison | `byzantine_compare_with_pbft` |
| 3 | Cross-fault tolerance (Byzantine + Crash) | `cross_fault_min_nodes`, `cross_fault_quorum_size` |
| 4 | Symmetry reduction in model checking | `tla_canonize_state` |
| 5 | Bounded model checking | `tla_bounded_model_check` |
| 6 | FLP infinite non-deciding run construction | `flp_bivalent_successor` |
| 7 | Inductive invariant strengthening (IC3/PDR) | `invariant_strengthen` |
| 8 | Byzantine node behavior simulation | `byzantine_acceptor_respond` |

## L9: Research Frontiers (Partial)

| # | Topic | Status |
|---|-------|--------|
| 1 | Blockchain consensus (HotStuff, Tendermint) | Documented only |
| 2 | Flexible Paxos (quorum intersection relaxation) | Referenced |
| 3 | Vertical Paxos (reconfiguration) | Named |
| 4 | Verified consensus (Coq/Lean implementations) | TLA+ encoding provided |
