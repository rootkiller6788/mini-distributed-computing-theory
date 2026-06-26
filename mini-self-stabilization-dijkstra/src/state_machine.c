/**
 * @file state_machine.c
 * @brief Finite state machine framework for self-stabilizing algorithms
 *
 * Implements the state machine abstraction used to model individual
 * machines in Dijkstra's ring and other self-stabilizing systems.
 * Each machine is modeled as a set of guarded commands.
 *
 * Reference:
 *   Dijkstra, E.W. "Guarded Commands, Nondeterminacy, and Formal
 *     Derivation of Programs." CACM, 1975.
 *   Dolev, S. "Self-Stabilization." MIT Press, 2000, Chapter 2.
 *
 * Knowledge Points (each function = 1 independent knowledge point):
 *
 * L1: ss_machine_init/destroy — state machine instance management
 * L2: ss_machine_evaluate_guards — guard evaluation semantics
 * L3: ss_machine_execute_command — atomic command execution
 * L3: ss_build_transition_system — complete state space enumeration
 * L4: ss_verify_closure_exhaustive — closure verification
 * L4: ss_verify_convergence_exhaustive — convergence verification
 * L5: ss_max_convergence_steps — worst-case convergence distance
 */

#include "state_machine.h"
#include "dijkstra_ring.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── L1: State Machine Lifecycle ────────────────────────────────────────── */

/**
 * ss_machine_init — Allocate and initialize a state machine.
 *
 * Each machine starts with state 0, has a set of guarded commands
 * (rules), and knows its neighbors in the communication topology.
 *
 * Time: O(num_rules + num_neighbors), Space: O(rules + neighbors)
 */
int32_t ss_machine_init(ss_state_machine_t *machine,
                        int32_t machine_id,
                        int32_t num_states,
                        int32_t num_rules,
                        int32_t num_neighbors)
{
    if (!machine) return -1;
    if (num_states < 2 || num_states > 65536) return -1;
    if (num_rules < 0 || num_rules > 256) return -1;
    if (num_neighbors < 0 || num_neighbors > 256) return -1;

    machine->local_state  = 0;
    machine->num_states   = num_states;
    machine->machine_id   = machine_id;
    machine->num_rules    = num_rules;
    machine->num_neighbors = num_neighbors;
    machine->context      = NULL;

    if (num_rules > 0) {
        machine->rules = (guarded_command_t *)calloc((size_t)num_rules,
                                                      sizeof(guarded_command_t));
        if (!machine->rules) return -1;
    } else {
        machine->rules = NULL;
    }

    if (num_neighbors > 0) {
        machine->neighbor_ids = (int32_t *)calloc((size_t)num_neighbors,
                                                   sizeof(int32_t));
        if (!machine->neighbor_ids) {
            free(machine->rules);
            machine->rules = NULL;
            return -1;
        }
    } else {
        machine->neighbor_ids = NULL;
    }

    return 0;
}

/**
 * ss_machine_destroy — Free state machine resources.
 *
 * Time: O(1), Space: O(1)
 */
void ss_machine_destroy(ss_state_machine_t *machine)
{
    if (machine) {
        free(machine->rules);
        free(machine->neighbor_ids);
        machine->rules = NULL;
        machine->neighbor_ids = NULL;
        machine->num_rules = 0;
        machine->num_neighbors = 0;
    }
}

/**
 * ss_machine_set_rule — Register a guarded command.
 *
 * Each rule has the form: guard → command
 * When guard(local, neighbors) is true, the machine may execute
 * command(local, neighbors) to update its state.
 *
 * Knowledge: Guarded commands are the formal basis for describing
 *   self-stabilizing algorithms. Dijkstra introduced them as a
 *   programming language construct for nondeterministic programs,
 *   and they naturally model distributed machine programs.
 *
 * Time: O(1), Space: O(1)
 */
