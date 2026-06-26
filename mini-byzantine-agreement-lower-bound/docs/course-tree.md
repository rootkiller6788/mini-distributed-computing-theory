# Course Tree — mini-byzantine-agreement-lower-bound

## Prerequisites

This module depends on the following knowledge:

```
mini-complexity-foundations/
  mini-cook-levin-theorem/          ← NP-completeness (BA is NP-hard)
  mini-reductions-completeness/     ← Reduction technique (simulation argument)
  mini-time-hierarchy-theorem/      ← Round complexity hierarchy

mini-circuit-complexity/
  mini-boolean-circuits-model/      ← Circuit model for BA protocols
  mini-hastad-lower-bounds/         ← Lower bound proof techniques

mini-algorithmic-complexity/
  mini-communication-complexity/    ← Message complexity lower bounds
```

## Dependency Graph

```
                    ┌──────────────────────────┐
                    │ Cook-Levin (NP-complete)  │
                    └────────────┬─────────────┘
                                 │
                    ┌────────────▼─────────────┐
                    │ Reductions & Completeness │
                    └────────────┬─────────────┘
                                 │
              ┌──────────────────┼──────────────────┐
              │                  │                  │
    ┌─────────▼──────┐  ┌───────▼────────┐  ┌──────▼──────────┐
    │ Boolean        │  │ Time Hierarchy │  │ Communication   │
    │ Circuits       │  │ Theorem        │  │ Complexity      │
    └─────────┬──────┘  └───────┬────────┘  └──────┬──────────┘
              │                  │                  │
              └──────────────────┼──────────────────┘
                                 │
                    ┌────────────▼─────────────┐
                    │  Byzantine Agreement     │
                    │  Lower Bound             │  ← THIS MODULE
                    └────────────┬─────────────┘
                                 │
         ┌───────────────────────┼───────────────────────┐
         │                       │                       │
  ┌──────▼──────┐       ┌───────▼────────┐      ┌───────▼──────────┐
  │ PCP Theorem │       │ IP = PSPACE    │      │ Fine-Grained     │
  │ (future)    │       │ (future)       │      │ Complexity       │
  └─────────────┘       └────────────────┘      └──────────────────┘
```

## Knowledge Dependencies Within Module

```
L1: Definitions
  ├── L2: Core Concepts (consensus types, reductions)
  │     ├── L3: Math Structures (automaton, EIG tree, quorum)
  │     │     ├── L5: Algorithms (EIG, Phase King, Ben-Or, DS, PBFT)
  │     │     │     ├── L6: Canonical Problems (examples)
  │     │     │     └── L7: Applications (CAP, blockchain)
  │     │     └── L4: Fundamental Laws (LSP82, FLP, DS bounds)
  │     │           ├── L8: Advanced (quantum BA, hybrid consensus)
  │     │           └── L9: Research Frontiers
  │     └── L3: Indistinguishability
  │           └── L4: Impossibility proofs (key lemma)
  └── L2: Bivalence (FLP)
        └── L4: FLP impossibility
```

## Learning Path

1. Start with L1 (definitions): understand what Byzantine agreement is
2. Move to L2 (concepts): understand why it's hard (impossibility results)
3. Study L3 (structures): understand the formal models (automata, trees)
4. Deep dive L4 (theorems): prove the lower bounds
5. Learn L5 (algorithms): see what's achievable within the bounds
6. Practice L6 (problems): work through canonical examples
7. Explore L7 (applications): see real-world relevance (blockchain)
8. Advanced L8: push beyond classical bounds (quantum, hybrid)
9. Research L9: identify open problems

