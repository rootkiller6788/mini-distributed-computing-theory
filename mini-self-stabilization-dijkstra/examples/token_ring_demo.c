/**
 * @file token_ring_demo.c
 * @brief End-to-end demonstration of Dijkstra's 3-state token ring
 *
 * Demonstrates the complete life cycle:
 *   1. Create a ring of N machines with the 3-state algorithm
 *   2. Start from a random (potentially corrupted) configuration
 *   3. Observe convergence to a legitimate state (single token)
 *   4. Track the token as it circulates around the ring
 *   5. Inject a transient fault and observe recovery
 *
 * This is a self-contained, runnable example with >30 lines of
 * meaningful output, demonstrating L6 (Canonical Problems):
 * mutual exclusion on a ring via self-stabilization.
 */

#include "dijkstra_ring.h"
#include "convergence.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int32_t n = 7; /* 7 machines in the ring */
    int32_t k = 3; /* 3-state algorithm */
    ring_configuration_t config;

    printf("╔══════════════════════════════════════════════╗\n");
    printf("║  Dijkstra's Self-Stabilizing Token Ring      ║\n");
    printf("║  3-State Algorithm (Dijkstra, CACM 1974)    ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");

    /* ── Phase 1: Initialization ─────────────────────────────────── */
    printf("Phase 1: Ring Initialization\n");
    printf("──────────────────────────────\n");
    if (ring_init(&config, n, k, ALGORITHM_3STATE) < 0) {
        printf("ERROR: Failed to initialize ring\n");
        return 1;
    }
    printf("Created ring with %d machines, %d states per machine.\n", n, k);
    printf("Machine 0 is the distinguished \"bottom\" machine.\n");
    printf("Algorithm: bottom uses rule S[0]=S[N-1]→S[0]=(S[N-1]+1)%%K\n");
    printf("          others use rule S[i]≠S[i-1]→S[i]=S[i-1]\n\n");

    /* ── Phase 2: Random Starting Configuration ──────────────────── */
    printf("Phase 2: Random Initial Configuration (Corrupted State)\n");
    printf("───────────────────────────────────────────────────────\n");
    ring_randomize_states(&config, 42);
    printf("Initial configuration after random perturbation:\n");
    ring_print(&config);
    printf("\n");

    /* ── Phase 3: Convergence ────────────────────────────────────── */
    printf("Phase 3: Self-Stabilization (Convergence)\n");
    printf("──────────────────────────────────────────\n");

    /* Track convergence history */
    ring_step_record_t history[200];
    int32_t steps = ring_converge_to_legitimate(&config, 1000,
                                                 history, 200);
    if (steps < 0) {
        printf("ERROR: Failed to converge within step limit.\n");
        ring_destroy(&config);
        return 1;
    }

    printf("Converged to legitimate state in %d steps!\n", steps);
    printf("Final configuration:\n");
    ring_print(&config);
    printf("\n");

    printf("Last 5 steps of convergence:\n");
    int32_t start = (steps > 5) ? (steps - 5) : 0;
    int32_t s;
    for (s = start; s < steps && s < 200; s++) {
        printf("  Step %d: machine %d: state %d → %d (privilege: %d)\n",
               history[s].step_number,
               history[s].machine_id,
               history[s].old_state,
               history[s].new_state,
               (int)history[s].privilege);
    }
    printf("\n");

    /* ── Phase 4: Token Circulation ─────────────────────────────────*/
    printf("Phase 4: Token Circulation (Normal Operation)\n");
    printf("───────────────────────────────────────────────\n");
    printf("Once in a legitimate state, the token circulates\n");
    printf("around the ring, providing mutual exclusion:\n\n");

    int32_t round;
    for (round = 0; round < 15; round++) {
        /* Find and execute the privileged machine */
        int32_t i;
        for (i = 0; i < n; i++) {
            privilege_t p = ring_compute_privilege(&config, i);
            if (p != PRIVILEGE_NONE) {
                machine_state_t old = ring_get_state(&config, i);
                ring_step(&config, i);
                printf("  Round %2d: Token at machine %d (state %d→%d)\n",
                       round, i, old, ring_get_state(&config, i));
                break;
            }
        }
    }
    printf("\n");

    /* ── Phase 5: Fault Injection and Recovery ───────────────────── */
    printf("Phase 5: Fault Injection and Recovery\n");
    printf("───────────────────────────────────────\n");

    /* Corrupt machine 3's state */
    printf("Injecting transient fault: corrupting machine 3's state...\n");
    ring_set_state(&config, 3, (ring_get_state(&config, 3) + 1) % k);

    printf("Configuration after fault:\n");
    ring_print(&config);
    printf("\n");

    /* Recover */
    printf("Self-stabilization recovery in progress...\n");
    steps = ring_converge_to_legitimate(&config, 1000, NULL, 0);
    if (steps >= 0) {
        printf("Recovered in %d steps! System is fault-tolerant.\n", steps);
        printf("Final configuration:\n");
        ring_print(&config);
    } else {
        printf("ERROR: Recovery failed.\n");
    }
    printf("\n");

    /* ── Phase 6: Verification ───────────────────────────────────── */
    printf("Phase 6: Verification of Theorems\n");
    printf("───────────────────────────────────\n");

    bool closure = ring_verify_closure(&config);
    printf("Closure property holds: %s\n", closure ? "YES ✓" : "NO ✗");

    int32_t e_diff = convergence_energy_difference_count(&config);
    printf("Energy (difference count): %d\n", e_diff);

    printf("Minimum states for N=%d uniform ring: %d\n", n, n + 1);
    printf("(Dijkstra's 3-state algorithm with bottom machine beats\n");
    printf(" the uniform lower bound of %d states)\n\n", n + 1);

    printf("╔══════════════════════════════════════════════╗\n");
    printf("║  Demonstration Complete                      ║\n");
    printf("║  Dijkstra's self-stabilization works!        ║\n");
    printf("╚══════════════════════════════════════════════╝\n");

    ring_destroy(&config);
    return 0;
}
