# Mini Distributed Computing Theory

A collection of **from-scratch, zero-dependency C implementations** of classical results and algorithms in distributed computing theory. Each module translates textbook theorems—from FLP impossibility and Byzantine agreement lower bounds to LOCAL model round complexity and self-stabilization—into runnable, self-contained C code.

## Sub-Modules

| Sub-Module | Topics | Key Courses |
|-----------|--------|-------------|
| [mini-byzantine-agreement-lower-bound](mini-byzantine-agreement-lower-bound/) | Byzantine agreement, f < N/3 lower bound, EIG algorithm, Phase King, Ben-Or randomized, Dolev-Strong authenticated | MIT 6.852, ETH 263-4650, Princeton COS 551 |
| [mini-coloring-mis-lower-bounds](mini-coloring-mis-lower-bounds/) | Distributed (Δ+1)-coloring, MIS (Luby), symmetry breaking, Cole-Vishkin, Linial log* n lower bound | MIT 6.852, CMU 15-855, ETH 263-4650 |
| [mini-consensus-fault-tolerance](mini-consensus-fault-tolerance/) | Paxos, Raft, PBFT, Nakamoto consensus, quorum systems, crash/Byzantine fault models | MIT 6.852, Stanford CS254, CMU 15-712 |
| [mini-flp-impossibility-proof](mini-flp-impossibility-proof/) | FLP impossibility theorem, bivalence proof, asynchronous consensus, configuration graph exploration | MIT 6.852, Stanford CS244E, CMU 15-712 |
| [mini-local-model-port-numbering](mini-local-model-port-numbering/) | LOCAL/CONGEST models, port numbering, Linial lower bounds, 3-coloring ring, sinkless orientation | MIT 6.852, ETH 263-4650, Aalto CS |
| [mini-paxos-raft-formal](mini-paxos-raft-formal/) | Formal verification of Paxos & Raft, TLA+-like model checking, inductive invariants, safety proofs | MIT 6.852, Stanford CS254, Cambridge Part III |
| [mini-self-stabilization-dijkstra](mini-self-stabilization-dijkstra/) | Dijkstra's token ring, daemon/scheduler models, convergence analysis, closure proofs | MIT 6.841, Stanford CS254, ETH 263-4650 |
| [mini-synchronizer-alpha-beta-gamma](mini-synchronizer-alpha-beta-gamma/) | α/β/γ synchronizers (Awerbuch), synchronous simulation, BFS, leader election, MST in async networks | MIT 6.852, ETH 263-4650, Princeton COS 551 |

## Design Philosophy

- **Zero external dependencies** — pure C (C99/C11), only `libc` (and `libm` where needed)
- **Self-contained modules** — each directory has its own `Makefile`, `include/`, `src/`, `examples/`, `demos/`, `tests/`, `docs/`
- **Theory-to-code mapping** — every `.h` file embeds L1–L8 knowledge levels and course alignments from the original textbooks
- **Proofs as runnable artifacts** — impossibility proofs (FLP, BA lower bounds) are not just prose; they come with executable state-space explorers that demonstrate the bivalence construction

## Building

Each module is standalone. Navigate to a module directory and run:

```bash
cd mini-byzantine-agreement-lower-bound
make all    # build everything
make test   # run tests
```

Requires **GCC** and **GNU Make**.

## Project Structure

```
mini-distributed-computing-theory/
├── mini-byzantine-agreement-lower-bound/   # Byzantine agreement, f < N/3 lower bound, EIG, Phase King
├── mini-coloring-mis-lower-bounds/         # Distributed coloring, MIS, symmetry breaking, Linial lower bounds
├── mini-consensus-fault-tolerance/         # Paxos, Raft, PBFT, quorum systems, fault models
├── mini-flp-impossibility-proof/           # FLP theorem, bivalence, async impossibility
├── mini-local-model-port-numbering/        # LOCAL/CONGEST models, port numbering, round complexity
├── mini-paxos-raft-formal/                 # Formal verification, TLA+-like model checking, invariants
├── mini-self-stabilization-dijkstra/       # Dijkstra's ring, daemon models, convergence analysis
└── mini-synchronizer-alpha-beta-gamma/     # α/β/γ synchronizers, synchronous simulation
```

## License

MIT
