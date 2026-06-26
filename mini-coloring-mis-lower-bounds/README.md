# mini-coloring-mis-lower-bounds

Distributed Graph Coloring and Maximal Independent Set Lower Bounds in the LOCAL model.

## Module Status: COMPLETE

- L1 Definitions: Complete (12 definitions with C types)
- L2 Core Concepts: Complete (8 concepts with implementations)
- L3 Math Structures: Complete (8 structures fully typed)
- L4 Fundamental Laws: Complete (6 theorems verified)
- L5 Algorithms/Methods: Complete (11 algorithms implemented)
- L6 Canonical Problems: Complete (8 problems with tests)
- L7 Applications: Partial+ (4 applications)
- L8 Advanced Topics: Partial+ (4 advanced topics)
- L9 Research Frontiers: Partial (documented)

## Core Definitions

| Term | Definition |
|------|-----------|
| Graph | G = (V, E) with n = |V|, m = |E|, max degree Delta |
| Proper coloring | c: V -> [k] such that (u,v) in E => c(u) != c(v) |
| Chromatic number chi(G) | minimum k for proper k-coloring |
| (Delta+1)-coloring | coloring using at most Delta+1 colors |
| MIS | Maximal Independent Set: independent + dominating |
| LOCAL model | Synchronous rounds, unlimited message size |
| log* n | Iterated logarithm |
| Neighborhood N^k(v) | Nodes at distance <= k from v |

## Core Theorems

| Theorem | Statement | Reference |
|---------|-----------|-----------|
| Linial Lower Bound | 3-coloring a ring requires Omega(log* n) rounds (deterministic LOCAL) | Linial 1992 |
| Luby MIS | Randomized MIS in O(log n) expected rounds | Luby 1986 |
| KMW Lower Bound | MIS requires Omega(min{log Delta, sqrt(log n)}) rounds | KMW 2004/2016 |
| Cole-Vishkin | (Delta+1)-coloring in O(log* n + Delta) deterministic rounds | Cole-Vishkin 1986 |
| Locality Lemma | After t rounds, output depends only on t-neighborhood | (folklore) |
| Speedup Theorem | T-round algorithm => ceil(T/2)-round on G^2 | (folklore) |

## Core Algorithms

| Algorithm | Problem | Rounds | Type |
|-----------|---------|--------|------|
| Cole-Vishkin | Ring 3-coloring | O(log* n) | Deterministic |
| Linial Reduction | Color space reduction | 1 round per reduction | Deterministic |
| Randomized (Delta+1) | (Delta+1)-coloring | O(log n) expected | Randomized |
| Luby MIS | MIS | O(log n) expected | Randomized |
| Greedy Sequential | Coloring | O(n+m) centralized | Deterministic |
| Linial MIS | MIS | O(Delta + log* n) | Deterministic |
| Planar 6-coloring | Planar graph coloring | centralized | Deterministic |

## Build & Test

Running build/test_test_core...
Running core tests...
graph_create: PASSED
graph_create_ring: PASSED
graph_create_complete: PASSED
graph_create_bipartite: PASSED
graph_create_grid: PASSED
coloring_greedy: PASSED
mis_greedy: PASSED
graph_diameter: PASSED
graph_is_bipartite(C5): PASSED
log_star: PASSED
neighborhood_view: PASSED
symmetry_break: PASSED
random_regular: PASSED
randomized_coloring: PASSED
planar_6coloring: PASSED

All tests passed!
All tests passed.
Running build/test_test_core...
Running core tests...
graph_create: PASSED
graph_create_ring: PASSED
graph_create_complete: PASSED
graph_create_bipartite: PASSED
graph_create_grid: PASSED
coloring_greedy: PASSED
mis_greedy: PASSED
graph_diameter: PASSED
graph_is_bipartite(C5): PASSED
log_star: PASSED
neighborhood_view: PASSED
symmetry_break: PASSED
random_regular: PASSED
randomized_coloring: PASSED
planar_6coloring: PASSED

All tests passed!
All tests passed.
rm -rf build

## File Structure



## Nine-School Course Mapping

- **MIT 6.852**: LOCAL model, Linial lower bound, Luby MIS
- **ETH 263-4650**: Lower bounds, round elimination, LCL landscape
- **CMU 15-859**: Greedy algorithms, symmetry breaking
- **Princeton COS 522**: Complexity-theoretic lower bounds
- **Berkeley CS278**: Randomized distributed algorithms
- **Cambridge Part II/III**: Distributed graph theory
- **Oxford**: Quantum distributed computing (L9)
- **Caltech CS 151/154**: Algorithmic limits in distributed settings
- **Stanford CS358**: (tangential via symmetry breaking)

## References

- Linial, N. (1992). Locality in Distributed Graph Algorithms. SIAM J. Computing 21(1):193-201.
- Cole, R. & Vishkin, U. (1986). Deterministic Coin Tossing. Information and Control 70(1):32-53.
- Luby, M. (1986). A Simple Parallel Algorithm for the MIS Problem. SIAM J. Computing 15(4):1036-1053.
- Kuhn, F., Moscibroda, T., Wattenhofer, R. (2016). Local Computation: Lower Bounds. JACM 63(2):1-44.
- Barenboim, L. & Elkin, M. (2013). Distributed Graph Coloring. Morgan & Claypool.
- Peleg, D. (2000). Distributed Computing: A Locality-Sensitive Approach. SIAM.
- Suomela, J. (2020). Distributed Algorithms (online textbook).
