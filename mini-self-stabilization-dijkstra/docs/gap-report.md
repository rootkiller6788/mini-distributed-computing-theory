# Gap Report — mini-self-stabilization-dijkstra

## Current Gaps

### L8: Advanced Topics — Missing Items

| # | Missing Topic | Priority | Notes |
|---|---------------|----------|-------|
| 1 | Time-adaptive self-stabilization | Medium | Systems that adapt stabilization time to fault severity |
| 2 | Super-stabilization | Medium | Guarantees safety during stabilization period |
| 3 | Snap-stabilization | Medium | Stabilizes in 0 steps for specific fault types |
| 4 | Self-stabilizing group communication | Low | Reliable broadcast with stabilization |

### L9: Research Frontiers — Missing Items

| # | Missing Topic | Priority | Notes |
|---|---------------|----------|-------|
| 1 | Self-stabilizing Byzantine agreement | High | Active research area (Dolev et al., 2010s) |
| 2 | Game-theoretic self-stabilization | Low | Incentive-compatible stabilization |
| 3 | Quantum self-stabilization | Low | Stabilization with quantum communication |

### Lean Formalization Gaps

| # | Gap | Priority |
|---|-----|----------|
| 1 | Complete convergence proof (tactic-based) | High |
| 2 | Machine-checked closure proof | High |
| 3 | Formal energy function monotonicity proof | Medium |
| 4 | Composition theorem full proof | Medium |

## Priority Ranking

1. **Complete Lean convergence proof** — currently uses `sorry` for non-trivial arithmetic
2. **Time-adaptive self-stabilization** — practical importance for dynamic networks
3. **Snap-stabilization** — emerging topic with practical applications
4. **Self-stabilizing Byzantine agreement** — frontier research

## Rationale for Gaps

- The Lean proofs use `sorry` for low-level arithmetic (e.g., `omega` without Mathlib)
- The `Fin n` predecessor construction requires proof terms that are cumbersome in pure Lean 4
- L9 topics are research-level — full implementations require novel algorithms not yet published
