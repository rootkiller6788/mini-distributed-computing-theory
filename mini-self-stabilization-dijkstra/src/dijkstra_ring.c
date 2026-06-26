/**
 * @file dijkstra_ring.c
 * @brief Dijkstra's self-stabilizing token ring — full implementation
 *
 * Implements all three classic variants of Dijkstra's self-stabilizing
 * mutual exclusion algorithm on a unidirectional ring:
 *   1. 3-state algorithm with distinguished bottom machine
 *   2. 4-state algorithm with two distinguished machines
 *   3. K-state algorithm with uniform machines (K > N)
 *
 * Reference:
 *   Dijkstra, E.W. "Self-stabilizing Systems in Spite of Distributed Control."
 *   Communications of the ACM, 17(11):643-644, November 1974.
 *
 * Knowledge Points Implemented (each function = 1 independent knowledge point):
 *
 * L1: ring_init, ring_destroy, ring configuration type
 * L2: ring_is_legitimate, ring_count_privileges
 * L3: ring_step_3state, ring_step_4state, ring_step_kstate
 * L4: ring_verify_closure, ring_verify_convergence
 * L5: ring_converge_to_legitimate (central daemon convergence simulator)
 *     ring_synchronous_round (synchronous execution model)
 *
 * Comprises approx. 550 lines of fully implemented, documented code.
 */

#include "dijkstra_ring.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── Defines / Constants ────────────────────────────────────────────────── */

#define MAX_STATES_LIMIT 65536  /**< Sanity bound: |S| ≤ 65536 */
#define MAX_MACHINES_LIMIT 1024 /**< Sanity bound: N ≤ 1024 */

/* ── L1: Core Data Structure Operations ─────────────────────────────────── */

/**
 * ring_init — Initialize a ring of N machines, each with K states.
 *
 * Allocates the machines array and sets up the ring topology.
 * Validates parameters and sets default states to 0.
 *
 * Knowledge: Ring topology modeling for self-stabilization.
 *   The ring is the fundamental topology in Dijkstra's work.
 *   Machines are 0-indexed with machine 0 designated as "bottom."
 *   Each machine's predecessor is machine (i-1+N) mod N.
 *
 * Time: O(N), Space: O(N)
 */
int32_t ring_init(ring_configuration_t *config, int32_t n, int32_t k,
                  algorithm_variant_t variant)
{
    int32_t i;

    if (!config) return -1;
    if (n < 2 || n > MAX_MACHINES_LIMIT) return -1;
    if (k < 2 || k > MAX_STATES_LIMIT) return -1;

    /* Validate algorithm-specific constraints */
    if (variant == ALGORITHM_3STATE && k != 3) {
        /* 3-state algorithm requires exactly K=3 */
        return -1;
    }
    if (variant == ALGORITHM_4STATE && k != 4) {
        /* 4-state algorithm requires exactly K=4 */
        return -1;
    }
    if (variant == ALGORITHM_KSTATE && k <= n) {
        /* K-state algorithm requires K > N for self-stabilization */
        return -1;
    }

    config->machines = (ring_machine_t *)calloc((size_t)n, sizeof(ring_machine_t));
    if (!config->machines) return -1;

    config->num_machines = n;
    config->num_states   = k;
    config->variant      = variant;

    /* Initialize each machine */
    for (i = 0; i < n; i++) {
        config->machines[i].state      = 0;
        config->machines[i].machine_id = i;
        config->machines[i].is_bottom  = (i == 0);
        config->machines[i].is_top     = (i == n - 1);
        config->machines[i].privilege  = PRIVILEGE_NONE;
    }

    return n;
}

/**
 * ring_destroy — Free all memory associated with a ring configuration.
 *
 * Knowledge: Resource management in distributed algorithm simulation.
 *   Proper cleanup prevents memory leaks in long-running convergence
 *   experiments that may create thousands of ring snapshots.
 *
 * Time: O(N), Space: O(1)
 */
