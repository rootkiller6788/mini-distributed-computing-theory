# Course Alignment — mini-self-stabilization-dijkstra

## Nine-School Curriculum Mapping

### MIT
- **6.841 Advanced Complexity Theory**: Self-stabilization, fault tolerance, distributed computing
  - Aligned: Dijkstra's ring algorithm, convergence/closure proofs
- **6.045 Automata, Computability, Complexity**: Finite automata models
  - Aligned: State machine abstraction, guarded commands

### Stanford
- **CS254 Computational Complexity**: Self-stabilization & convergence
  - Aligned: Energy functions, Lyapunov methods for convergence
- **CS358 Circuit Complexity**: N/A (different domain)
  - Indirect: State machine as computational model

### Berkeley
- **CS172 Computability & Complexity**: Distributed algorithms
  - Aligned: Token ring mutual exclusion, convergence analysis
- **CS278 Computational Complexity**: Stabilization & convergence
  - Aligned: Exhaustive state space enumeration, attractor analysis

### CMU
- **15-455 Undergraduate Complexity**: Finite automata & distributed systems
  - Aligned: State machine framework, ring topology modeling
- **15-855 Graduate Complexity**: Distributed algorithms & stabilization
  - Aligned: Daemon models, fairness policies, fault injection

### Princeton
- **COS 522 Computational Complexity**: Self-stabilization
  - Aligned: Dijkstra's original algorithms, 3-state and K-state
- **COS 551 Advanced Complexity**: Lyapunov methods for distributed systems
  - Aligned: Energy functions, monotonicity verification

### Caltech
- **CS 151 Complexity Theory**: Limits of computation
  - Aligned: State lower bound theorem, symmetry-breaking lemma
- **CS 154 Limits of Computation**: Distributed computation limits
  - Aligned: Lower bounds on uniform ring stabilization

### Cambridge
- **Part II Complexity Theory**: Distributed computing foundations
  - Aligned: Self-stabilization definition, closure/convergence
- **Part III Advanced Complexity**: Advanced distributed algorithms
  - Aligned: Daemon models, probabilistic stabilization

### Oxford
- **Computational Complexity**: Distributed computing & stabilization
  - Aligned: Token-based mutual exclusion, leader election
- **Advanced Complexity Theory**: Quantum & advanced topics
  - Aligned: Probabilistic self-stabilization, Byzantine models

### ETH
- **263-4650 Advanced Complexity**: Self-stabilizing algorithms
  - Aligned: All three Dijkstra variants, composition theorem
- **252-0400 Logic & Computation**: Formal methods
  - Aligned: Lean 4 formalization of self-stabilization theorems

## Core Intersection

The following topics appear across **all nine schools**:

1. Self-stabilization definition (Dijkstra, 1974)
2. Closure and convergence properties
3. Dijkstra's token ring algorithm
4. Daemon / scheduler models
5. Fault tolerance and recovery
6. Mutual exclusion via token passing

## Unique Contributions by School

| School | Unique Perspective | Module Coverage |
|--------|-------------------|----------------|
| MIT | Theory → quantum link | Probabilistic stabilization |
| Stanford | Circuit complexity tradition | State machine abstraction |
| Berkeley | PCP theorem | Exhaustive verification |
| CMU | Algorithm → complexity bridge | Algorithm comparison |
| Princeton | Cryptography applications | Fault injection framework |
| Caltech | Physics intersection | Energy/Lyapunov functions |
| Cambridge | British complexity tradition | Daemon taxonomy |
| Oxford | Quantum computing | Probabilistic bounds |
| ETH | Logic depth | Lean formalization |
