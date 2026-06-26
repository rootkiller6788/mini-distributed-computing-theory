# Knowledge Graph: Consensus and Fault Tolerance

## L1: Definitions (Complete)

| Item | C Implementation | Header |
|------|-----------------|--------|
| Byzantine fault | fault_type_t::FAULT_BYZANTINE | consensus_types.h |
| Crash fault | fault_type_t::FAULT_CRASH | consensus_types.h |
| Omission fault | fault_type_t::FAULT_OMISSION | consensus_types.h |
| Timing fault | fault_type_t::FAULT_TIMING | consensus_types.h |
| Consensus (C1-C3) | consensus_property_t | consensus_types.h |
| Safety property | consensus_safety_t | consensus_types.h |
| Liveness property | PROP_TERMINATION | consensus_types.h |
| Quorum | quorum_t, quorum_system_t | quorum.h |
| View | view_state_t | consensus_types.h |
| Leader/Term | raft_server_t fields | raft.h |
| Log entry | log_entry_t | consensus_types.h |
| State machine replication | replicated_log_t | consensus_types.h |
| Byzantine behaviors | byzantine_behavior_t | consensus_types.h |
| Block (Nakamoto) | nakamoto_block_t | nakamoto.h |

## L2: Core Concepts (Complete)

| Concept | Implementation |
|---------|---------------|
| Synchronous model | sync_model_t, MODEL_SYNCHRONOUS |
| Asynchronous model | async_model_t, MODEL_ASYNCHRONOUS |
| Partially synchronous model | partial_sync_model_t |
| FLP impossibility conditions | flp_audit_run() |
| Quorum intersection | quorum_has_intersection() |
| Longest chain rule | nakamoto_compare_chains() |
| Probabilistic finality | nakamoto_reorg_probability() |
| Consensus solvability | consensus_is_solvable() |
| Failure detection | failure_detector_t |

## L3: Mathematical Structures (Complete)

| Structure | Implementation |
|-----------|---------------|
| Fault tolerance bounds | CRASH_TOLERANCE_BOUND, BYZANTINE_TOLERANCE_BOUND |
| Quorum system (majority) | quorum_create_majority() |
| Quorum system (grid) | quorum_create_grid() |
| B-Masking quorum | quorum_create_b_masking() |
| FPP quorum | quorum_create_fpp() |
| Replicated log | replicated_log_append/get/truncate |
| Paxos proposal numbers | paxos_proposal_num_t |
| Raft log | raft_log_t |
| PBFT replica state | pbft_replica_t |

## L4: Fundamental Laws (Complete)

| Theorem | Implementation |
|---------|---------------|
| FLP Impossibility (1985) | flp_impossible(), flp_audit_run() |
| Byzantine bound n > 3f | BYZANTINE_TOLERANCE_BOUND |
| Crash fault bound n > 2f | CRASH_TOLERANCE_BOUND |
| Paxos Safety | paxos_verify_safety() |
| Raft Leader Completeness | raft_verify_leader_completeness() |
| PBFT Safety | pbft_verify_safety() |
| Round lower bound f+1 | sync_min_rounds() |
| Oral Messages n > 3m | oral_message_om() |
| Signed Messages n > m | signed_message_agreement() |
| Nakamoto reorg probability | nakamoto_reorg_probability() |

## L5: Algorithms/Methods (Complete)

| Algorithm | Implementation |
|-----------|---------------|
| Basic Paxos | paxos_run_instance() |
| Multi-Paxos | multi_paxos_init/elect_leader/propose() |
| Raft leader election | raft_start_election() |
| Raft log replication | raft_append_command() |
| PBFT 3-phase | pbft_send_pre_prepare/prepare/commit |
| OM(m) Oral Messages | oral_message_om() |
| Nakamoto PoW mining | nakamoto_mine_block() |
| Quorum read/write | quorum_read/write() |

## L6: Canonical Problems (Complete)

| Problem | Demonstrator |
|---------|-------------|
| Byzantine Generals | byzantine_agreement.c |
| Crash Consensus | example_paxos.c |
| Log Replication | example_raft.c |
| Blockchain | example_nakamoto.c |

## L7: Applications (Complete)

| Application | Implementation |
|-------------|---------------|
| Bitcoin blockchain | nakamoto.c, example_nakamoto.c |
| Distributed database | example_raft.c |
| Cloud coordination | example_paxos.c |
| PBFT blockchain | pbft.c |

## L8: Advanced Topics (Complete)

| Topic | Implementation |
|-------|---------------|
| FLP impossibility proof | flp_audit_run() |
| Selfish mining | nakamoto_selfish_mining_revenue() |
| B-Masking quorums | quorum_create_b_masking() |
| FPP quorums | quorum_create_fpp() |
| Failure detector theory | failure_detector_t |

## L9: Research Frontiers (Partial)

| Topic | Status |
|-------|--------|
| DAG-based consensus | Documented |
| Proof of Stake | Referenced |
| Cross-chain protocols | Documented |
| Dynamic adversary models | Referenced |
