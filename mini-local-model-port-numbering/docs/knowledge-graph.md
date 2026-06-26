# Knowledge Graph — LOCAL Model + Port Numbering

## L1: Definitions (Complete)

| # | Concept | C Implementation | Lean Formalization |
|---|---------|-----------------|-------------------|
| 1 | LOCAL model definition | `local_context_t` in `local_model.h` | `Graph`, `RoundAction` in `local_model.lean` |
| 2 | Node with UID, degree, neighbors | `local_node_t` in `local_model.h` | `Vertex`, `UniqueIdentifiers` in `local_model.lean` |
| 3 | Synchronous round | `local_execute_round()` | `RoundAction` inductive type |
| 4 | Message in LOCAL model | `local_message_t` | `RoundAction.send/receive` |
| 5 | Termination | `terminated` flag + `terminated_count` | — |
| 6 | Port numbering definition | `port_numbering_t` in `port_numbering.h` | `PortNumbering` structure |
| 7 | Port-to-neighbor mapping | `port_get_neighbor_for_port()` | `neighbor_of_port` field |
| 8 | Neighbor-to-port mapping | `port_get_port_for_neighbor()` | `port_of_neighbor` field |
| 9 | Edge definition | `local_edge_t` | `Edge` structure |
| 10 | Adjacency matrix representation | `bool *adjacency` in `local_context_t` | `Graph.edges` |

## L2: Core Concepts (Complete)

| # | Concept | Implementation |
|---|---------|---------------|
| 1 | r-Neighborhood | `local_r_neighborhood()` |
| 2 | Synchronous execution engine | `local_execute_round()`, `local_execute_rounds()` |
| 3 | Graph distance & diameter | `local_distance()`, `local_diameter()` |
| 4 | Connectivity | `local_is_connected()` |
| 5 | Deterministic vs randomized | `lb_det_rand_gap()` |
| 6 | Graph properties (Δ, δ, avg deg, m) | `local_max_degree()`, `local_min_degree()`, etc. |
| 7 | Port numbering strategies | `port_assign_arbitrary()`, `port_assign_id_ordered()`, etc. |
| 8 | Port validation & consistency | `port_is_valid()`, `port_check_edge_consistency()` |
| 9 | Neighborhood indistinguishability | `lb_indistinguishability_test()` |
| 10 | Symmetry breaking via UIDs | `uids_break_initial_symmetry` (Lean) |

## L3: Mathematical Structures (Complete)

| # | Structure | Implementation |
|---|-----------|---------------|
| 1 | Graph topology: path P_n | `local_make_path()` |
| 2 | Graph topology: cycle C_n | `local_make_cycle()` |
| 3 | Graph topology: regular tree | `local_make_regular_tree()` |
| 4 | Graph topology: random G(n,p) | `local_make_random_graph()` |
| 5 | Graph topology: clique K_n | `local_make_clique()` |
| 6 | Edge list representation | `local_build_from_edges()` |
| 7 | State machine | `local_exec_state_t`, `local_get_state()` |
| 8 | Girth computation | `local_girth()` |
| 9 | Chromatic number χ(G) | `local_chromatic_number()` |
| 10 | Port-numbered dual view | `port_build_dual_view()` |
| 11 | Ring orientation port numbering | `port_assign_ring_orientation()` |
| 12 | BFS tree port numbering | `port_assign_bfs_tree()` |

## L4: Fundamental Laws / Theorems (Complete)