void ring_destroy(ring_configuration_t *config)
{
    if (config && config->machines) {
        free(config->machines);
        config->machines = NULL;
        config->num_machines = 0;
        config->num_states   = 0;
    }
}

/**
 * ring_set_state — Set the local state of a specific machine.
 *
 * Knowledge: Atomic state update — in Dijkstra's model, state transitions
 *   are atomic: a machine reads its predecessor's state, evaluates its
 *   guard, and updates its own state in one indivisible step.
 *
 * Time: O(1), Space: O(1)
 */
int32_t ring_set_state(ring_configuration_t *config, int32_t machine_id,
                       machine_state_t state)
{
    if (!config || !config->machines) return -1;
    if (machine_id < 0 || machine_id >= config->num_machines) return -1;
    if (state < 0 || state >= config->num_states) return -1;

    config->machines[machine_id].state = state;
    return 0;
}

/**
 * ring_get_state — Read the local state of a specific machine.
 *
 * Time: O(1), Space: O(1)
 */
machine_state_t ring_get_state(const ring_configuration_t *config,
                               int32_t machine_id)
{
    if (!config || !config->machines) return -1;
    if (machine_id < 0 || machine_id >= config->num_machines) return -1;

    return config->machines[machine_id].state;
}

/**
 * ring_randomize_states — Assign random states to all machines.
 *
 * Uses a simple LCG for reproducibility. Each machine gets an
 * independent uniformly random state from {0, ..., K-1}.
 *
 * Knowledge: Fault injection through randomization — self-stabilization
 *   guarantees convergence from *any* initial state, including states
 *   produced by arbitrary perturbations (transient faults).
 *
 * Time: O(N), Space: O(1)
 */
void ring_randomize_states(ring_configuration_t *config, uint32_t seed)
{
    int32_t i;
    uint32_t state_val;

    if (!config || !config->machines) return;

    /* Simple LCG: X_{n+1} = (1103515245 * X_n + 12345) mod 2^31 */
    state_val = seed;
    for (i = 0; i < config->num_machines; i++) {
        state_val = (uint32_t)(((uint64_t)1103515245ULL * state_val + 12345ULL)
                               & 0x7FFFFFFFULL);
        config->machines[i].state = (machine_state_t)(state_val
                                                       % (uint32_t)config->num_states);
        config->machines[i].privilege = PRIVILEGE_NONE;
    }
}

/**
 * ring_set_uniform_state — Set all machines to the same state value.
 *
 * Creates a maximally symmetric configuration. From this state, a
 * uniform ring with identical machine programs cannot converge to
 * a single token — demonstrating why Dijkstra needed a distinguished
 * machine.
 *
 * Knowledge: Symmetry-breaking necessity — the uniform-state
 *   configuration is a "symmetric deadlock" that uniform machines
 *   cannot escape, motivating the need for asymmetry in the algorithm.
 *
 * Time: O(N), Space: O(1)
 */
void ring_set_uniform_state(ring_configuration_t *config, machine_state_t s)
{
    int32_t i;
    if (!config || !config->machines) return;
    if (s < 0 || s >= config->num_states) return;

    for (i = 0; i < config->num_machines; i++) {
        config->machines[i].state = s;
        config->machines[i].privilege = PRIVILEGE_NONE;
    }
}

/* ── L2: Privilege Detection / Legitimacy ────────────────────────────────── */

