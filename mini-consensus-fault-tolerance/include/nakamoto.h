/*
 * nakamoto.h - Nakamoto Consensus (Bitcoin-style Blockchain)
 * L5: Proof-of-Work consensus for permissionless networks.
 * L7: Bitcoin/blockchain application.
 * L8: Probabilistic finality, selfish mining attack.
 * Reference: Nakamoto (2008), Garay et al. (2015), Pass et al. (2017)
 */
#ifndef NAKAMOTO_H
#define NAKAMOTO_H
#include "consensus_types.h"
#include <stdint.h>
#include <stdbool.h>

#define BLOCK_HASH_SIZE 32
#define BLOCK_DATA_SIZE 1024

typedef struct {
    uint8_t  prev_block_hash[BLOCK_HASH_SIZE];
    uint8_t  merkle_root[BLOCK_HASH_SIZE];
    uint64_t block_height;
    uint64_t timestamp;
    uint64_t nonce;
    uint32_t difficulty_bits;
    uint8_t  block_hash[BLOCK_HASH_SIZE];
    char     data[BLOCK_DATA_SIZE];
    int      data_len;
} nakamoto_block_t;

typedef struct nakamoto_block_node {
    nakamoto_block_t            block;
    struct nakamoto_block_node *next;
    struct nakamoto_block_node *prev;
} nakamoto_block_node_t;

typedef struct {
    nakamoto_block_node_t *genesis;
    nakamoto_block_node_t *tip;
    uint64_t               total_work;
    uint64_t               length;
} nakamoto_blockchain_t;

typedef struct {
    int      node_id;
    double   hash_power;
    bool     is_honest;
    bool     is_selfish_miner;
    nakamoto_blockchain_t chain;
    int      mempool_size;
    uint64_t blocks_mined;
    uint64_t blocks_orphaned;
} nakamoto_node_t;

typedef struct {
    nakamoto_node_t *nodes;
    int              num_nodes;
    double           honest_fraction;
    uint64_t         target_block_time;
    uint32_t         difficulty;
    int              network_delay;
    bool             allow_forks;
} nakamoto_network_t;

int   nakamoto_try_nonce(nakamoto_block_t *block, uint64_t nonce, uint32_t difficulty);
uint64_t nakamoto_mine_block(nakamoto_block_t *block, uint32_t difficulty);
nakamoto_block_t nakamoto_create_block(nakamoto_blockchain_t *chain, const char *data, int data_len, uint32_t difficulty);
int  nakamoto_compare_chains(nakamoto_blockchain_t *chain_a, nakamoto_blockchain_t *chain_b);
bool nakamoto_adopt_chain(nakamoto_node_t *node, nakamoto_blockchain_t *new_chain);
int  nakamoto_add_block(nakamoto_node_t *node, nakamoto_block_t *block);
uint64_t nakamoto_confirmations(nakamoto_blockchain_t *chain, uint64_t height);
double nakamoto_reorg_probability(double attacker_hash_power, int confirmations);
int  nakamoto_confirmations_needed(double attacker_hash_power, double epsilon);
int  nakamoto_selfish_mining_strategy(nakamoto_node_t *attacker, int private_chain_lead, int honest_chain_lead);
double nakamoto_selfish_mining_revenue(double alpha, double gamma);
int  nakamoto_network_init(nakamoto_network_t *net, int num_nodes, double honest_fraction, uint32_t difficulty);
void nakamoto_network_destroy(nakamoto_network_t *net);
int  nakamoto_network_step(nakamoto_network_t *net);
nakamoto_blockchain_t *nakamoto_get_canonical_chain(nakamoto_network_t *net);

#endif /* NAKAMOTO_H */