| # | Theorem | Implementation |
|---|---------|---------------|
| 1 | Linial's Ω(log* n) lower bound | `lb_linial_ring_impossible()`, `lb_linial_ring_min_rounds()` |
| 2 | Round elimination | `lb_round_elimination_step()`, `round_elimination_step` (Lean) |
| 3 | Neighborhood indistinguishability | `lb_neighborhood_classes()` |
| 4 | KMW MIS lower bound | `lb_kmw_hard_graph()`, `lb_kmw_mis_lower_bound()` |
| 5 | Speedup simulation | `lb_speedup_simulation()`, `speedup_simulation_exists` (Lean) |
| 6 | Covering graph / fibration | `lb_is_covering_graph()`, `lb_universal_cover_truncation()` |
| 7 | Communication complexity reduction | `lb_from_communication_complexity()` |
| 8 | Ramsey-theoretic threshold | `lb_ramsey_threshold()` |
| 9 | Neighborhood lemma (LOCAL) | `neighborhood_lemma` (Lean) |
| 10 | Port bijection theorem | `port_bijection` (Lean) |

## L5: Algorithms / Methods (Complete)

| # | Algorithm | Implementation |
|---|-----------|---------------|
| 1 | Cole-Vishkin color reduction | `cv_reduce_colors()`, `cv_full_reduction()` |
| 2 | Linial's (Δ+1)-coloring | `linial_color_reduce()`, `linial_delta_plus_one_coloring()` |
| 3 | Luby's randomized MIS | `mis_luby_round()`, `luby_mis_full()` |
| 4 | MIS random priority variant | `mis_random_priority_round()` |
| 5 | BFS tree construction | `bfs_tree_build()` |
| 6 | Broadcast | `bfs_broadcast()` |
| 7 | Convergecast | `bfs_convergecast()` |
| 8 | Leader election (deterministic) | `leader_election_deterministic()` |
| 9 | Leader election (randomized) | `leader_election_randomized()` |
| 10 | Triangle detection | `triangle_detect()`, `triangle_count()` |
| 11 | Ring sort | `ring_sort()` |
| 12 | Distributed sum | `distributed_sum()` |
| 13 | Distributed max | `distributed_max()` |
| 14 | Distance-2 coloring | `distance2_coloring()` |
| 15 | Maximal matching (full) | `mm_matching_round()`, `mm_full_random()` |
| 16 | Iterated logarithm log* n | `iterated_log2()` |
| 17 | Diff-bit-index (CV helper) | `diff_bit_index` (Lean) |

## L6: Canonical Problems (Complete)

| # | Problem | Implementation |
|---|---------|---------------|
| 1 | (Δ+1)-Vertex Coloring | `vcol_reduce_round()`, `vcol_is_proper()` |
| 2 | Maximal Independent Set | `mis_is_valid()` |
| 3 | Maximal Matching | `mm_is_valid()` |
| 4 | 3-Coloring a Ring | `ring_3color_cole_vishkin()`, `ring_3color_verify()` |
| 5 | Sinkless Orientation | `sinkless_round()`, `sinkless_is_valid()` |
| 6 | Weak 2-Coloring | `weak_2coloring()`, `weak_2coloring_verify()` |
| 7 | Vertex Cover (2-approx) | `vc_from_matching()`, `vc_is_valid()` |
| 8 | Dominating Set | `dominating_set_greedy()`, `domset_is_valid()` |

## L7: Applications (Partial+)

| # | Application | Status |
|---|-------------|--------|
| 1 | Wireless sensor networks (coloring = frequency assignment) | Documented |
| 2 | Leader election in ad-hoc networks | Implemented (`leader_election_*`) |
| 3 | Formal verification of LOCAL algorithms (Lean) | Implemented (`flood_max_correct`) |

## L8: Advanced Topics (Partial+)

| # | Topic | Status |
|---|-------|--------|
| 1 | Covering graph lower bounds | Implemented (`lb_is_covering_graph`) |
| 2 | Communication complexity reductions | Implemented (`lb_from_communication_complexity`) |
| 3 | Ramsey-theoretic lower bounds | Implemented (`lb_ramsey_threshold`) |

## L9: Research Frontiers (Partial)

| # | Topic | Status |
|---|-------|--------|
| 1 | SLOCAL model extensions | Documented in course-tree.md |
| 2 | Distributed quantum computing | Documented |
| 3 | Deterministic vs randomized complexity gaps | Implemented (`lb_det_rand_gap`) |
