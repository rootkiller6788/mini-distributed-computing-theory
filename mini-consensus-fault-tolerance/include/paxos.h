/*
 * paxos.h �� Paxos Consensus Algorithm (Lamport 1998, 2001)
 *
 * L5: Basic Paxos (Synod) and Multi-Paxos algorithms.
 * L4: Safety proof �� only a chosen value can be learned.
 * L6: Consensus problem solved for crash-fault model (n > 2f).
 */

#ifndef PAXOS_H
#define PAXOS_H

#include "consensus_types.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    PAXOS_ROLE_PROPOSER = 0,
    PAXOS_ROLE_ACCEPTOR = 1,
    PAXOS_ROLE_LEARNER  = 2
} paxos_role_t;

typedef struct {
    uint64_t round;
    int      server_id;
} paxos_proposal_num_t;

typedef struct {
    paxos_proposal_num_t promised;
    paxos_proposal_num_t accepted;
    int                   accepted_value;
    bool                  has_accepted;
} paxos_acceptor_t;

typedef struct {
    paxos_proposal_num_t current_num;
    int                   proposed_value;
    int                   highest_value_seen;
    paxos_proposal_num_t  highest_num_seen;
    int                   promises_received;
    int                   accepts_received;
    int                   quorum_size;
} paxos_proposer_t;

typedef struct {
    int  learned_value;
    bool has_learned;
    int  accept_count;
    int  quorum_size;
} paxos_learner_t;

typedef struct {
    int               instance_id;
    paxos_proposer_t  proposer;
    paxos_acceptor_t  acceptor;
    paxos_learner_t   learner;
    int               decided_value;
    bool              is_decided;
    int               num_acceptors;
    paxos_acceptor_t *peer_acceptors;
} paxos_instance_t;

consensus_message_t paxos_prepare(int sender_id, paxos_proposal_num_t n);
consensus_message_t paxos_receive_prepare(paxos_acceptor_t *acceptor, consensus_message_t *msg);
consensus_message_t paxos_accept(paxos_proposer_t *proposer, int sender_id, int original_value,
                                  paxos_acceptor_t *peer_acceptors, int num_acceptors);
consensus_message_t paxos_receive_accept(paxos_acceptor_t *acceptor, consensus_message_t *msg);
bool paxos_learner_receive_accepted(paxos_learner_t *learner, consensus_message_t *msg);
int  paxos_run_instance(paxos_instance_t *instance, int value,
                        paxos_acceptor_t *peers, int num_peers);

typedef struct {
    paxos_instance_t *instances;
    int               num_instances;
    int               leader_id;
    bool              phase1_done;
    paxos_proposal_num_t leader_proposal_num;
} multi_paxos_t;

int  multi_paxos_init(multi_paxos_t *mp, int num_slots, int num_acceptors);
void multi_paxos_destroy(multi_paxos_t *mp);
int  multi_paxos_elect_leader(multi_paxos_t *mp, int node_id,
                               paxos_acceptor_t *acceptors, int num_acceptors);
int  multi_paxos_propose(multi_paxos_t *mp, int slot, int value,
                         paxos_acceptor_t *acceptors, int num_acceptors);
bool paxos_verify_safety(paxos_instance_t *instances, int num_instances);
bool paxos_has_distinguished_leader(paxos_instance_t *instances, int num_instances);

#endif /* PAXOS_H */