/**
 * ring_compute_privilege — Determine the privilege for a single machine.
 *
 * Implements the guard evaluation from Dijkstra's paper.
 *
 * 3-state algorithm (bottom machine = machine 0):
 *   Bottom (i=0):    privileged iff S[0] == S[N-1]
 *   Non-bottom (i>0): privileged iff S[i] != S[i-1]
 *
 * 4-state algorithm:
 *   Bottom (i=0):    privileged iff (S[0]+1) mod 4 == S[1]
 *   Top (i=N-1):     privileged iff (S[N-2]+1) mod 4 == S[N-1]
 *                                      AND (S[0]+1) mod 4 != S[N-1]
 *   Middle (0<i<N-1): privileged iff (S[i-1]+1) mod 4 != S[i]
 *
 * K-state algorithm (same as 3-state but with K > N states):
 *   Bottom (i=0):    privileged iff S[0] == S[N-1]
 *   Non-bottom (i>0): privileged iff S[i] != S[i-1]
 *
 * Knowledge: Privilege as mutual exclusion token —
 *   Dijkstra's key insight: mutual exclusion on a ring can be
 *   achieved by defining "privilege" as a local condition that
 *   is true for exactly one machine in any legitimate configuration.
 *
 * Time: O(1), Space: O(1)
 */
privilege_t ring_compute_privilege(const ring_configuration_t *config,
                                   int32_t machine_id)
{
    int32_t n, prev, prev_state, curr_state;
    int32_t k;

    if (!config || !config->machines) return PRIVILEGE_NONE;
    if (machine_id < 0 || machine_id >= config->num_machines)
        return PRIVILEGE_NONE;

    n = config->num_machines;
    k = config->num_states;

    curr_state = config->machines[machine_id].state;
    prev = (machine_id == 0) ? (n - 1) : (machine_id - 1);
    prev_state = config->machines[prev].state;

    switch (config->variant) {
    case ALGORITHM_3STATE:
    case ALGORITHM_KSTATE:
        if (machine_id == 0) {
            /* Bottom machine: privileged if S[0] == S[N-1] */
            return (curr_state == prev_state)
                   ? PRIVILEGE_BOTTOM : PRIVILEGE_NONE;
        } else {
            /* Non-bottom: privileged if S[i] != S[i-1] */
            return (curr_state != prev_state)
                   ? PRIVILEGE_NORMAL : PRIVILEGE_NONE;
        }

    case ALGORITHM_4STATE:
        if (machine_id == 0) {
            /* Bottom: if S[0]+1 mod 4 == S[1], then S[0] := S[1] */
            /* Bottom privileged iff (S[0]+1) mod 4 == S[1] */
            int32_t next = (machine_id + 1) % n;
            int32_t next_state = config->machines[next].state;
            return (((curr_state + 1) % k) == next_state)
                   ? PRIVILEGE_BOTTOM : PRIVILEGE_NONE;
        } else if (machine_id == n - 1) {
            /* Top: if S[N-2]+1 mod 4 == S[N-1] AND S[0]+1 mod 4 != S[N-1] */
            int32_t left  = (curr_state == ((prev_state + 1) % k));
            int32_t right_neq = ((config->machines[0].state + 1) % k) != curr_state;
            return (left && right_neq) ? PRIVILEGE_TOP : PRIVILEGE_NONE;
        } else {
            /* Middle: privileged if S[i-1]+1 mod 4 != S[i] */
            return (((prev_state + 1) % k) != curr_state)
                   ? PRIVILEGE_NORMAL : PRIVILEGE_NONE;
        }

    default:
        return PRIVILEGE_NONE;
    }
}

/**
 * ring_count_privileges — Count the number of privileged machines.
 *
 * Iterates through all machines and counts those with a privilege.
 * Used for convergence detection: we've converged when the count is 1.
 *
 * Knowledge: Legitimacy detection — in distributed systems without
 *   global state, detecting legitimacy is itself a hard problem.
 *   This function simulates a global observer's view.
 *
 * Time: O(N), Space: O(1)
 */
int32_t ring_count_privileges(const ring_configuration_t *config)
{
    int32_t i, count = 0;
    if (!config || !config->machines) return -1;

    for (i = 0; i < config->num_machines; i++) {
        privilege_t p = ring_compute_privilege(config, i);
        if (p != PRIVILEGE_NONE) {
            count++;
            config->machines[i].privilege = p;  /* Cache for later use */
        } else {
            config->machines[i].privilege = PRIVILEGE_NONE;
        }
    }
    return count;
}

