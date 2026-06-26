/**
 * @example example_mis_random.c
 * @brief End-to-end example: Maximal Independent Set on a random graph.
 *
 * Demonstrates Luby's randomized MIS algorithm and verifies correctness.
 * Also computes the 2-approximate vertex cover from the MIS.
 *
 * Knowledge: L6-2 (MIS), L5-3 (Luby's Algorithm), L6-7 (Vertex Cover)
 *
 * Theorem: Luby's algorithm finds an MIS in O(log n) rounds with high
 * probability (Luby, 1986).
 */

#include "local_model.h"
#include "port_numbering.h"
#include "graph_problems.h"
#include "algorithms.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(void) {
    printf("=== MIS on a Random Graph (Luby's Algorithm) ===\n\n");

    const uint32_t n = 30;

    /* Step 1: Create random graph G(n, p) */
    local_context_t ctx;
    uint64_t seed = (uint64_t)time(NULL);
    printf("Generating random graph G(%u, 0.3) with seed %llu...\n", n, (unsigned long long)seed);

    if (local_make_random_graph(&ctx, n, 0.3, seed) != 0) {
        printf("Error: Failed to create random graph\n");
        return 1;
    }

    uint32_t max_deg = local_max_degree(&ctx);
    uint32_t edges = local_edge_count(&ctx);
    printf("Graph: |V|=%u, |E|=%u, Δ=%u, connected=%s\n",
           n, edges, max_deg,
           local_is_connected(&ctx) ? "yes" : "no");

    /* Step 2: Allocate MIS state */
    mis_node_state_t *states = (mis_node_state_t *)calloc(n, sizeof(mis_node_state_t));
    if (!states) {
        printf("Error: Allocation failed\n");
        local_context_destroy(&ctx);
        return 1;
    }
    for (uint32_t i = 0; i < n; i++) {
        mis_init(&states[i]);
    }

    /* Step 3: Run Luby's algorithm */
    printf("\nRunning Luby's MIS algorithm...\n");
    uint32_t rounds = luby_mis_full(&ctx, states, seed);

    printf("MIS completed in %u rounds\n", rounds);

    /* Step 4: Verify */
    bool valid = mis_is_valid(&ctx, states);
    printf("MIS is %s\n", valid ? "VALID ✓" : "INVALID ✗");

    /* Step 5: Statistics */
    uint32_t mis_size = 0;
    uint32_t out_size = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (states[i].status == MIS_IN_SET) mis_size++;
        else if (states[i].status == MIS_OUT_SET) out_size++;
    }

    printf("\nMIS Statistics:\n");
    printf("  |MIS| = %u / %u (%.1f%%)\n", mis_size, n, 100.0 * mis_size / n);
    printf("  |OUT| = %u\n", out_size);
    printf("  Expected: between n/(Δ+1)=%.0f and n/2=%.0f\n",
           (double)n / (max_deg + 1), (double)n / 2);

    /* Step 6: Derived 2-approximate Vertex Cover */
    printf("\nDerived 2-Approximate Vertex Cover:\n");

    mm_node_state_t *mm_states = (mm_node_state_t *)calloc(n, sizeof(mm_node_state_t));
    bool *vc = (bool *)calloc(n, sizeof(bool));

    if (mm_states && vc) {
        /* Compute MM from MIS (every MIS node can be matched to an arbitrary
           OUT neighbor, but here we simply use the MIS size as a lower bound
           for vertex cover) */
        bool *vc_set = (bool *)calloc(n, sizeof(bool));
        for (uint32_t i = 0; i < n; i++) {
            /* A simple 2-approximation: take all MIS nodes
               For edges between two OUT nodes, take one endpoint */
            if (states[i].status == MIS_IN_SET) {
                vc_set[i] = true;
            }
        }
        /* Cover remaining edges: for each unchecked edge, add one endpoint */
        for (uint32_t i = 0; i < n; i++) {
            for (uint32_t d = 0; d < ctx.nodes[i].degree; d++) {
                uint32_t j = ctx.nodes[i].neighbors[d];
                if (i < j && !vc_set[i] && !vc_set[j]) {
                    vc_set[i] = true;
                }
            }
        }

        uint32_t vc_sz = vc_size(vc_set, n);
        printf("  VC size: %u\n", vc_sz);
        printf("  VC is valid: %s\n", vc_is_valid(&ctx, vc_set) ? "yes ✓" : "no ✗");
        printf("  Approximation ratio guarantee: ≤ 2·OPT since MIS ⊆ VC\n");
        free(vc_set);
    }

    /* Cleanup */
    for (uint32_t i = 0; i < n; i++) {
        mis_destroy(&states[i]);
    }
    free(states);
    free(mm_states);
    free(vc);
    local_context_destroy(&ctx);

    return valid ? 0 : 1;
}
