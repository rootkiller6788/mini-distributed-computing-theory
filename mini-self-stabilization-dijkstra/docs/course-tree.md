# Course Tree — mini-self-stabilization-dijkstra

## Prerequisites

This module depends on the following prerequisite knowledge:

```
mini-self-stabilization-dijkstra
├── Discrete Mathematics
│   ├── Modular arithmetic (mod K operations)
│   ├── Graph theory (ring topology = cycle graph C_N)
│   └── Combinatorics (K^N state space enumeration)
├── Automata Theory
│   ├── Finite state machines
│   ├── State transition functions
│   └── Reachability in transition graphs
├── Distributed Computing
│   ├── Mutual exclusion problem
│   ├── Token-based algorithms
│   └── Fault models (transient, Byzantine)
├── Complexity Theory
│   ├── O-notation (convergence bounds)
│   ├── NP-completeness (n/a for this module)
│   └── Hierarchy theorems (n/a)
└── Formal Methods (for Lean formalization)
    ├── Predicate logic
    ├── Inductive types
    └── Theorem proving (Lean 4)
```

## Dependency Graph (Forward)

This module serves as a prerequisite for:

```
mini-self-stabilization-dijkstra
├── Advanced Self-Stabilization
│   ├── Time-adaptive stabilization
│   ├── Super-stabilization
│   └── Snap-stabilization
├── Byzantine Fault Tolerance
│   ├── Byzantine self-stabilization
│   └── Self-stabilizing consensus
├── Network Protocol Design
│   ├── Self-stabilizing routing (RIP, BGP)
│   └── Self-stabilizing spanning tree (STP)
├── Cloud / Distributed Systems
│   ├── Leader election (ZooKeeper, etcd)
│   └── Distributed locking (Chubby, DynamoDB)
└── Blockchain / Cryptocurrency
    ├── Proof-of-Stake consensus recovery
    └── Fork resolution via stabilization
```

## Knowledge Dependencies (Detailed)

### What This Module Assumes You Know

1. **Modular Arithmetic**: The core operations `(x+1) mod K` and `x mod K` are used throughout
2. **Ring Topology**: A ring is a cycle graph C_N where node i is connected to i-1 and i+1 (mod N)
3. **Finite State Machines**: Each machine is an FSM with states {0, ..., K-1} and transition rules
4. **Mutual Exclusion**: The problem of ensuring exactly one process in a critical section
5. **Fault Models**: Transient faults (state corruption that stops after the fault) vs permanent faults
6. **Asymptotic Analysis**: O(N²) convergence bounds, K^N state space growth

### What This Module Teaches

1. **Self-Stabilization Definition**: The two-part definition (closure + convergence)
2. **Dijkstra's Solutions**: Three algorithms (3-state, 4-state, K-state)
3. **Daemon Models**: Central, distributed, synchronous, and probabilistic
4. **Energy Functions**: Lyapunov-like potential functions for convergence proofs
5. **Symmetry Breaking**: Why distinguished machines are necessary
6. **Fault Recovery Analysis**: Measuring and bounding recovery time
7. **Applications**: How self-stabilization applies to real systems