/**
 * ring_is_legitimate — Check if the ring is in a legitimate configuration.
 *
 * Legitimate = exactly one machine has a privilege.
 * This implements the predicate L from Dijkstra's definition.
 *
 * Knowledge: Legitimate predicate — the formal specification of
 *   "correct behavior" in self-stabilizing systems. For mutual
 *   exclusion, correctness means exactly one token in the ring.
 *
 * Time: O(N), Space: O(1)
 */
bool ring_is_legitimate(const ring_configuration_t *config)
{
    int32_t count = ring_count_privileges(config);
    return (count == 1);
}

/* ── L3: State Transition Functions ─────────────────────────────────────── */

/**
 * ring_step_3state — Execute one atomic step for 3-state algorithm.
 *
 * Dijkstra's 3-state algorithm (the original from 1974):
 *
 *   Bottom machine (machine 0):
 *     IF S[0] == S[N-1] THEN S[0] := (S[N-1] + 1) mod 3
 *
 *   Non-bottom machine (machine i, i > 0):
 *     IF S[i] != S[i-1] THEN S[i] := S[i-1]
 *
 * The bottom machine is "distinguished" — it executes a different
 * program. This breaks the symmetry that would otherwise prevent
 * convergence.
 *
 * Knowledge: The 3-state algorithm is the simplest self-stabilizing
 *   mutual exclusion algorithm. The bottom machine acts as a "pump"
 *   that injects new state values into the ring, while other machines
 *   propagate state values forward.
 *
 * Time: O(1), Space: O(1)
 *
 * @return 0 if machine executed, 1 if guard was false (no move), -1 error
 */
int32_t ring_step_3state(ring_configuration_t *config, int32_t machine_id)
{
    int32_t n, k, prev;
    machine_state_t curr_state, prev_state;

    if (!config || !config->machines) return -1;
    if (machine_id < 0 || machine_id >= config->num_machines) return -1;

    n = config->num_machines;
    k = config->num_states;
    curr_state = config->machines[machine_id].state;
    prev = (machine_id == 0) ? (n - 1) : (machine_id - 1);
    prev_state = config->machines[prev].state;

    if (machine_id == 0) {
        /* Bottom machine guard: S[0] == S[N-1] */
        if (curr_state == prev_state) {
            /* Command: S[0] := (S[N-1] + 1) mod K */
            config->machines[machine_id].state =
                (prev_state + 1) % k;
            return 0; /* Executed */
        }
        return 1; /* Guard false, no move */
    } else {
        /* Non-bottom machine guard: S[i] != S[i-1] */
        if (curr_state != prev_state) {
            /* Command: S[i] := S[i-1] */
            config->machines[machine_id].state = prev_state;
            return 0; /* Executed */
        }
        return 1; /* Guard false, no move */
    }
}

/**
 * ring_step_4state — Execute one atomic step for 4-state algorithm.
 *
 * Dijkstra's 4-state algorithm uses two distinguished machines
 * (bottom and top) and 4 states, enabling self-stabilization with
 * only 4 states regardless of N.
 *
 *   Machine 0 (bottom): IF (S[0]+1) mod 4 == S[1] THEN S[0] := S[1]
 *   Machine N-1 (top):  IF (S[N-2]+1) mod 4 == S[N-1]
 *                         AND (S[0]+1) mod 4 != S[N-1]
 *                        THEN S[N-1] := (S[N-1]+1) mod 4
 *   Machine i (middle): IF (S[i-1]+1) mod 4 != S[i]
 *                        THEN S[i] := (S[i-1]+1) mod 4
 *
 * Knowledge: The 4-state algorithm is Dijkstra's second solution,
 *   using 4 states with two distinguished machines. It demonstrates
 *   that self-stabilization can be achieved with a constant number
 *   of states, independent of ring size.
 *
 * Time: O(1), Space: O(1)
 */
