# Knowledge Graph — mini-self-stabilization-dijkstra

## L1: Definitions ✅ Complete

| # | Concept | C Implementation | Lean Formalization |
|---|---------|-----------------|-------------------|
| 1 | Self-stabilization | `ss_system_descriptor_t` (self_stabilization.h) | `SelfStabilizingSystem` |
| 2 | Legitimate configuration | `ring_is_legitimate()` (dijkstra_ring.c) | `legitimate` predicate |
| 3 | Privilege (token) | `privilege_t` enum (dijkstra_ring.h) | `Privilege` inductive |
| 4 | Machine state | `machine_state_t` (dijkstra_ring.h) | `State := Nat` |
| 5 | Ring configuration | `ring_configuration_t` (dijkstra_ring.h) | `RingConfig n k` |
| 6 | Guarded command | `guarded_command_t` (state_machine.h) | — |
| 7 | Daemon (scheduler) | `daemon_type_t` (scheduler.h) | — |
| 8 | Convergence | `ring_converge_to_legitimate()` | `convergence_3state` |
| 9 | Closure | `ring_verify_closure()` | `closure_3state` |
| 10 | Attractor | `attractor_t` (convergence.h) | — |

## L2: Core Concepts ✅ Complete

| # | Concept | Implementation |
|---|---------|---------------|
| 1 | Token passing / mutual exclusion | `ring_step_3state()`, `ring_step_4state()`, `ring_step_kstate()` |
| 2 | Bottom machine (distinguished) | `is_bottom` field in `ring_machine_t` |
| 3 | Symmetry breaking | `ss_lemma_symmetry_breaking()` |
| 4 | Central daemon model | `DAEMON_CENTRAL` in scheduler.c |
| 5 | Distributed daemon model | `DAEMON_DISTRIBUTED` in scheduler.c |
| 6 | Synchronous execution | `ring_synchronous_round()` |
| 7 | Fairness policies | `fairness_policy_t` enum, `scheduler_starvation_counts()` |
| 8 | Fault tolerance | `fault_inject_single()`, `fault_inject_burst()` |
| 9 | Probabilistic scheduling | `scheduler_select_probabilistic()` |

## L3: Mathematical Structures ✅ Complete

| # | Structure | Implementation |
|---|-----------|---------------|
| 1 | Ring topology (Z_N) | `ring_configuration_t` with predecessor relation |
| 2 | State space K^N | `ss_build_transition_system()` |
| 3 | Transition relation → | `ss_transition_t`, `ss_transition_system_t` |
| 4 | Guarded command language | `guarded_command_t` struct |
| 5 | Energy function (Lyapunov) | `convergence_energy_difference_count()` |
| 6 | Quadratic energy | `convergence_energy_squared_sum()` |
| 7 | Token misplacement metric | `convergence_energy_token_misplacement()` |
| 8 | Legitimate set L ⊆ C | `legitimate_set_t`, `ss_enumerate_legitimate_configs()` |

## L4: Fundamental Laws / Theorems ✅ Complete

| # | Theorem | Verification |
|---|---------|-------------|
| 1 | **Dijkstra's Theorem 1** (3-state convergence) | `ss_theorem_dijkstra_3state()` |
| 2 | **Dijkstra's Theorem 2** (K-state convergence) | `ss_theorem_dijkstra_kstate()` |
| 3 | **State Lower Bound** (uniform ring) | `ss_theorem_min_states_uniform()` |
| 4 | **Symmetry-Breaking Lemma** | `ss_lemma_symmetry_breaking()` |
| 5 | **Closure Theorem** | `ss_verify_closure_exhaustive()`, `closure_3state` (Lean) |
| 6 | **Convergence Theorem** | `ss_verify_convergence_exhaustive()`, `convergence_3state` (Lean) |
| 7 | **Energy Monotonicity** | `convergence_verify_monotonicity()`, `energy_non_increasing` (Lean) |
| 8 | **Fair Composition** | `ss_verify_composition()`, `composition_preserves_stabilization` (Lean) |

## L5: Algorithms / Methods ✅ Complete

| # | Algorithm | Implementation |
|---|-----------|---------------|
| 1 | 3-state bottom-machine algorithm | `ring_step_3state()` |
| 2 | 4-state two-distinguished algorithm | `ring_step_4state()` |
| 3 | K-state uniform algorithm (K > N) | `ring_step_kstate()` |
| 4 | Central daemon convergence simulator | `ring_converge_to_legitimate()` |
| 5 | Synchronous round execution | `ring_synchronous_round()` |
| 6 | Exhaustive configuration enumeration | `ss_build_transition_system()` |
| 7 | BFS shortest-path convergence | `ss_max_convergence_steps()` |
| 8 | Monte Carlo convergence trials | `convergence_run_trials()` |

## L6: Canonical Problems ✅ Complete

| # | Problem | Example |
|---|---------|---------|
| 1 | Token ring stabilization | `examples/token_ring_demo.c` |
| 2 | Mutual exclusion on a ring | `examples/mutual_exclusion.c` |
| 3 | Fault recovery from corruption | `examples/fault_recovery.c` |

## L7: Applications ✅ Complete (6 applications)

| # | Application | Implementation |
|---|-------------|---------------|
| 1 | Network routing table recovery | `ss_app_routing_recovery()` |
| 2 | Self-stabilizing leader election | `ss_app_leader_election()` |
| 3 | Sensor network time synchronization | `ss_app_time_sync()` |
| 4 | Cloud mutual exclusion | `ss_app_mutual_exclusion_cloud()` |
| 5 | Self-healing spanning tree | `ss_app_spanning_tree()` |
| 6 | Blockchain consensus stabilization | `ss_app_blockchain_consensus()` |

## L8: Advanced Topics ✅ Partial (4 topics)

| # | Topic | Implementation |
|---|-------|---------------|
| 1 | Probabilistic self-stabilization | `ss_probabilistic_expected_steps()`, `ProbabilisticSSSystem` (Lean) |
| 2 | Byzantine fault tolerance + SS | `fault_inject_byzantine()`, `ByzantineSSSystem` (Lean) |
| 3 | Cascade failure analysis | `fault_cascade_analysis()` |
| 4 | Energy function / Lyapunov method | `convergence_energy_delta()`, `energy_non_increasing` (Lean) |

## L9: Research Frontiers ✅ Partial (documented)

| # | Topic | Documentation |
|---|-------|--------------|
| 1 | Self-stabilizing blockchain consensus | `ss_app_blockchain_consensus()` |
| 2 | Byzantine self-stabilization | `ByzantineSSSystem` (Lean structure) |
| 3 | Probabilistic stabilization bounds | `expected_convergence_bound` (Lean) |