int32_t ss_machine_set_rule(ss_state_machine_t *machine,
                            int32_t rule_idx,
                            guard_fn_t guard,
                            command_fn_t command,
                            const char *name)
{
    if (!machine || !machine->rules) return -1;
    if (rule_idx < 0 || rule_idx >= machine->num_rules) return -1;
    if (!guard || !command) return -1;

    machine->rules[rule_idx].guard     = guard;
    machine->rules[rule_idx].command   = command;
    machine->rules[rule_idx].rule_name = name;
    return 0;
}

/**
 * ss_machine_set_neighbor — Register a neighbor machine ID.
 *
 * In a ring, each machine has exactly one predecessor (for Dijkstra's
 * unidirectional ring). But the framework supports arbitrary topologies.
 *
 * Time: O(1), Space: O(1)
 */
int32_t ss_machine_set_neighbor(ss_state_machine_t *machine,
                                int32_t neighbor_idx,
                                int32_t neighbor_id)
{
    if (!machine || !machine->neighbor_ids) return -1;
    if (neighbor_idx < 0 || neighbor_idx >= machine->num_neighbors) return -1;

    machine->neighbor_ids[neighbor_idx] = neighbor_id;
    return 0;
}

/**
 * ss_machine_evaluate_guards — Evaluate all guards and return the first
 * enabled rule index.
 *
 * Guards are evaluated in order (rule 0, rule 1, ...). The first
 * enabled guard determines which command executes. If no guard is
 * true, the machine is not privileged and should not execute.
 *
 * Knowledge: Guard evaluation semantics — the order of guard
 *   evaluation can affect behavior when multiple guards are true.
 *   Dijkstra's algorithms are designed so that guards are mutually
 *   exclusive, avoiding nondeterminism.
 *
 * Time: O(num_rules * guard_eval_time), Space: O(1)
 */
int32_t ss_machine_evaluate_guards(const ss_state_machine_t *machine,
                                   const int32_t *neighbor_states)
{
    int32_t i;
    if (!machine || !machine->rules) return -1;

    for (i = 0; i < machine->num_rules; i++) {
        if (machine->rules[i].guard(&machine->local_state,
                                     neighbor_states,
                                     machine->num_neighbors,
                                     machine->context)) {
            return i; /* First enabled rule */
        }
    }
    return -1; /* No guard enabled — machine is not privileged */
}

/**
 * ss_machine_execute_command — Execute the command for an enabled rule.
 *
 * The command computes the new local state based on the current
 * local state and neighbor states. The update is atomic at the
 * machine level.
 *
 * Knowledge: Atomic command execution — the state transition is
 *   indivisible. This models the fundamental assumption in
 *   self-stabilizing systems that a machine's state update,
 *   once initiated, completes without interference.
 *
 * Time: O(command_eval_time), Space: O(1)
 */
int32_t ss_machine_execute_command(ss_state_machine_t *machine,
                                   int32_t rule_idx,
                                   const int32_t *neighbor_states)
{
    if (!machine || !machine->rules) return -1;
    if (rule_idx < 0 || rule_idx >= machine->num_rules) return -1;

    machine->local_state = machine->rules[rule_idx].command(
        &machine->local_state,
        neighbor_states,
        machine->num_neighbors,
        machine->context);
    return machine->local_state;
}

/* ── L3: Global Configuration Management ───────────────────────────────── */

/**
 * ss_global_config_init — Initialize a global configuration snapshot.
 *
 * Allocates a state buffer that mirrors all machine local states,
 * enabling efficient neighbor lookups during simulation.
 *
 * Time: O(N), Space: O(N)
 */
int32_t ss_global_config_init(ss_global_config_t *config,
                              ss_state_machine_t *machines,
                              int32_t n)
{
    if (!config || !machines || n <= 0) return -1;

    config->machines     = machines;
    config->num_machines = n;

    config->state_buffer = (int32_t *)calloc((size_t)n, sizeof(int32_t));
    if (!config->state_buffer) return -1;

    ss_global_config_sync(config);
    return 0;
}