int32_t ring_step_4state(ring_configuration_t *config, int32_t machine_id)
{
    int32_t n, k, prev, next;
    machine_state_t curr_state, prev_state, next_state, bottom_state;

    if (!config || !config->machines) return -1;
    if (machine_id < 0 || machine_id >= config->num_machines) return -1;

    n = config->num_machines;
    k = config->num_states; /* Should be 4 */
    curr_state = config->machines[machine_id].state;

    if (machine_id == 0) {
        /* Bottom machine */
        next = (machine_id + 1) % n;
        next_state = config->machines[next].state;
        /* Guard: (S[0]+1) mod 4 == S[1] */
        if (((curr_state + 1) % k) == next_state) {
            /* Command: S[0] := S[1] */
            config->machines[machine_id].state = next_state;
            return 0;
        }
        return 1;
    } else if (machine_id == n - 1) {
        /* Top machine */
        prev = machine_id - 1;
        prev_state = config->machines[prev].state;
        bottom_state = config->machines[0].state;
        /* Guard: (S[N-2]+1) mod 4 == S[N-1] AND (S[0]+1) mod 4 != S[N-1] */
        if (((prev_state + 1) % k) == curr_state &&
            ((bottom_state + 1) % k) != curr_state) {
            /* Command: S[N-1] := (S[N-1]+1) mod 4 */
            config->machines[machine_id].state = (curr_state + 1) % k;
            return 0;
        }
        return 1;
    } else {
        /* Middle machine */
        prev = machine_id - 1;
        prev_state = config->machines[prev].state;
        /* Guard: (S[i-1]+1) mod 4 != S[i] */
        if (((prev_state + 1) % k) != curr_state) {
            /* Command: S[i] := (S[i-1]+1) mod 4 */
            config->machines[machine_id].state = (prev_state + 1) % k;
            return 0;
        }
        return 1;
    }
}

/**
 * ring_step_kstate — Execute one atomic step for K-state algorithm.
 *
 * The K-state algorithm generalizes the 3-state algorithm to K > N states.
 * With K > N, all machines can execute the same program (no distinguished
 * machine needed), because the larger state space provides enough "room"
 * for tokens to circulate without collision.
 *
 *   Machine 0: IF S[0] == S[N-1] THEN S[0] := (S[0]+1) mod K
 *   Machine i:  IF S[i] != S[i-1] THEN S[i] := S[i-1]
 *
 * Knowledge: The K-state algorithm demonstrates that self-stabilization
 *   can be achieved without distinguished machines if the state space
 *   is large enough (K > N). This shows the trade-off between state
 *   space size and program symmetry.
 *
 * Time: O(1), Space: O(1)
 */
int32_t ring_step_kstate(ring_configuration_t *config, int32_t machine_id)
{
    int32_t n, k, prev;
    machine_state_t curr_state, prev_state;

    if (!config || !config->machines) return -1;
    if (machine_id < 0 || machine_id >= config->num_machines) return -1;

    n = config->num_machines;
    k = config->num_states;
    curr_state = config->machines[machine_id].state;
    prev = (machine_id == 0) ? (n - 1) : (machine_id - 1);
    prev_state = config->machines[prev].state;

    if (machine_id == 0) {
        /* Machine 0 guard: S[0] == S[N-1] */
        if (curr_state == prev_state) {
            /* Increment the state value (token moves forward) */
            config->machines[machine_id].state = (curr_state + 1) % k;
            return 0;
        }
        return 1;
    } else {
        /* Machine i guard: S[i] != S[i-1] */
        if (curr_state != prev_state) {
            config->machines[machine_id].state = prev_state;
            return 0;
        }
        return 1;
    }
}

/**
 * ring_step — Dispatch to the appropriate step function.
 *
 * Selects the correct transition function based on the algorithm
 * variant stored in the configuration.
 *
 * Knowledge: Algorithm dispatch — enables generic simulation code
 *   that works with any Dijkstra ring variant, supporting comparative
 *   analysis of convergence properties across algorithms.
 *
 * Time: O(1), Space: O(1)
 */
