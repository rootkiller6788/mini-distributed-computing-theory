/**
 * @file visualizer.c
 * @brief Console-based visualization of Dijkstra's token ring convergence
 *
 * Renders the evolution of machine states over time as an ASCII
 * animation, showing how a self-stabilizing ring converges from
 * a random configuration to a legitimate state with a single
 * circulating token.
 */

#include "dijkstra_ring.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#define SLEEP_MS(ms) Sleep(ms)
#else
#include <unistd.h>
#define SLEEP_MS(ms) usleep((ms) * 1000)
#endif

static void render_ring(const ring_configuration_t *config, int32_t step) {
    int32_t i, n = config->num_machines;
    const char *state_chars = ".oO*#@";

    printf("\033[H\033[J"); /* Clear screen (ANSI) */
    printf("═══════ Dijkstra's Self-Stabilizing Ring ═══════\n");
    printf("Step: %d  |  N=%d  |  K=%d  |  Algorithm: 3-State\n",
           step, n, config->num_states);
    printf("─────────────────────────────────────────────────\n\n");

    /* Render ring as a horizontal line of machines */
    printf("  ");
    for (i = 0; i < n; i++) {
        privilege_t p = ring_compute_privilege(config, i);
        int32_t s = config->machines[i].state;
        char marker = (p != PRIVILEGE_NONE) ? '▼' : ' ';
        printf(" %c ", marker);
    }
    printf("\n");

    /* Machine IDs */
    printf("  ");
    for (i = 0; i < n; i++) {
        printf("[%d]", i);
    }
    printf("\n");

    /* State values */
    printf("  ");
    for (i = 0; i < n; i++) {
        int32_t s = config->machines[i].state;
        printf(" %c ", state_chars[s % 6]);
    }
    printf("  ← state visual\n");

    /* Numerical states */
    printf("  ");
    for (i = 0; i < n; i++) {
        printf(" %d ", config->machines[i].state);
    }
    printf("  ← state value\n\n");

    /* Legend */
    printf("─────────────────────────────────────────────────\n");
    printf("▼ = privileged (holds token)  ");
    printf(".=0 o=1 O=2 *=3 #=4 @=5\n");
    printf("Privileges: %d  |  Legitimate: %s\n",
           ring_count_privileges(config),
           ring_is_legitimate(config) ? "YES ✓" : "NO");

    if (ring_is_legitimate(config)) {
        printf("\n\n✅ LEGITIMATE CONFIGURATION REACHED! ✅\n");
    }
}

int main(void) {
    int32_t n = 8;
    ring_configuration_t config;

    printf("Self-Stabilizing Ring Visualizer\n");
    printf("N=%d, K=3, 3-State Algorithm\n", n);
    printf("Starting in 2 seconds...\n");
    SLEEP_MS(2000);

    ring_init(&config, n, 3, ALGORITHM_3STATE);
    ring_randomize_states(&config, 12345);

    int32_t step = 0;
    int32_t max_steps = 500;

    while (step < max_steps) {
        render_ring(&config, step);

        if (ring_is_legitimate(&config)) {
            printf("\nConverged in %d steps!\n", step);
            /* Show a few rounds of normal token circulation */
            int32_t extra;
            for (extra = 0; extra < 10; extra++) {
                SLEEP_MS(500);
                int32_t i;
                for (i = 0; i < n; i++) {
                    if (ring_compute_privilege(&config, i)
                        != PRIVILEGE_NONE) {
                        ring_step(&config, i);
                        break;
                    }
                }
                step++;
                render_ring(&config, step);
            }
            break;
        }

        /* Execute one step (central daemon: first privileged machine) */
        int32_t i;
        for (i = 0; i < n; i++) {
            if (ring_compute_privilege(&config, i) != PRIVILEGE_NONE) {
                ring_step(&config, i);
                break;
            }
        }

        step++;
        SLEEP_MS(100);
    }

    if (!ring_is_legitimate(&config)) {
        printf("\nDid not converge within %d steps.\n", max_steps);
    }

    ring_destroy(&config);
    return 0;
}