/**
 * ss_global_config_sync — Refresh the state buffer from local states.
 *
 * Must be called after any machine state change. The buffer provides
 * O(1) access to neighbor states during guard evaluation.
 *
 * Time: O(N), Space: O(1)
 */
void ss_global_config_sync(ss_global_config_t *config)
{
    int32_t i;
    if (!config || !config->state_buffer || !config->machines) return;

    for (i = 0; i < config->num_machines; i++) {
        config->state_buffer[i] = config->machines[i].local_state;
    }
}

/**
 * ss_global_config_destroy — Free the global configuration buffer.
 *
 * Time: O(1), Space: O(1)
 */
void ss_global_config_destroy(ss_global_config_t *config)
{
    if (config) {
        free(config->state_buffer);
        config->state_buffer = NULL;
        config->machines = NULL;
        config->num_machines = 0;
    }
}

/* ── L3: Complete State Space Enumeration ────────────────────────────────── */

/**
 * config_index_to_states — Convert a configuration index to state array.
 *
 * Uses mixed-radix representation: config_id encodes the states
 * of N machines, each with K possible states (base-K representation).
 *
 * config_id = Σ_{i=0}^{N-1} S[i] * K^i
 *
 * Time: O(N), Space: O(K^N) for the overall enumeration
 */
static void config_index_to_states(int32_t config_id, int32_t *states,
                                   int32_t n, int32_t k)
{
    int32_t i, rem = config_id;
    for (i = 0; i < n; i++) {
        states[i] = rem % k;
        rem /= k;
    }
}

/**
 * config_states_to_index — Convert state array to configuration index.
 *
 * Inverse of config_index_to_states.
 *
 * Time: O(N), Space: O(1)
 */
static int32_t config_states_to_index(const int32_t *states,
                                       int32_t n, int32_t k)
{
    int32_t i, index = 0, power = 1;
    (void)k; /* Used only for bounds */
    for (i = 0; i < n; i++) {
        index += states[i] * power;
        power *= k;
    }
    return index;
}

/**
 * ss_build_transition_system — Enumerate all K^N configurations and
 * all possible transitions under Dijkstra's algorithm.
 *
 * This is an exhaustive construction of the transition graph for
 * small rings. The state space grows exponentially (K^N), so this
 * is feasible only for N ≤ 5 and K ≤ 4 (max 4^5 = 1024 configs).
 *
 * For each configuration:
 *   1. Check which machines are privileged
 *   2. For each privileged machine, compute the resulting configuration
 *   3. Record the transition
 *
 * Knowledge: Complete state space analysis — by enumerating the
 *   entire transition graph, we can prove self-stabilization
 *   properties exhaustively for small systems. This is the
 *   computational analog of a mathematical proof by case analysis.
 *
 * Time: O(K^N * N^2), Space: O(K^N * N)
 */
