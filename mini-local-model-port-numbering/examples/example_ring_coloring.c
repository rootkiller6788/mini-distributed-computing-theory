/**
 * @example example_ring_coloring.c
 * @brief End-to-end example: 3-coloring a ring using Cole-Vishkin algorithm.
 *
 * This example demonstrates:
 *  - Creating a ring graph C_n
 *  - Assigning port numberings
 *  - Running the Cole-Vishkin algorithm to 3-color the ring
 *  - Verifying correctness
 *
 * Knowledge: L6-4 (3-Coloring a Ring), L5-1 (Cole-Vishkin Reduction)
 *
 * Theorem verified: A ring C_n can be 3-colored in O(log* n) rounds
 * deterministically (Cole & Vishkin, 1986).
 */

#include "local_model.h"
#include "port_numbering.h"
#include "graph_problems.h"
#include "algorithms.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("=== 3-Coloring a Ring with Cole-Vishkin Algorithm ===\n\n");

    const uint32_t n = 20;

    /* Step 1: Create a ring C_n */
    local_context_t ctx;
    if (local_make_cycle(&ctx, n) != 0) {
        printf("Error: Failed to create ring C_%u\n", n);
        return 1;
    }

    printf("Created ring C_%u with %u nodes, diameter = %u\n",
           n, ctx.num_nodes, local_diameter(&ctx));

    /* Step 2: Assign ring orientation port numbering */
    port_numbering_t *pns = (port_numbering_t *)calloc(n, sizeof(port_numbering_t));
    if (!pns) {
        printf("Error: Allocation failed\n");
        local_context_destroy(&ctx);
        return 1;
    }
    port_assign_ring_orientation(pns, n, ctx.adjacency);

    printf("Port numbering: ring orientation (port 0 = clockwise)\n");

    /* Step 3: Run Cole-Vishkin algorithm */
    uint32_t *colors = (uint32_t *)calloc(n, sizeof(uint32_t));
    if (!colors) {
        printf("Error: Allocation failed\n");
        free(pns);
        local_context_destroy(&ctx);
        return 1;
    }

    /* Initialize with UIDs */
    for (uint32_t i = 0; i < n; i++) {
        colors[i] = ctx.nodes[i].uid;
    }
    printf("Initial coloring: UID-based (|C| = %u distinct colors)\n", n);

    uint32_t rounds = ring_3color_cole_vishkin(&ctx, colors, pns);
    printf("Cole-Vishkin completed in %u rounds\n", rounds);

    /* Step 4: Verify and display */
    bool valid = ring_3color_verify(&ctx, colors);
    printf("3-coloring is %s\n", valid ? "VALID ✓" : "INVALID ✗");

    printf("\nNode colors:\n");
    for (uint32_t i = 0; i < n; i++) {
        printf("  node %2u (UID=%u): color %u\n", i, ctx.nodes[i].uid, colors[i]);
    }

    /* Step 5: Theoretically compare with log* n */
    uint32_t logstar = iterated_log2(n);
    printf("\nTheoretical bound: O(log* n) rounds\n");
    printf("  log* (%u) = %u\n", n, logstar);
    printf("  Actual rounds: %u\n", rounds);
    printf("  Ratio: %.2f\n", (double)rounds / (double)(logstar > 0 ? logstar : 1));

    /* Cleanup */
    free(colors);
    free(pns);
    local_context_destroy(&ctx);

    return valid ? 0 : 1;
}
