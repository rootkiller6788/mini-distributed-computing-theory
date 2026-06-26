/*
 * nakamoto.c - Nakamoto Consensus (Bitcoin-style Blockchain) Implementation
 * L5: Proof-of-Work based consensus for permissionless networks.
 * L7: Bitcoin/blockchain application of consensus.
 * L8: Probabilistic finality, selfish mining attack analysis.
 * Reference: Nakamoto (2008), Garay et al. (2015), Eyal & Sirer (2014)
 */
#include "nakamoto.h"
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* Simple non-cryptographic hash for demonstration.
 * Real PoW uses double SHA-256. */
static uint64_t simple_hash(const uint8_t *data, size_t len, uint64_t nonce)
{
    uint64_t h = 0x1505;
    for (size_t i = 0; i < len; i++) {
        h = (h * 31 + data[i]) ^ (h >> 3);
    }
    h = (h * 1103515245 + nonce) ^ (nonce >> 13);
    h ^= (h << 7) | (h >> 57);
    return h;
}

int nakamoto_try_nonce(nakamoto_block_t *block, uint64_t nonce, uint32_t difficulty)
{
    if (!block) return -1;
    /* In real PoW: target = MAX_TARGET / difficulty, hash < target means success.
     * We simulate: hash mod difficulty == 0 (higher difficulty = harder to find). */
    size_t header_size = sizeof(block->prev_block_hash) + sizeof(block->merkle_root) +
                          sizeof(block->block_height) + sizeof(block->timestamp) + sizeof(nonce);
    uint8_t *header = (uint8_t *)malloc(header_size);
    if (!header) return -1;
    size_t offset = 0;
    memcpy(header + offset, block->prev_block_hash, sizeof(block->prev_block_hash));
    offset += sizeof(block->prev_block_hash);
    memcpy(header + offset, block->merkle_root, sizeof(block->merkle_root));
    offset += sizeof(block->merkle_root);
    memcpy(header + offset, &block->block_height, sizeof(block->block_height));
    offset += sizeof(block->block_height);
    memcpy(header + offset, &block->timestamp, sizeof(block->timestamp));
    offset += sizeof(block->timestamp);
    memcpy(header + offset, &nonce, sizeof(nonce));
    uint64_t hash_val = simple_hash(header, header_size, nonce);
    free(header);
    if (difficulty == 0) return 0;
    if ((hash_val % difficulty) == 0) {
        return 0;  /* success */
    }
    return 1;  /* continue mining */
}

uint64_t nakamoto_mine_block(nakamoto_block_t *block, uint32_t difficulty)
{
    if (!block) return 0;
    uint64_t nonce = 0;
    uint64_t tries = 0;
    /* For demonstration, cap tries to avoid infinite loop */
    uint64_t max_tries = (uint64_t)difficulty * 10;
    if (max_tries < 1000) max_tries = 1000;
    while (tries < max_tries) {
        if (nakamoto_try_nonce(block, nonce, difficulty) == 0) {
            block->nonce = nonce;
            /* Compute block hash */
            uint64_t h = simple_hash((uint8_t *)block, sizeof(*block), nonce);
            memset(block->block_hash, 0, BLOCK_HASH_SIZE);
            memcpy(block->block_hash, &h, sizeof(h) < BLOCK_HASH_SIZE ? sizeof(h) : BLOCK_HASH_SIZE);
            return tries + 1;
        }
        nonce++;
        tries++;
    }
    /* Set nonce anyway for simulation purposes */
    block->nonce = nonce;
    return tries;
}

nakamoto_block_t nakamoto_create_block(nakamoto_blockchain_t *chain,
                                        const char *data, int data_len,
                                        uint32_t difficulty)
{
    nakamoto_block_t block;
    memset(&block, 0, sizeof(block));
    if (chain && chain->tip) {
        memcpy(block.prev_block_hash, chain->tip->block.block_hash, BLOCK_HASH_SIZE);
        block.block_height = chain->tip->block.block_height + 1;
    } else {
        memset(block.prev_block_hash, 0, BLOCK_HASH_SIZE);
        block.block_height = 0;  /* genesis */
    }
    if (data && data_len > 0) {
        int len = data_len < BLOCK_DATA_SIZE ? data_len : BLOCK_DATA_SIZE - 1;
        memcpy(block.data, data, (size_t)len);
        block.data[len] = 0;
        block.data_len = len;
    }
    block.difficulty_bits = difficulty;
    block.timestamp = (uint64_t)time(NULL);
    return block;
}

static uint64_t block_work(nakamoto_block_t *block)
{
    return block->difficulty_bits > 0 ? block->difficulty_bits : 1;
}

