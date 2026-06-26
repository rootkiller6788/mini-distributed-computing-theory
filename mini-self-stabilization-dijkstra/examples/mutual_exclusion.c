/**
 * @file mutual_exclusion.c
 * @brief Mutual exclusion via self-stabilizing token ring
 *
 * Demonstrates how Dijkstra's self-stabilizing token ring provides
 * distributed mutual exclusion: the machine holding the token has
 * exclusive access to the critical section. This example shows:
 *   1. Multiple processes competing for a shared resource
 *   2. Token-based arbitration ensuring exactly one process in CS
 *   3. Fairness of token circulation (each machine gets a turn)
 *   4. Self-stabilization after token loss or duplication
 *
 * L6 Canonical Problem: Mutual Exclusion on a Ring
 */

#include "dijkstra_ring.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* Simulate a critical section with a shared counter */
static int32_t shared_counter = 0;

static void enter_critical_section(int32_t machine_id) {
    printf("  [Machine %d] ENTER critical section\n", machine_id);
    shared_counter++;
    printf("  [Machine %d] Shared counter incremented to %d\n",
           machine_id, shared_counter);
    /* Simulate work in critical section */
    int32_t busy;
    for (busy = 0; busy < 1000000; busy++) { /* spin */ }
    printf("  [Machine %d] LEAVE critical section\n", machine_id);
}

int main(void) {
    int32_t n = 5;
    int32_t k = 3;
    ring_configuration_t config;

    printf("╔══════════════════════════════════════════════╗\n");
    printf("║  Self-Stabilizing Mutual Exclusion           ║\n");
    printf("║  via Dijkstra's Token Ring                  ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");

    ring_init(&config, n, k, ALGORITHM_3STATE);
    ring_randomize_states(&config, (uint32_t)time(NULL));

    printf("Initial state (random):\n");
    ring_print(&config);
    printf("\n");

    /* Converge to legitimate state first */
    printf("Stabilizing to legitimate state...\n");
    int32_t steps = ring_converge_to_legitimate(&config, 5000, NULL, 0);
    if (steps < 0) {
        printf("ERROR: Failed to converge\n");
        ring_destroy(&config);
        return 1;
    }
    printf("Converged in %d steps.\n", steps);
    ring_print(&config);
    printf("\n");

    /* Now simulate 20 rounds of mutual exclusion */
    printf("=== Mutual Exclusion Rounds ===\n");
    printf("(Token circulates, each machine gets a turn)\n\n");

    int32_t round;
    int32_t access_count[5] = {0, 0, 0, 0, 0};

    for (round = 0; round < 20; round++) {
        /* Find token holder */
        int32_t i;
        for (i = 0; i < n; i++) {
            privilege_t p = ring_compute_privilege(&config, i);
            if (p != PRIVILEGE_NONE) {
                /* This machine holds the token — enter CS */
                printf("Round %d: Token at machine %d\n", round, i);
                enter_critical_section(i);
                access_count[i]++;

                /* Pass the token */
                ring_step(&config, i);
                break;
            }
        }
    }

    printf("\n=== Fairness Statistics ===\n");
    int32_t i;
    for (i = 0; i < n; i++) {
        printf("  Machine %d: accessed CS %d times\n", i, access_count[i]);
    }
    printf("  Total critical section entries: %d\n", shared_counter);
    printf("  Average per machine: %.1f\n",
           (double)shared_counter / (double)n);
    printf("\n");

    /* Verify mutual exclusion: final state should still be legitimate */
    printf("Final ring state:\n");
    ring_print(&config);
    printf("Mutual exclusion maintained: %s\n",
           ring_is_legitimate(&config) ? "YES ✓" : "NO ✗");

    ring_destroy(&config);
    return 0;
}
