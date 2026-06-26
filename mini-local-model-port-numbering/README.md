# mini-local-model-port-numbering

**LOCAL Model of Distributed Computing with Port Numbering**

A complete implementation of the LOCAL model (Linial, 1987/1992), covering
definitions, algorithms, and lower bounds for synchronous message-passing
distributed computation.

---

## Module Status: COMPLETE ✅

| Level | Name | Status |
|-------|------|--------|
| **L1** | Definitions | Complete (10 items) |
| **L2** | Core Concepts | Complete (10 items) |
| **L3** | Mathematical Structures | Complete (12 items) |
| **L4** | Fundamental Laws | Complete (10 theorems) |
| **L5** | Algorithms / Methods | Complete (17 algorithms) |
| **L6** | Canonical Problems | Complete (8 problems) |
| **L7** | Applications | Partial+ (3 applications) |
| **L8** | Advanced Topics | Partial+ (3 topics) |
| **L9** | Research Frontiers | Partial (documented) |

**Code**: include/ + src/ = 4,948 lines | Tests: 121 passed, 0 failed | `make test` passes

---

## Core Definitions (L1)

- **LOCAL model**: Synchronous message-passing model with unbounded message size and local computation. Complexity measured in rounds.
- **Node**: `local_node_t` — UID, degree, neighbor list, port numbers, local state, termination flag.
- **Port numbering**: `port_numbering_t` — bijection from {0,...,deg-1} to incident edges.
- **Round**: All nodes (1) send to neighbors, (2) receive from neighbors, (3) compute locally.
- **Unique Identifiers (UIDs)**: Each node has a distinct identifier from {1,...,poly(n)}.

## Core Theorems (L4)

1. **Linial's Lower Bound (1992)**: 3-coloring a ring C_n requires Ω(log* n) deterministic rounds.
   - c_{t-1} ≥ f(c_t) where f grows like tower function.
2. **Kuhn-Moscibroda-Wattenhofer (2004/2016)**: MIS requires Ω(min{log Δ, √(log n)}) deterministic rounds.
3. **Round Elimination**: If a problem is solvable in T rounds, its "projected" version is solvable in T-1 rounds.
4. **Neighborhood Lemma**: After T rounds, node v knows its entire T-neighborhood.
5. **Covering Graph Lower Bounds**: If G → H is a covering map, LOCAL algorithms on H lift to G.
6. **Communication Complexity Reduction**: T ≥ C / (k · log n) for problems reducible to 2-party communication.
7. **Ramsey-Theoretic Bound**: Graph size n bounded by Ramsey number for given rounds and output complexity.
8. **Port Bijection**: Port numbering defines a valid bijection between ports and neighbors.

## Core Algorithms (L5)

| Algorithm | Complexity | Reference |
|-----------|-----------|-----------|
| Cole-Vishkin Color Reduction | 1 round per step | Cole & Vishkin (1986) |
| Linial (Δ+1)-Coloring | O(Δ² + log* n) | Linial (1992) |
| Luby's MIS | O(log n) randomized | Luby (1986) |
| BFS Tree Construction | O(diameter) | Standard |
| Leader Election | O(diameter) det / O(log n) rand | Standard |
| Triangle Detection | 1 round (classic LOCAL) | Standard |
| Ring Sort | O(n) | Odd-even transposition |
| Distance-2 Coloring | 1 round | Greedy on square graph |
| Maximal Matching | O(log n) randomized | Hanckowiak et al. |

## Canonical Problems (L6)

1. **(Δ+1)-Vertex Coloring** — Every node picks a color from {0,...,Δ}, adjacent nodes differ.
2. **Maximal Independent Set (MIS)** — Maximal set of non-adjacent vertices.
3. **Maximal Matching (MM)** — Maximal set of disjoint edges.
4. **3-Coloring a Ring** — Linial's canonical lower bound problem.
5. **Sinkless Orientation** — Orient edges so no node is a sink. Ω(log log n) lower bound (Brandt et al. 2016).
6. **Weak 2-Coloring** — Each node has at least one neighbor of different color.
7. **Vertex Cover (2-approximation)** — All endpoints of a maximal matching.
8. **Dominating Set** — Every node is in the set or adjacent to a node in the set.