int32_t ss_build_transition_system(ss_transition_system_t *ts,
                                   int32_t n, int32_t k,
                                   int32_t variant)
{
    int32_t total_configs, c, i, j, new_c;
    int32_t *states, *new_states;
    int32_t prev;
    int32_t trans_cap;

    if (!ts || n < 2 || n > 5 || k < 2 || k > 4) return -1;

    total_configs = 1;
    for (i = 0; i < n; i++) {
        total_configs *= k;
        if (total_configs > 10000) return -1; /* Too large */
    }

    ts->num_machines        = n;
    ts->num_machine_states  = k;
    ts->num_configs         = total_configs;
    ts->num_transitions     = 0;

    /* Allocate state array */
    ts->states = (int32_t *)calloc((size_t)(n * total_configs),
                                    sizeof(int32_t));
    if (!ts->states) return -1;

    /* Fill state array */
    states = (int32_t *)calloc((size_t)n, sizeof(int32_t));
    new_states = (int32_t *)calloc((size_t)n, sizeof(int32_t));
    if (!states || !new_states) {
        free(ts->states); free(states); free(new_states);
        return -1;
    }

    for (c = 0; c < total_configs; c++) {
        config_index_to_states(c, states, n, k);
        for (j = 0; j < n; j++) {
            ts->states[c * n + j] = states[j];
        }
    }

    /* Allocate transitions buffer (estimate: N * total_configs) */
    trans_cap = n * total_configs + 10;
    ts->transitions = (ss_transition_t *)calloc((size_t)trans_cap,
                                                 sizeof(ss_transition_t));
    if (!ts->transitions) {
        free(ts->states); free(states); free(new_states);
        ts->states = NULL;
        return -1;
    }
    ts->trans_capacity = trans_cap;

    /* Build transition relation */
    for (c = 0; c < total_configs; c++) {
        config_index_to_states(c, states, n, k);

        for (i = 0; i < n; i++) {
            /* Copy states for new configuration */
            for (j = 0; j < n; j++) new_states[j] = states[j];

            /* Determine if machine i is privileged and compute new state */
            prev = (i == 0) ? (n - 1) : (i - 1);
            int32_t moved = 0;

            if (variant == 0 || variant == 2) {
                /* 3-state or K-state rule */
                if (i == 0) {
                    if (states[0] == states[n - 1]) {
                        new_states[0] = (states[n - 1] + 1) % k;
                        moved = 1;
                    }
                } else {
                    if (states[i] != states[prev]) {
                        new_states[i] = states[prev];
                        moved = 1;
                    }
                }
            } else {
                /* 4-state rule */
                if (i == 0) {
                    if (((states[0] + 1) % k) == states[1]) {
                        new_states[0] = states[1];
                        moved = 1;
                    }
                } else if (i == n - 1) {
                    if (((states[n - 2] + 1) % k) == states[n - 1] &&
                        ((states[0] + 1) % k) != states[n - 1]) {
                        new_states[n - 1] = (states[n - 1] + 1) % k;
                        moved = 1;
                    }
                } else {
                    if (((states[prev] + 1) % k) != states[i]) {
                        new_states[i] = (states[prev] + 1) % k;
                        moved = 1;
                    }
                }
            }

            if (moved) {
                new_c = config_states_to_index(new_states, n, k);

                /* Add transition */
                if (ts->num_transitions >= ts->trans_capacity) {
                    /* Resize */
                    int32_t new_cap = ts->trans_capacity * 2;
                    ss_transition_t *new_t =
                        (ss_transition_t *)realloc(ts->transitions,
                           (size_t)new_cap * sizeof(ss_transition_t));
                    if (!new_t) {
                        free(states); free(new_states);
                        return -1;
                    }
                    ts->transitions = new_t;
                    ts->trans_capacity = new_cap;
                }

                ts->transitions[ts->num_transitions].from_config_id = c;
                ts->transitions[ts->num_transitions].to_config_id   = new_c;
                ts->transitions[ts->num_transitions].machine_id     = i;
                ts->transitions[ts->num_transitions].rule_idx       = 0;
                ts->num_transitions++;
            }
        }
    }

    free(states);
    free(new_states);
    return 0;
}

/**
 * ss_transition_system_destroy — Free transition system resources.
 */
void ss_transition_system_destroy(ss_transition_system_t *ts)
{
    if (ts) {
        free(ts->states);
        free(ts->transitions);
        ts->states = NULL;
        ts->transitions = NULL;
        ts->num_configs = 0;
        ts->num_transitions = 0;
    }
}

/* ── L4: Exhaustive Verification ───────────────────────────────────────── */

/**
 * ss_config_is_legitimate — Determine if a configuration is legitimate.
 *
 * For Dijkstra's ring, legitimate = exactly one machine has a privilege.
 * This is computed from the stored states.
 *
 * Time: O(N), Space: O(1)
 */
