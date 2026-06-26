/**
 * example_leader_election.c — Leader Election on Ring using Synchronizers
 *
 * Demonstrates the LeLann-Chang-Roberts leader election algorithm
 * executed on top of Alpha, Beta, and Gamma synchronizers on ring networks.
 */

#include <stdio.h>
#include <stdlib.h>
#include "synchro.h"
#include "distributed_algorithms.h"

int main(void) {
    printf("============================================================\n");
    printf("  Example: Leader Election on α, β, γ Synchronizers\n");
    printf("  Algorithm: LeLann-Chang-Roberts (Ring)\n");
    printf("============================================================\n\n");

    const int n = 8; /* Ring of 8 nodes */

    /*----------------------------------------------------------
     * Alpha Synchronizer
     *----------------------------------------------------------*/
    printf("--- Alpha Synchronizer on C%d ---\n", n);
    network_t *net_a = network_gen_cycle(n, SYNCHRO_ALPHA);
    synchro_alpha_init(net_a);
    synchro_reset_counters(net_a);

    int rounds_a = leader_election_run(net_a);
    printf("  Rounds to elect leader: %d\n", rounds_a);
    printf("  Messages exchanged: %zu\n", synchro_get_message_complexity(net_a));
    leader_election_print_result(net_a);
    network_destroy(net_a);

    /*----------------------------------------------------------
     * Beta Synchronizer
     *----------------------------------------------------------*/
    printf("\n--- Beta Synchronizer on C%d ---\n", n);
    network_t *net_b = network_gen_cycle(n, SYNCHRO_BETA);
    synchro_beta_init(net_b);
    synchro_reset_counters(net_b);

    int rounds_b = leader_election_run(net_b);
    printf("  Rounds to elect leader: %d\n", rounds_b);
    printf("  Messages exchanged: %zu\n", synchro_get_message_complexity(net_b));
    leader_election_print_result(net_b);
    network_destroy(net_b);

    /*----------------------------------------------------------
     * Gamma Synchronizer
     *----------------------------------------------------------*/
    printf("\n--- Gamma Synchronizer on C%d ---\n", n);
    network_t *net_g = network_gen_cycle(n, SYNCHRO_GAMMA);
    synchro_gamma_init(net_g, 3);
    synchro_reset_counters(net_g);

    int rounds_g = leader_election_run(net_g);
    printf("  Rounds to elect leader: %d\n", rounds_g);
    printf("  Messages exchanged: %zu\n", synchro_get_message_complexity(net_g));
    leader_election_print_result(net_g);
    printf("  Clusters formed: ");
    node_id_t last_cluster = NODE_ID_NONE;
    int num_clusters = 0;
    for (int i = 0; i < n; i++) {
        if (net_g->nodes[i].cluster_id != last_cluster) {
            num_clusters++;
            printf("%d ", net_g->nodes[i].cluster_id);
            last_cluster = net_g->nodes[i].cluster_id;
        }
    }
    printf("(total=%d)\n", num_clusters);
    network_destroy(net_g);

    /*----------------------------------------------------------
     * Theoretical Analysis
     *----------------------------------------------------------*/
    printf("\n============================================================\n");
    printf("  Theoretical Analysis\n");
    printf("============================================================\n");
    printf("  LeLann-Chang-Roberts on ring of n=%d nodes:\n", n);
    printf("    Message complexity: O(n^2) worst case\n");
    printf("    Synchronizer overhead per round:\n");
    printf("      Alpha:  +2|E| = %d messages\n", 2*n);
    printf("      Beta:   +2|V| = %d messages\n", 2*n);
    printf("      Gamma:  +O(k|V|) messages with cluster size k\n");
    printf("============================================================\n");

    return 0;
}