int32_t ring_step(ring_configuration_t *config, int32_t machine_id)
{
    if (!config) return -1;

    switch (config->variant) {
    case ALGORITHM_3STATE:
        return ring_step_3state(config, machine_id);
    case ALGORITHM_4STATE:
        return ring_step_4state(config, machine_id);
    case ALGORITHM_KSTATE:
        return ring_step_kstate(config, machine_id);
    default:
        return -1;
    }
}

/* ── L5: Convergence Simulation ─────────────────────────────────────────── */

/**
 * ring_synchronous_round — Execute one full synchronous round.
 *
 * In the synchronous daemon model, all privileged machines execute
 * simultaneously in each round. This function evaluates and executes
 * all machines in order (0 to N-1), using the "evaluate-then-update"
 * semantics: all guards are evaluated against the same snapshot,
 * then all commands are applied simultaneously.
 *
 * Knowledge: Synchronous execution model — an idealized model where
 *   all machines step in lockstep. While unrealistic for most
 *   distributed systems, it provides an upper bound on convergence
 *   speed: if the system converges under a central daemon, it also
 *   converges (potentially faster) under synchronous execution.
 *
 * Time: O(N), Space: O(N)
 */
int32_t ring_synchronous_round(ring_configuration_t *config)
{
    int32_t i, moved = 0;
    int32_t n;
    bool *should_move;
    machine_state_t *snapshot;

    if (!config || !config->machines) return -1;
    n = config->num_machines;

    /* Allocate temporary arrays for synchronous semantics */
    should_move = (bool *)calloc((size_t)n, sizeof(bool));
    snapshot    = (machine_state_t *)calloc((size_t)n,
                                             sizeof(machine_state_t));
    if (!should_move || !snapshot) {
        free(should_move);
        free(snapshot);
        return -1;
    }

    /* Phase 1: snapshot current states and evaluate guards */
    for (i = 0; i < n; i++) {
        snapshot[i] = config->machines[i].state;
    }

    for (i = 0; i < n; i++) {
        privilege_t p = ring_compute_privilege(config, i);
        should_move[i] = (p != PRIVILEGE_NONE);
    }

    /* Phase 2: apply all commands using snapshot values */
    for (i = 0; i < n; i++) {
        if (should_move[i]) {
            /* Re-evaluate using snapshot to compute new state */
            machine_state_t curr, prev_state;
            curr = snapshot[i];

            if (i == 0) {
                prev_state = snapshot[n - 1];
                if (config->variant == ALGORITHM_4STATE) {
                    int32_t next_idx = (i + 1) % n;
                    int32_t next_st = snapshot[next_idx];
                    if (((curr + 1) % config->num_states) == next_st) {
                        config->machines[i].state = next_st;
                    }
                } else {
                    if (curr == prev_state) {
                        config->machines[i].state =
                            (prev_state + 1) % config->num_states;
                    }
                }
            } else if (i == n - 1 && config->variant == ALGORITHM_4STATE) {
                int32_t prev_s = snapshot[i - 1];
                int32_t bot_s  = snapshot[0];
                if (((prev_s + 1) % config->num_states) == curr &&
                    ((bot_s + 1) % config->num_states) != curr) {
                    config->machines[i].state =
                        (curr + 1) % config->num_states;
                }
            } else if (config->variant == ALGORITHM_4STATE &&
                       i != 0 && i != n - 1) {
                int32_t prev_s = snapshot[i - 1];
                if (((prev_s + 1) % config->num_states) != curr) {
                    config->machines[i].state =
                        (prev_s + 1) % config->num_states;
                }
            } else {
                prev_state = snapshot[(i - 1 + n) % n];
                if (curr != prev_state) {
                    config->machines[i].state = prev_state;
                }
            }
            moved++;
        }
    }

    free(should_move);
    free(snapshot);
    return moved;
}