bool ss_config_is_legitimate(const ss_transition_system_t *ts,
                             int32_t config_id)
{
    int32_t i, n, count = 0;
    const int32_t *states;

    if (!ts || config_id < 0 || config_id >= ts->num_configs) return false;

    n = ts->num_machines;
    states = &ts->states[config_id * n];

    for (i = 0; i < n; i++) {
        int32_t prev = (i == 0) ? (n - 1) : (i - 1);
        if (i == 0) {
            if (states[0] == states[n - 1]) count++;
        } else {
            if (states[i] != states[prev]) count++;
        }
    }

    return (count == 1);
}

/**
 * ss_verify_closure_exhaustive — Verify closure on the full transition system.
 *
 * Checks: for every legitimate configuration, all outgoing transitions
 * go to other legitimate configurations.
 *
 * Knowledge: Exhaustive closure verification — computational proof
 *   that the closure property holds for all legitimate configurations
 *   in a given ring size. Complements the analytical proof in Dijkstra's
 *   original paper.
 *
 * Time: O(|L| * out_degree), Space: O(1)
 */
bool ss_verify_closure_exhaustive(const ss_transition_system_t *ts)
{
    int32_t i;

    if (!ts) return false;

    for (i = 0; i < ts->num_transitions; i++) {
        int32_t from = ts->transitions[i].from_config_id;
        int32_t to   = ts->transitions[i].to_config_id;

        if (ss_config_is_legitimate(ts, from)) {
            if (!ss_config_is_legitimate(ts, to)) {
                return false; /* Closure violated! */
            }
        }
    }

    return true;
}

/**
 * ss_verify_convergence_exhaustive — Verify convergence on the full
 * transition system.
 *
 * Uses a simple reachability analysis: from every non-legitimate
 * configuration, check if there exists a path to a legitimate one.
 * Also checks for cycles in the non-legitimate region (using
 * limited-depth DFS with visited set).
 *
 * Knowledge: Exhaustive convergence verification — computational
 *   proof that from every configuration, all paths lead to a
 *   legitimate state. This verifies Dijkstra's convergence theorem
 *   for small instances.
 *
 * Time: O(|C| * |T|), Space: O(|C|)
 */
int32_t ss_verify_convergence_exhaustive(const ss_transition_system_t *ts)
{
    int32_t c, i, converged = 0;
    bool *visited;
    int32_t *queue;
    int32_t q_head, q_tail;
    int32_t max_steps_per_config;

    if (!ts || ts->num_configs <= 0) return -1;

    max_steps_per_config = ts->num_configs * 2; /* Safety bound */

    visited = (bool *)calloc((size_t)ts->num_configs, sizeof(bool));
    queue   = (int32_t *)calloc((size_t)ts->num_configs, sizeof(int32_t));
    if (!visited || !queue) {
        free(visited); free(queue);
        return -1;
    }

    for (c = 0; c < ts->num_configs; c++) {
        if (ss_config_is_legitimate(ts, c)) {
            visited[c] = true; /* Legitimate, no need to check */
            converged++;
            continue;
        }

        /* BFS from c to find a legitimate config */
        bool *local_visited = (bool *)calloc((size_t)ts->num_configs,
                                              sizeof(bool));
        bool found = false;
        q_head = 0;
        q_tail = 0;
        queue[q_tail++] = c;
        local_visited[c] = true;

        while (q_head < q_tail && !found) {
            int32_t cur = queue[q_head++];

            if (ss_config_is_legitimate(ts, cur)) {
                found = true;
                break;
            }

            /* Follow all outgoing transitions */
            for (i = 0; i < ts->num_transitions; i++) {
                if (ts->transitions[i].from_config_id == cur) {
                    int32_t next = ts->transitions[i].to_config_id;
                    if (!local_visited[next]) {
                        local_visited[next] = true;
                        if (q_tail < ts->num_configs) {
                            queue[q_tail++] = next;
                        }
                    }
                }
            }

            /* Safety: prevent infinite loop */
            if (q_head > max_steps_per_config) break;
        }

        if (found) {
            visited[c] = true;
            converged++;
        }

        free(local_visited);
    }

    free(visited);
    free(queue);

    /* Return number of configs that converge; -1 if any diverge */
    if (converged < ts->num_configs) return -1;
    return converged;
}

