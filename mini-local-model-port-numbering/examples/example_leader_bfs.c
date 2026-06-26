/**
 * @example example_leader_bfs.c
 * @brief End-to-end example: Leader election and BFS tree construction.
 *
 * Demonstrates:
 *  - Deterministic leader election (min-UID) in O(diameter) rounds
 *  - BFS tree construction from the elected leader
 *  - Broadcast and convergecast on the BFS tree
 *  - Port numbering for tree-based communication
 *
 * Knowledge: L5-4 (BFS Tree), L5-5 (Leader Election), L2 (Port Numbering)
 *
 * Theorem: In the LOCAL model with unique UIDs, leader election takes
 * O(diameter) deterministic rounds.
 */

#include "local_model.h"
#include "port_numbering.h"
#include "graph_problems.h"
#include "algorithms.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/* Simple sum aggregation function for convergecast */
static uint32_t sum_agg_fn(uint32_t a, uint32_t b) {
    return a + b;
}

int main(void) {
    printf("=== Leader Election + BFS Tree Construction ===\n\n");

    /* Step 1: Create a random tree-like graph */
    /* Build manually to ensure it's connected and interesting */
    local_context_t ctx;
    if (local_make_regular_tree(&ctx, 3, 3) != 0) {
        printf("Error: Failed to create tree\n");
        return 1;
    }

    printf("Graph: regular tree, degree=3, depth=3, |V|=%u\n", ctx.num_nodes);
    printf("  Diameter: %u\n", local_diameter(&ctx));
    printf("  Max degree Δ: %u\n", local_max_degree(&ctx));

    /* Step 2: Leader election */
    printf("\n--- Leader Election ---\n");
    uint32_t leader;
    uint32_t rounds_le = leader_election_deterministic(&ctx, &leader);
    printf("Leader elected: node with UID = %u\n", leader);
    printf("Rounds taken: %u (= diameter)\n", rounds_le);

    /* Find the node index corresponding to the leader UID */
    uint32_t leader_idx = UINT32_MAX;
    for (uint32_t i = 0; i < ctx.num_nodes; i++) {
        if (ctx.nodes[i].uid == leader) {
            leader_idx = i;
            break;
        }
    }

    /* Step 3: BFS tree rooted at the leader */
    printf("\n--- BFS Tree Construction ---\n");
    uint32_t *parent = (uint32_t *)calloc(ctx.num_nodes, sizeof(uint32_t));
    uint32_t *distance = (uint32_t *)calloc(ctx.num_nodes, sizeof(uint32_t));
    if (!parent || !distance) {
        printf("Error: Allocation failed\n");
        local_context_destroy(&ctx);
        return 1;
    }

    uint32_t rounds_bfs = bfs_tree_build(&ctx, leader_idx, parent, distance);
    printf("BFS tree built in %u rounds\n", rounds_bfs);

    printf("\nTree structure (parent → child):\n");
    for (uint32_t i = 0; i < ctx.num_nodes; i++) {
        if (parent[i] == i) {
            printf("  node %2u (UID=%2u): ROOT, depth=0\n", i, ctx.nodes[i].uid);
        } else if (parent[i] < ctx.num_nodes) {
            printf("  node %2u (UID=%2u): parent=%2u (UID=%2u), depth=%u\n",
                   i, ctx.nodes[i].uid, parent[i], ctx.nodes[parent[i]].uid, distance[i]);
        } else {
            printf("  node %2u (UID=%2u): UNREACHABLE\n", i, ctx.nodes[i].uid);
        }
    }

    /* Step 4: Broadcast a value from the leader */
    printf("\n--- Broadcast ---\n");
    uint32_t *broadcast_vals = (uint32_t *)calloc(ctx.num_nodes, sizeof(uint32_t));
    if (!broadcast_vals) {
        free(parent); free(distance);
        local_context_destroy(&ctx);
        return 1;
    }

    uint32_t rounds_bcast = bfs_broadcast(&ctx, parent, 42, broadcast_vals);
    printf("Broadcast value=42 completed in %u rounds\n", rounds_bcast);

    int all_received = 1;
    for (uint32_t i = 0; i < ctx.num_nodes; i++) {
        if (broadcast_vals[i] != 42) {
            printf("  ERROR: node %u didn't receive broadcast!\n", i);
            all_received = 0;
        }
    }
    if (all_received) printf("All nodes received broadcast ✓\n");

    /* Step 5: Convergecast (sum) */
    printf("\n--- Convergecast (Sum) ---\n");
    uint32_t *converge_vals = (uint32_t *)calloc(ctx.num_nodes, sizeof(uint32_t));
    if (!converge_vals) {
        free(parent); free(distance); free(broadcast_vals);
        local_context_destroy(&ctx);
        return 1;
    }

    /* Each node contributes its UID as its value */
    for (uint32_t i = 0; i < ctx.num_nodes; i++) {
        converge_vals[i] = ctx.nodes[i].uid;
    }

    uint32_t rounds_ccast = bfs_convergecast(&ctx, parent, converge_vals, sum_agg_fn);
    (void)rounds_ccast;

    /* The root should now have the sum of all UIDs */
    uint64_t expected_sum = 0;
    for (uint32_t i = 0; i < ctx.num_nodes; i++) {
        expected_sum += ctx.nodes[i].uid;
    }

    printf("Convergecast result at root: %u\n", converge_vals[leader_idx]);
    printf("Expected sum of all UIDs: %llu\n", (unsigned long long)expected_sum);

    /* Cleanup */
    free(converge_vals);
    free(broadcast_vals);
    free(distance);
    free(parent);
    local_context_destroy(&ctx);

    printf("\n=== Done ===\n");
    return 0;
}