## Nine-School Curriculum Mapping

| School | Key Courses | Covered Topics |
|--------|------------|----------------|
| MIT | 6.045, 6.841 | LOCAL model, lower bounds |
| Stanford | CS254, CS358 | Communication complexity, circuit analogies |
| Berkeley | CS172, CS278 | Round elimination, KMW bound |
| CMU | 15-455, 15-855 | Cole-Vishkin, Linial bounds |
| Princeton | COS 522, COS 551 | Luby MIS, covering graphs |
| Caltech | CS 151, CS 154 | Ramsey bounds, symmetry breaking |
| Cambridge | Part II/III | Port numbering, sinkless orientation |
| Oxford | Comp Complexity | Graph algorithms, det vs rand |
| ETH | 263-4650, 252-0400 | Round elimination, formal verification |

## Building and Testing

```bash
make all       # Build test suite and all examples
make test      # Run test suite (121 tests)
make examples  # Run all examples
make clean     # Remove build artifacts
```

## Directory Structure

```
mini-local-model-port-numbering/
├── Makefile
├── README.md                   ← This file
├── include/
│   ├── local_model.h           (329 lines) — Core LOCAL model
│   ├── port_numbering.h        (341 lines) — Port numbering
│   ├── graph_problems.h        (364 lines) — Canonical problems
│   ├── algorithms.h            (325 lines) — Distributed algorithms
│   └── lower_bounds.h          (300 lines) — Lower bound theorems
├── src/
│   ├── local_model.c           (594 lines) — Model implementation
│   ├── port_numbering.c        (466 lines) — Port numbering impl
│   ├── graph_problems.c        (699 lines) — Problem implementations
│   ├── algorithms.c            (664 lines) — Algorithm implementations
│   ├── lower_bounds.c          (459 lines) — Lower bound proofs
│   └── local_model.lean        (407 lines) — Lean 4 formalization
├── tests/
│   └── test_local_model.c      (520+ lines) — 121 assert-based tests
├── examples/
│   ├── example_ring_coloring.c — Cole-Vishkin on C_n
│   ├── example_mis_random.c    — Luby MIS on random graph
│   ├── example_leader_bfs.c    — Leader election + BFS tree
│   └── example_lower_bounds.c  — Lower bound explorer
└── docs/
    ├── knowledge-graph.md      — L1-L9 itemized coverage
    ├── coverage-report.md      — Per-level status
    ├── gap-report.md           — Missing items
    ├── course-alignment.md     — Nine-school mapping
    └── course-tree.md          — Prerequisite dependencies
```

## References

- Linial, N. (1992). "Locality in distributed graph algorithms." *SIAM J. Comput.* 21(1):193-201.
- Cole, R. & Vishkin, U. (1986). "Deterministic coin tossing..." *Inf. Control* 70(1):32-53.
- Luby, M. (1986). "A simple parallel algorithm for the MIS problem." *SIAM J. Comput.* 15(4):1036-1053.
- Naor, M. & Stockmeyer, L. (1995). "What can be computed locally?" *SIAM J. Comput.* 24(6):1259-1277.
- Kuhn, F., Moscibroda, T., Wattenhofer, R. (2004/2016). "What cannot be computed locally!" *PODC 2004 / JACM 2016*.
- Brandt, S. et al. (2016). "A lower bound for the distributed Lovász Local Lemma." *STOC 2016*.
- Suomela, J. (2013). "Survey of local algorithms." *ACM Comput. Surv.* 45(2):1-40.
- Barenboim, L. & Elkin, M. (2013). "Distributed Graph Coloring." Morgan & Claypool.
- Peleg, D. (2000). "Distributed Computing: A Locality-Sensitive Approach." SIAM.