int nakamoto_compare_chains(nakamoto_blockchain_t *chain_a, nakamoto_blockchain_t *chain_b)
{
    if (!chain_a && !chain_b) return 0;
    if (!chain_a) return -1;
    if (!chain_b) return 1;
    if (chain_a->total_work > chain_b->total_work) return 1;
    if (chain_a->total_work < chain_b->total_work) return -1;
    /* Tiebreaker: shorter chain wins (less forks) */
    if (chain_a->length < chain_b->length) return 1;
    if (chain_a->length > chain_b->length) return -1;
    return 0;
}

bool nakamoto_adopt_chain(nakamoto_node_t *node, nakamoto_blockchain_t *new_chain)
{
    if (!node || !new_chain) return false;
    int cmp = nakamoto_compare_chains(new_chain, &node->chain);
    if (cmp > 0) {
        /* Reorg: count orphaned blocks */
        uint64_t old_len = node->chain.length;
        node->chain = *new_chain;
        if (node->chain.length < old_len)
            node->blocks_orphaned += (old_len - node->chain.length);
        return true;
    }
    return false;
}

int nakamoto_add_block(nakamoto_node_t *node, nakamoto_block_t *block)
{
    if (!node || !block) return -1;
    /* Check if block extends current tip */
    if (node->chain.tip) {
        if (memcmp(block->prev_block_hash, node->chain.tip->block.block_hash, BLOCK_HASH_SIZE) == 0) {
            /* Extends current tip */
            nakamoto_block_node_t *new_node = (nakamoto_block_node_t *)calloc(1, sizeof(nakamoto_block_node_t));
            if (!new_node) return -1;
            new_node->block = *block;
            new_node->prev = node->chain.tip;
            node->chain.tip->next = new_node;
            node->chain.tip = new_node;
            node->chain.length++;
            node->chain.total_work += block_work(block);
            return 1;  /* extended tip */
        } else {
            /* Fork: block doesn't extend current tip */
            return 0;  /* fork created */
        }
    } else {
        /* Genesis block */
        nakamoto_block_node_t *gen = (nakamoto_block_node_t *)calloc(1, sizeof(nakamoto_block_node_t));
        if (!gen) return -1;
        gen->block = *block;
        node->chain.genesis = gen;
        node->chain.tip = gen;
        node->chain.length = 1;
        node->chain.total_work = block_work(block);
        return 1;
    }
}

uint64_t nakamoto_confirmations(nakamoto_blockchain_t *chain, uint64_t height)
{
    if (!chain || height == 0 || height > chain->length) return 0;
    return chain->length - height;
}

/* L8: Reorg Probability — Gambler's Ruin model
 *
 * Nakamoto (2008), Section 11.
 * For an attacker with hash power fraction q (q < 0.5):
 *   Probability of catching up from z blocks behind =
 *     1 - sum_{k=0}^{z-1} (lambda^k * e^{-lambda} / k!) * (1 - (q/p)^{z-k})
 *   where lambda = z * (q/p).
 * As z grows large, P ~ (q/p)^z (exponential decay).
 */
double nakamoto_reorg_probability(double q, int z)
{
    if (z <= 0 || q <= 0.0 || q >= 0.5) {
        if (q <= 0.0) return 0.0;
        if (q >= 0.5) return 1.0;  /* attacker controls majority */
        if (z <= 0) return 1.0;    /* no confirmations */
    }
    double p = 1.0 - q;
    double ratio = q / p;
    /* For large z, use asymptotic: (q/p)^z */
    if (z > 20) return pow(ratio, (double)z);
    /* Exact computation */
    double lambda = (double)z * ratio;
    double sum = 0.0;
    double poisson_term = exp(-lambda);  /* e^{-lambda} for k=0 */
    for (int k = 0; k < z; k++) {
        double term = poisson_term * (1.0 - pow(ratio, (double)(z - k)));
        sum += term;
        /* Update Poisson term for next k */
        poisson_term *= lambda / (double)(k + 1);
    }
    return 1.0 - sum;
}

int nakamoto_confirmations_needed(double q, double epsilon)
{
    if (q <= 0.0 || epsilon <= 0.0) return 0;
    if (q >= 0.5) return -1;  /* impossible */
    int k = 1;
    while (k < 1000) {
        if (nakamoto_reorg_probability(q, k) < epsilon)
            return k;
        k++;
    }
    return k;
}

/* L8: Selfish Mining Strategy (Eyal & Sirer 2014)
 *
 * The attacker withholds blocks and publishes strategically.
 * Simplified policy:
 *   - Private lead 0, Honest found: Adopt (no advantage)
 *   - Private lead 1, Honest found: Publish 1 block (race)
 *   - Private lead 2+: Honest found: Publish all (overwrite)
 *   - Private lead > 0, Attacker found: Continue withholding
 */