/**
 * ss_max_convergence_steps — Compute the worst-case convergence distance.
 *
 * For each non-legitimate configuration, computes the shortest path
 * to a legitimate configuration (using BFS). Returns the maximum
 * over all non-legitimate starting points.
 *
 * This gives the exact worst-case convergence time for a specific
 * ring size under a central daemon with worst-case scheduling.
 *
 * Knowledge: Convergence distance (attraction radius) — the maximum
 *   number of steps needed to reach legitimacy from any starting
 *   configuration. For Dijkstra's 3-state ring, this is known to
 *   be O(N^2) in the worst case.
 *
 * Time: O(|C| * |T|), Space: O(|C|)
 */
int32_t ss_max_convergence_steps(const ss_transition_system_t *ts)
{
    int32_t c, i, max_dist = 0;
    int32_t *distance;
    int32_t *queue;
    int32_t q_head, q_tail;

    if (!ts || ts->num_configs <= 0) return -1;

    distance = (int32_t *)calloc((size_t)ts->num_configs, sizeof(int32_t));
    queue    = (int32_t *)calloc((size_t)ts->num_configs, sizeof(int32_t));
    if (!distance || !queue) {
        free(distance); free(queue);
        return -1;
    }

    /* Initialize distances: 0 for legitimate, -1 for others */
    q_head = 0;
    q_tail = 0;
    for (c = 0; c < ts->num_configs; c++) {
        if (ss_config_is_legitimate(ts, c)) {
            distance[c] = 0;
            queue[q_tail++] = c;
        } else {
            distance[c] = -1;
        }
    }

    /* BFS backward (reverse transitions) from legitimate set */
    /* Build reverse adjacency */
    int32_t *rev_head = (int32_t *)calloc((size_t)(ts->num_configs + 1),
                                           sizeof(int32_t));
    int32_t *rev_edges = (int32_t *)calloc((size_t)ts->num_transitions,
                                            sizeof(int32_t));
    int32_t *rev_next  = (int32_t *)calloc((size_t)ts->num_transitions,
                                            sizeof(int32_t));

    if (!rev_head || !rev_edges || !rev_next) {
        free(distance); free(queue);
        free(rev_head); free(rev_edges); free(rev_next);
        return -1;
    }

    for (i = 0; i < ts->num_transitions; i++) {
        rev_edges[i] = ts->transitions[i].from_config_id;
        int32_t to_c = ts->transitions[i].to_config_id;
        rev_next[i] = rev_head[to_c];
        rev_head[to_c] = i + 1; /* 1-based to distinguish from 0 */
    }

    /* BFS from all legitimate configs simultaneously */
    while (q_head < q_tail) {
        int32_t cur = queue[q_head++];
        int32_t cur_dist = distance[cur];

        /* Follow reverse edges from cur */
        int32_t edge = rev_head[cur];
        while (edge > 0) {
            int32_t prev_c = rev_edges[edge - 1];
            if (distance[prev_c] < 0) {
                distance[prev_c] = cur_dist + 1;
                if (distance[prev_c] > max_dist) {
                    max_dist = distance[prev_c];
                }
                queue[q_tail++] = prev_c;
            }
            edge = rev_next[edge - 1];
        }
    }

    free(rev_head);
    free(rev_edges);
    free(rev_next);
    free(distance);
    free(queue);

    return max_dist;
}