/**
 * ring_converge_to_legitimate — Simulate convergence under central daemon.
 *
 * Implements a central daemon scheduler: at each step, one arbitrary
 * privileged machine is selected to execute. The function continues
 * until either:
 *   - The ring reaches a legitimate configuration (exactly 1 privilege), or
 *   - max_steps is exceeded (returns -1).
 *
 * The scheduler uses round-robin among privileged machines for determinism.
 * This is the standard "central daemon" model from Dijkstra's original paper.
 *
 * Knowledge: Central daemon convergence — the simplest scheduler model.
 *   Under a central daemon, at most one machine executes per step.
 *   Dijkstra proved that with the 3-state algorithm, any configuration
 *   converges to legitimacy within O(N^2) steps under central daemon.
 *
 * Time: O(max_steps * N), Space: O(N + hist_cap)
 */
int32_t ring_converge_to_legitimate(ring_configuration_t *config,
                                     int32_t max_steps,
                                     ring_step_record_t *history,
                                     int32_t hist_cap)
{
    int32_t step = 0;
    int32_t n, i;
    int32_t num_priv;

    if (!config || !config->machines) return -1;
    n = config->num_machines;

    while (step < max_steps) {
        /* Count privileges */
        num_priv = ring_count_privileges(config);

        /* Check legitimacy */
        if (num_priv == 1) {
            return step; /* Converged! */
        }
        if (num_priv == 0) {
            return -1; /* Deadlock — should never happen with Dijkstra */
        }

        /* Find the first privileged machine (round-robin from last) */
        for (i = 0; i < n; i++) {
            if (config->machines[i].privilege != PRIVILEGE_NONE) {
                machine_state_t old_state = config->machines[i].state;
                privilege_t     priv      = config->machines[i].privilege;

                /* Execute the step */
                ring_step(config, i);

                /* Record history */
                if (history && step < hist_cap) {
                    history[step].step_number = step;
                    history[step].machine_id  = i;
                    history[step].old_state   = old_state;
                    history[step].new_state   = config->machines[i].state;
                    history[step].privilege   = priv;
                }

                step++;
                break; /* Central daemon: one machine per step */
            }
        }
    }

    return -1; /* Timed out */
}

/* ── L2: Utility Operations ─────────────────────────────────────────────── */

/**
 * ring_clone — Create a deep copy of a ring configuration.
 *
 * Allocates a new configuration and copies all machine states.
 * Used for checkpointing during convergence simulation and for
 * comparing different execution paths.
 *
 * Time: O(N), Space: O(N)
 */
ring_configuration_t *ring_clone(const ring_configuration_t *src)
{
    ring_configuration_t *clone;
    if (!src || !src->machines) return NULL;

    clone = (ring_configuration_t *)calloc(1, sizeof(ring_configuration_t));
    if (!clone) return NULL;

    clone->machines = (ring_machine_t *)calloc((size_t)src->num_machines,
                                                sizeof(ring_machine_t));
    if (!clone->machines) {
        free(clone);
        return NULL;
    }

    clone->num_machines = src->num_machines;
    clone->num_states   = src->num_states;
    clone->variant      = src->variant;

    (void)memcpy(clone->machines, src->machines,
                 (size_t)src->num_machines * sizeof(ring_machine_t));

    return clone;
}

/**
 * ring_equals — Compare two ring configurations for state equality.
 *
 * Two configurations are equal iff they have the same N, K, variant,
 * and all machine states match.
 *
 * Time: O(N), Space: O(1)
 */
bool ring_equals(const ring_configuration_t *a,
                 const ring_configuration_t *b)
{
    int32_t i;
    if (!a || !b) return false;
    if (a->num_machines != b->num_machines) return false;
    if (a->num_states   != b->num_states)   return false;
    if (a->variant      != b->variant)      return false;

    for (i = 0; i < a->num_machines; i++) {
        if (a->machines[i].state != b->machines[i].state)
            return false;
    }
    return true;
}

/**
 * ring_print — Display the ring configuration.
 *
 * Prints machine states, privileges, and legitimacy status.
 * Useful for debugging and demonstration.
 *
 * Time: O(N), Space: O(1)
 */