int nakamoto_selfish_mining_strategy(nakamoto_node_t *attacker,
                                      int private_chain_lead,
                                      int honest_chain_lead)
{
    if (!attacker) return 0;
    if (private_chain_lead == 0 && honest_chain_lead >= 0)
        return 0;  /* no lead, adopt honest chain */
    if (private_chain_lead == 1 && honest_chain_lead > 0)
        return 1;  /* publish 1 to race */
    if (private_chain_lead >= 2 && honest_chain_lead > 0)
        return private_chain_lead;  /* publish all to overwrite */
    return 0;  /* withhold */
}

/* Revenue ratio from Eyal & Sirer (2014), equation (7).
 * alpha: attacker's hash power fraction.
 * gamma: fraction of honest miners that adopt attacker's block in tie.
 * Returns: relative revenue (normalized so honest strategy = alpha). */
double nakamoto_selfish_mining_revenue(double alpha, double gamma)
{
    if (alpha <= 0.0 || alpha >= 0.5) {
        if (alpha <= 0.0) return 0.0;
        if (alpha >= 0.5) return 1.0;  /* 51% attack possible */
    }
    double one_minus_alpha = 1.0 - alpha;
    double num = alpha * one_minus_alpha * one_minus_alpha *
                 (4.0 * alpha + gamma * (1.0 - 2.0 * alpha)) -
                 alpha * alpha * alpha;
    double den = 1.0 - alpha * (1.0 + (2.0 - alpha) * alpha);
    if (den <= 0.0) return 1.0;
    return num / den;
}

int nakamoto_network_init(nakamoto_network_t *net, int num_nodes,
                           double honest_fraction, uint32_t difficulty)
{
    if (!net || num_nodes < 2) return -1;
    if (honest_fraction <= 0.5 || honest_fraction > 1.0) return -1;
    net->nodes = (nakamoto_node_t *)calloc((size_t)num_nodes, sizeof(nakamoto_node_t));
    if (!net->nodes) return -1;
    net->num_nodes = num_nodes;
    net->honest_fraction = honest_fraction;
    net->difficulty = difficulty;
    net->target_block_time = 600;
    net->network_delay = 1;
    net->allow_forks = true;
    int honest_count = (int)(honest_fraction * num_nodes);
    for (int i = 0; i < num_nodes; i++) {
        net->nodes[i].node_id = i;
        net->nodes[i].hash_power = 1.0 / num_nodes;
        net->nodes[i].is_honest = (i < honest_count);
        net->nodes[i].is_selfish_miner = false;
        net->nodes[i].blocks_mined = 0;
        net->nodes[i].blocks_orphaned = 0;
        net->nodes[i].mempool_size = 0;
    }
    return 0;
}

void nakamoto_network_destroy(nakamoto_network_t *net)
{
    if (!net || !net->nodes) return;
    /* Free blockchain nodes */
    for (int i = 0; i < net->num_nodes; i++) {
        nakamoto_block_node_t *cur = net->nodes[i].chain.genesis;
        while (cur) {
            nakamoto_block_node_t *next = cur->next;
            free(cur);
            cur = next;
        }
    }
    free(net->nodes);
    net->nodes = NULL;
    net->num_nodes = 0;
}

int nakamoto_network_step(nakamoto_network_t *net)
{
    if (!net || !net->nodes) return -1;
    int blocks_mined = 0;
    for (int i = 0; i < net->num_nodes; i++) {
        nakamoto_node_t *node = &net->nodes[i];
        /* Simple probabilistic mining: each node mines with prob ~ hash_power */
        double mine_chance = node->hash_power;
        if ((double)rand() / RAND_MAX < mine_chance) {
            nakamoto_block_t blk = nakamoto_create_block(&node->chain, "tx", 2, net->difficulty);
            nakamoto_mine_block(&blk, net->difficulty);
            int result = nakamoto_add_block(node, &blk);
            if (result >= 0) {
                node->blocks_mined++;
                blocks_mined++;
            }
        }
    }
    return blocks_mined;
}

nakamoto_blockchain_t *nakamoto_get_canonical_chain(nakamoto_network_t *net)
{
    if (!net || !net->nodes) return NULL;
    /* Find the chain with the most work among honest nodes */
    nakamoto_blockchain_t *best = NULL;
    for (int i = 0; i < net->num_nodes; i++) {
        if (!net->nodes[i].is_honest) continue;
        if (!best || nakamoto_compare_chains(&net->nodes[i].chain, best) > 0)
            best = &net->nodes[i].chain;
    }
    return best;
}
