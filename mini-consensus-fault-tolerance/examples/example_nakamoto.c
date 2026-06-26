/*
 * example_nakamoto.c - Blockchain/Nakamoto consensus demonstration.
 * L7: Blockchain application of consensus (Bitcoin-style).
 *
 * Simulates a small proof-of-work blockchain network with
 * block mining, chain comparison, and fork resolution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "nakamoto.h"

int main(void)
{
    printf("=== Nakamoto Consensus (Blockchain) Demonstration ===\n\n");

    /* Initialize network: 4 nodes, 75% honest */
    int num_nodes = 4;
    double honest_frac = 0.75;
    uint32_t difficulty = 100;

    nakamoto_network_t net;
    if (nakamoto_network_init(&net, num_nodes, honest_frac, difficulty) < 0) {
        printf("Failed to initialize network.\n");
        return 1;
    }
    printf("Network: %d nodes, %.0f%% honest, difficulty=%u\n\n",
           num_nodes, honest_frac * 100, difficulty);

    /* Step 1: Create genesis block */
    printf("--- Step 1: Genesis Block ---\n");
    nakamoto_block_t genesis = nakamoto_create_block(
        NULL, "Genesis Block", 13, difficulty);
    genesis.timestamp = 0;
    uint64_t genesis_tries = nakamoto_mine_block(&genesis, difficulty);
    printf("Genesis mined: nonce=%llu, tries=%llu\n",
           (unsigned long long)genesis.nonce,
           (unsigned long long)genesis_tries);

    /* Add genesis to all nodes */
    for (int i = 0; i < num_nodes; i++) {
        nakamoto_add_block(&net.nodes[i], &genesis);
    }
    printf("Genesis block added to all %d nodes.\n\n", num_nodes);

    /* Step 2: Mine blocks */
    printf("--- Step 2: Mining Blocks ---\n");
    int num_blocks = 5;
    for (int b = 0; b < num_blocks; b++) {
        char data[64];
        snprintf(data, sizeof(data), "Transaction batch #%d: Alice->Bob 10 BTC", b);

        /* Honest nodes mine */
        for (int i = 0; i < num_nodes; i++) {
            if (!net.nodes[i].is_honest) continue;

            nakamoto_block_t blk = nakamoto_create_block(
                &net.nodes[i].chain, data, (int)strlen(data), difficulty);
            nakamoto_mine_block(&blk, difficulty);
            int rc = nakamoto_add_block(&net.nodes[i], &blk);
            if (rc == 1) {
                net.nodes[i].blocks_mined++;
                printf("  Node %d mined block %llu: '%s'\n",
                       i, (unsigned long long)blk.block_height, data);
                break;  /* one block per round */
            }
        }
    }

    /* Step 3: Chain comparison */
    printf("\n--- Step 3: Chain Analysis ---\n");
    for (int i = 0; i < num_nodes; i++) {
        nakamoto_node_t *n = &net.nodes[i];
        printf("  Node %d (%s): height=%llu, work=%llu, mined=%llu\n",
               i, n->is_honest ? "honest" : "Byzantine",
               (unsigned long long)n->chain.length,
               (unsigned long long)n->chain.total_work,
               (unsigned long long)n->blocks_mined);
    }

    /* Step 4: Canonical chain */
    printf("\n--- Step 4: Canonical Chain ---\n");
    nakamoto_blockchain_t *canon = nakamoto_get_canonical_chain(&net);
    if (canon) {
        printf("Canonical chain: %llu blocks, %llu total work\n",
               (unsigned long long)canon->length,
               (unsigned long long)canon->total_work);

        /* Compute confirmations for block at height 2 */
        uint64_t confs = nakamoto_confirmations(canon, 2);
        printf("Confirmations for block at height 2: %llu\n",
               (unsigned long long)confs);
    }

    /* Step 5: Security analysis */
    printf("\n--- Step 5: Security Analysis ---\n");
    double attacker_hash = 1.0 - honest_frac;
    printf("Attacker hash power: %.2f%%\n", attacker_hash * 100);

    for (int k = 1; k <= 6; k++) {
        double prob = nakamoto_reorg_probability(attacker_hash, k);
        printf("  P(reorg with %d confirmations) = %.6f%%\n",
               k, prob * 100);
    }

    int needed = nakamoto_confirmations_needed(attacker_hash, 0.001);
    printf("\nConfirmations needed for <0.1%% reorg risk: %d\n", needed);

    /* Step 6: Selfish mining analysis */
    printf("\n--- Step 6: Selfish Mining Analysis ---\n");
    printf("Eyal & Sirer (2014) analysis:\n");
    for (double alpha = 0.1; alpha < 0.5; alpha += 0.1) {
        double rev = nakamoto_selfish_mining_revenue(alpha, 0.5);
        double honest_rev = alpha;
        printf("  alpha=%.1f: honest_rev=%.3f, selfish_rev=%.3f, gain=%.1f%%\n",
               alpha, honest_rev, rev,
               honest_rev > 0 ? (rev / honest_rev - 1.0) * 100 : 0);
    }

    nakamoto_network_destroy(&net);
    printf("\n=== Nakamoto consensus demo complete ===\n");
    return 0;
}
