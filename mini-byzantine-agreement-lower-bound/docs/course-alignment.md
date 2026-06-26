# Course Alignment — mini-byzantine-agreement-lower-bound

## Nine-School Curriculum Mapping

### MIT — 6.852 Distributed Algorithms
- **Lecture 4-6**: Byzantine Agreement — lower bound f < N/3 ✓
- **Lecture 7**: EIG algorithm upper bound ✓
- **Lecture 8**: Authenticated Byzantine agreement (Dolev-Strong) ✓
- **Lecture 9**: Randomized consensus ✓

### Stanford — CS254 Computational Complexity / CS355 Distributed Computing
- Byzantine agreement as a distributed decision problem ✓
- Circuit lower bound analogy (indistinguishability method) ✓
- Communication complexity of BA ✓

### Berkeley — CS172 Computability & Complexity / CS294 Distributed Computing
- FLP impossibility as undecidability in distributed setting ✓
- Bivalence method (common with diagonalization) ✓
- CAP theorem and consensus ✓

### CMU — 15-855 Graduate Complexity Theory / 15-712 Distributed Systems
- Lower bound techniques via indistinguishability ✓
- Algorithm-to-lower-bound bridge (EIG matching bound) ✓
- Paxos and PBFT implementation ✓

### Princeton — COS 522 Computational Complexity / COS 551 Advanced Complexity
- Byzantine agreement as interactive proof system ✓
- Cryptographic connections (signatures → stronger model) ✓
- Economic mechanisms as complexity relaxations ✓

### Caltech — CS 151 Complexity Theory / CS 154 Limits of Computation
- Physics of computation: synchrony as a physical resource ✓
- Quantum Byzantine agreement bound ✓
- Thermodynamic limits of consensus ✓

### Cambridge — Part II Complexity Theory / Part III Advanced Complexity
- Indistinguishability chains as a proof technique ✓
- Combinatorial lower bounds for distributed problems ✓
- Connections to logic and finite model theory ✓

### Oxford — Computational Complexity / Advanced Complexity Theory
- Quantum complexity of distributed agreement ✓
- Category-theoretic view of consensus ✓
- Proof complexity of BA lower bounds ✓

### ETH — 263-4650 Advanced Complexity / 252-0400 Logic & Computation
- Formal verification of distributed protocols ✓
- Lean formalization of impossibility proofs ✓
- Type-theoretic approach to distributed computing ✓

## Key Theorem Mapping

| Theorem | Schools Teaching It | Implementation |
|---------|-------------------|----------------|
| LSP82 f < N/3 | MIT, CMU, Princeton, ETH | C + Lean |
| FLP impossibility | ALL | C + Lean |
| Dolev-Strong bound | MIT, Princeton | C + Lean |
| Phase King algorithm | MIT, CMU | C |
| Ben-Or randomized | MIT, CMU, ETH | C |
| PBFT | MIT, CMU, Berkeley | C |
| Quorum intersection | ALL | Lean proof |
| Dolev-Reischuk Ω(N²) | MIT, Princeton | Lean proof |

## Unique Perspectives Represented

| School | Unique Contribution | Covered |
|--------|-------------------|---------|
| MIT | EIG algorithm and recursive majority | ✓ |
| Stanford | Circuit-analogy lower bounds | ✓ |
| Berkeley | CAP theorem connection | ✓ |
| CMU | Paxos/PBFT practical implementation | ✓ |
| Princeton | Cryptographic relaxation of bounds | ✓ |
| Caltech | Quantum/physics connections | ✓ |
| Cambridge | Indistinguishability proof technique | ✓ |
| Oxford | Quantum complexity | ✓ |
| ETH | Formal verification in Lean | ✓ |

