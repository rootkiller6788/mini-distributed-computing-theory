/**
 * example_bfs.c — BFS using Alpha, Beta, and Gamma synchronizers
 *
 * Demonstrates the Breadth-First Search algorithm running on top of
 * each synchronizer type. Compares message and time complexity.
 */

#include <stdio.h>
#include <stdlib.h>
#include "synchro.h"
#include "distributed_algorithms.h"

int main(void) {
    printf("============================================================\n");
    printf("  Example: Synchronous BFS on α, β, γ Synchronizers\n");
    printf("============================================================\n\n");

    /*----------------------------------------------------------
     * Alpha Synchronizer on 3x3 Grid
     *----------------------------------------------------------*/
    printf("--- Alpha Synchronizer (edge-based) ---\n");
    network_t *net_alpha = network_gen_grid(3, 3, SYNCHRO_ALPHA);
    synchro_alpha_init(net_alpha);
    bfs_reset(net_alpha);
    synchro_reset_counters(net_alpha);

    /* Run BFS until tree is complete */
    int rounds_a = bfs_run(net_alpha, 0);

    printf("  Rounds: %d\n", rounds_a);
    printf("  Messages: %zu\n", synchro_get_message_complexity(net_alpha));
    bfs_print_tree(net_alpha);

    printf("  Theoretical: O(|E|) = O(%d) msg/round\n\n", net_alpha->num_edges);
    network_destroy(net_alpha);

    /*----------------------------------------------------------
     * Beta Synchronizer on 3x3 Grid
     *----------------------------------------------------------*/
    printf("--- Beta Synchronizer (tree-based) ---\n");
    network_t *net_beta = network_gen_grid(3, 3, SYNCHRO_BETA);
    synchro_beta_init(net_beta);
    bfs_reset(net_beta);
    synchro_reset_counters(net_beta);

    int rounds_b = bfs_run(net_beta, 0);

    printf("  Rounds: %d\n", rounds_b);
    printf("  Messages: %zu\n", synchro_get_message_complexity(net_beta));
    bfs_print_tree(net_beta);

    printf("  Theoretical: O(|V|) = O(%d) msg/round\n\n", net_beta->num_nodes);
    network_destroy(net_beta);

    /*----------------------------------------------------------
     * Gamma Synchronizer on 3x3 Grid
     *----------------------------------------------------------*/
    printf("--- Gamma Synchronizer (cluster-based) ---\n");
    network_t *net_gamma = network_gen_grid(3, 3, SYNCHRO_GAMMA);
    synchro_gamma_init(net_gamma, 3);
    bfs_reset(net_gamma);
    synchro_reset_counters(net_gamma);

    int rounds_g = bfs_run(net_gamma, 0);

    printf("  Rounds: %d\n", rounds_g);
    printf("  Messages: %zu\n", synchro_get_message_complexity(net_gamma));
    bfs_print_tree(net_gamma);
    printf("  Clusters: node 0 leader=%d, node 4 leader=%d\n",
           net_gamma->nodes[0].is_cluster_leader,
           net_gamma->nodes[4].cluster_leader);

    printf("  Theoretical: O(k|V|) msg/round (k=cluster size)\n\n");
    network_destroy(net_gamma);

    /*----------------------------------------------------------
     * Complexity Comparison Table
     *----------------------------------------------------------*/
    printf("============================================================\n");
    printf("  Complexity Comparison (Grid 3×3, |V|=9, |E|=12)\n");
    printf("============================================================\n");
    printf("  Synchronizer | Msg/Round Bound | Time/Round Bound\n");
    printf("  -------------+-----------------+-----------------\n");
    printf("  Alpha (α)    | O(|E|) = 12    | O(1)\n");
    printf("  Beta (β)     | O(|V|) = 9     | O(|V|) = 9\n");
    printf("  Gamma (γ)    | O(k|V|) = 27   | O(log_k|V|)\n");
    printf("============================================================\n");

    return 0;
}