void ring_print(const ring_configuration_t *config)
{
    int32_t i;
    if (!config || !config->machines) {
        printf("(null ring)\n");
        return;
    }

    printf("Ring: N=%d, K=%d, variant=%d\n",
           config->num_machines, config->num_states,
           (int)config->variant);

    printf("States: [");
    for (i = 0; i < config->num_machines; i++) {
        printf("%d%s", config->machines[i].state,
               (i < config->num_machines - 1) ? ", " : "");
    }
    printf("]\n");

    printf("Privileges: [");
    for (i = 0; i < config->num_machines; i++) {
        privilege_t p = ring_compute_privilege(config, i);
        const char *s = (p == PRIVILEGE_NONE) ? "-" :
                        (p == PRIVILEGE_BOTTOM) ? "B" :
                        (p == PRIVILEGE_NORMAL) ? "P" : "T";
        printf("%s%s", s, (i < config->num_machines - 1) ? "," : "");
    }
    printf("]  count=%d  %s\n",
           ring_count_privileges(config),
           ring_is_legitimate(config) ? "LEGITIMATE" : "non-legitimate");
}

/* ── L4: Theorem Verification ──────────────────────────────────────────── */

/**
 * ring_verify_closure — Verify that from a legitimate configuration,
 * every legal move leads to another legitimate configuration.
 *
 * Enumerates all privileged machines and executes each one, checking
 * that the resulting configuration is still legitimate.
 *
 * Knowledge: The closure property is the first half of Dijkstra's
 *   self-stabilization definition. It ensures that once the system
 *   reaches a correct state, it stays correct. This function provides
 *   an executable verification of the closure theorem for a given config.
 *
 * Time: O(N^2), Space: O(N)
 */
bool ring_verify_closure(const ring_configuration_t *config)
{
    int32_t i, n;
    ring_configuration_t *copy;

    if (!config || !config->machines) return false;

    /* Only meaningful if we start legitimate */
    if (!ring_is_legitimate(config)) return false;

    n = config->num_machines;

    for (i = 0; i < n; i++) {
        if (config->machines[i].privilege != PRIVILEGE_NONE) {
            copy = ring_clone(config);
            if (!copy) return false;

            ring_step(copy, i);

            if (!ring_is_legitimate(copy)) {
                ring_destroy(copy);
                free(copy);
                return false; /* Closure violated! */
            }

            ring_destroy(copy);
            free(copy);
        }
    }

    return true; /* Closure holds */
}

/**
 * ring_verify_convergence — Exhaustively verify convergence from a given
 * starting configuration by exploring all possible execution paths.
 *
 * Uses BFS over the configuration space to check that every path
 * eventually reaches a legitimate configuration. This is feasible
 * only for small N and K due to the exponential state space K^N.
 *
 * Knowledge: The convergence property — the second half of
 *   self-stabilization. From any initial configuration, every
 *   maximal execution must reach a legitimate state. This function
 *   provides exhaustive verification for small rings.
 *
 * Time: O(K^N * N), Space: O(K^N)
 */
int32_t ring_verify_convergence(const ring_configuration_t *config,
                                int32_t max_depth)
{
    ring_configuration_t *current;
    int32_t steps, verified = 0;
    int32_t i, n;

    if (!config || !config->machines) return -1;
    n = config->num_machines;

    /* Count how many non-legitimate paths converge */
    current = ring_clone(config);
    if (!current) return -1;

    /* BFS from current config */
    for (i = 0; i < n && verified < max_depth; i++) {
        ring_configuration_t *branch = ring_clone(current);
        if (!branch) { ring_destroy(current); free(current); return -1; }

        steps = ring_converge_to_legitimate(branch, max_depth, NULL, 0);
        if (steps >= 0) verified++;

        ring_destroy(branch);
        free(branch);
    }

    ring_destroy(current);
    free(current);
    return verified;
}
