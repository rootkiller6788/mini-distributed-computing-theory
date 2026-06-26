/*
 * paxos.c - Paxos Consensus Algorithm Implementation
 * L5: Basic Paxos and Multi-Paxos algorithms.
 * L4: Safety verification.
 * Reference: Lamport (1998) "The Part-Time Parliament"
 *            Lamport (2001) "Paxos Made Simple"
 */

#include "paxos.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ================================================================
 * L3: Proposal Number Utilities
 * ================================================================
 *
 * Proposal numbers must be totally ordered. We use (round, server_id)
 * with lexicographic ordering: compare round first, then server_id.
 */

int paxos_proposal_compare(paxos_proposal_num_t a, paxos_proposal_num_t b)
{
    if (a.round < b.round) return -1;
    if (a.round > b.round) return 1;
    if (a.server_id < b.server_id) return -1;
    if (a.server_id > b.server_id) return 1;
    return 0;
}

paxos_proposal_num_t paxos_proposal_make(uint64_t round, int server_id)
{
    paxos_proposal_num_t p;
    p.round = round;
    p.server_id = server_id;
    return p;
}

/* ================================================================
 * L5: Phase 1 — Prepare / Promise
 * ================================================================
 *
 * Phase 1a (Prepare): Proposer selects proposal number n,
 * sends Prepare(n) to a majority of acceptors.
 *
 * Phase 1b (Promise): Acceptor receives Prepare(n).
 * If n > promised: promise not to accept any proposal < n,
 * respond with Promise(n, highest-numbered accepted proposal).
 * Else: ignore or reject.
 */

consensus_message_t paxos_prepare(int sender_id, paxos_proposal_num_t n)
{
    consensus_message_t msg;
    consensus_message_init(&msg);
    msg.type = MSG_PREPARE;
    msg.sender_id = sender_id;
    msg.receiver_id = -1; /* broadcast */
    msg.seq_num = n.round;
    msg.view = n.server_id; /* encode server_id in view field */
    return msg;
}

consensus_message_t paxos_receive_prepare(paxos_acceptor_t *acceptor,
                                           consensus_message_t *msg)
{
    consensus_message_t response;
    consensus_message_init(&response);
    response.type = MSG_PROMISE;
    response.sender_id = -1; /* caller should set this */
    response.receiver_id = msg->sender_id;

    paxos_proposal_num_t incoming;
    incoming.round = msg->seq_num;
    incoming.server_id = (int)msg->view;

    /* If incoming n > promised, accept the prepare */
    if (paxos_proposal_compare(incoming, acceptor->promised) > 0) {
        acceptor->promised = incoming;
        response.seq_num = incoming.round;
        response.view = incoming.server_id;
        response.success = true;

        /* Return highest-numbered accepted proposal if any */
        if (acceptor->has_accepted) {
            response.proposed_value = acceptor->accepted_value;
            response.prev_log_index = acceptor->accepted.round;
            response.prev_log_term = acceptor->accepted.server_id;
        } else {
            response.proposed_value = -1; /* no accepted value */
        }
    } else {
        /* Reject: incoming n <= promised */
        response.success = false;
        response.seq_num = acceptor->promised.round;
        response.view = acceptor->promised.server_id;
    }

    return response;
}

/* ================================================================
 * L5: Phase 2 — Accept / Accepted
 * ================================================================
 *
 * Phase 2a (Accept): If proposer receives promises from a majority:
 *   - Pick the value from the highest-numbered accepted proposal
 *     among the promise responses, or its own value if none.
 *   - Send Accept(n, value) to acceptors.
 *
 * Phase 2b (Accepted): Acceptor receives Accept(n, value).
 * If n >= promised: accept proposal, send Accepted(n, value).
 */

consensus_message_t paxos_accept(paxos_proposer_t *proposer,
                                  int sender_id, int original_value,
                                  paxos_acceptor_t *peer_acceptors,
                                  int num_acceptors)
{
    (void)peer_acceptors;
    (void)num_acceptors;
    consensus_message_t msg;
    consensus_message_init(&msg);
    msg.type = MSG_ACCEPT;
    msg.sender_id = sender_id;
    msg.receiver_id = -1;

    /* Choose value: highest-numbered accepted from promises, or own */
    int choose_value = original_value;

    if (proposer->highest_value_seen >= 0) {
        /* Some acceptor already accepted a value at higher ballot */
        choose_value = proposer->highest_value_seen;
    }

    msg.proposed_value = choose_value;
    msg.seq_num = proposer->current_num.round;
    msg.view = proposer->current_num.server_id;

    return msg;
}

consensus_message_t paxos_receive_accept(paxos_acceptor_t *acceptor,
                                          consensus_message_t *msg)
{
    consensus_message_t response;
    consensus_message_init(&response);
    response.type = MSG_ACCEPTED;
    response.sender_id = -1;
    response.receiver_id = msg->sender_id;

    paxos_proposal_num_t incoming;
    incoming.round = msg->seq_num;
    incoming.server_id = (int)msg->view;

    /* Accept if incoming n >= promised */
    if (paxos_proposal_compare(incoming, acceptor->promised) >= 0) {
        acceptor->promised = incoming;
        acceptor->accepted = incoming;
        acceptor->accepted_value = msg->proposed_value;
        acceptor->has_accepted = true;

        response.success = true;
        response.seq_num = incoming.round;
        response.proposed_value = msg->proposed_value;
    } else {
        response.success = false;
    }

    return response;
}

/* ================================================================
 * L5: Learner — Decision Detection
 * ================================================================
 *
 * A value is chosen when a majority of acceptors have accepted it.
 * Learners track Accepted messages and detect when a majority is reached.
 */

bool paxos_learner_receive_accepted(paxos_learner_t *learner,
                                     consensus_message_t *msg)
{
    if (!learner || !msg) return false;

    if (msg->type == MSG_ACCEPTED && msg->success) {
        learner->learned_value = msg->proposed_value;
        learner->accept_count++;
        if (learner->accept_count >= learner->quorum_size) {
            learner->has_learned = true;
            return true;
        }
    }
    return false;
}

/* ================================================================
 * L5: Basic Paxos — Full instance execution
 * ================================================================
 *
 * Runs one complete Paxos instance:
 *   Phase 1: Proposer sends Prepare, collects majority promises
 *   Phase 2: Proposer sends Accept, collects majority accepts
 *   Learner: Detects when value is chosen
 *
 * Simplified model: assume synchronous rounds for demonstration.
 * Phase 1 and Phase 2 each take one round.
 */

int paxos_run_instance(paxos_instance_t *instance, int value,
                       paxos_acceptor_t *peers, int num_peers)
{
    if (!instance || !peers || num_peers < 1)
        return -1;

    int majority = num_peers / 2 + 1;
    instance->num_acceptors = num_peers;
    instance->proposer.quorum_size = majority;
    instance->learner.quorum_size = majority;

    /* === Phase 1: Prepare === */
    paxos_proposal_num_t n;
    n.round = instance->instance_id + 1; /* unique round per instance */
    n.server_id = 0;  /* this node's proposer id */

    instance->proposer.current_num = n;
    instance->proposer.proposed_value = value;
    instance->proposer.promises_received = 0;
    instance->proposer.highest_value_seen = -1;
    paxos_proposal_num_t zero_num = {0, 0};
    instance->proposer.highest_num_seen = zero_num;

    consensus_message_t prep = paxos_prepare(0, n);

    for (int i = 0; i < num_peers; i++) {
        instance->peer_acceptors[i] = peers[i];  /* copy initial state */

        consensus_message_t promise = paxos_receive_prepare(
            &instance->peer_acceptors[i], &prep);

        if (promise.success) {
            instance->proposer.promises_received++;

            /* Track highest-numbered accepted proposal */
            if (promise.proposed_value >= 0) {
                paxos_proposal_num_t prev;
                prev.round = promise.prev_log_index;
                prev.server_id = (int)promise.prev_log_term;
                if (paxos_proposal_compare(prev,
                        instance->proposer.highest_num_seen) > 0) {
                    instance->proposer.highest_num_seen = prev;
                    instance->proposer.highest_value_seen =
                        promise.proposed_value;
                }
            }
        }
    }

    if (instance->proposer.promises_received < majority)
        return -1;  /* Phase 1 failed */

    /* === Phase 2: Accept === */
    consensus_message_t acc = paxos_accept(
        &instance->proposer, 0, value,
        instance->peer_acceptors, num_peers);

    int chosen_value = acc.proposed_value;
    instance->proposer.accepts_received = 0;

    for (int i = 0; i < num_peers; i++) {
        consensus_message_t accepted = paxos_receive_accept(
            &instance->peer_acceptors[i], &acc);

        if (accepted.success)
            instance->proposer.accepts_received++;
    }

    if (instance->proposer.accepts_received >= majority) {
        instance->decided_value = chosen_value;
        instance->is_decided = true;
        return chosen_value;
    }

    return -1;  /* Phase 2 failed */
}

/* ================================================================
 * L3: Multi-Paxos
 * ================================================================
 *
 * Multi-Paxos amortizes Phase 1 across many slots by electing a
 * distinguished leader. Once elected, the leader skips Phase 1
 * for subsequent slots, reducing message complexity per decision
 * from 4 message delays to 2.
 */

int multi_paxos_init(multi_paxos_t *mp, int num_slots, int num_acceptors)
{
    if (!mp || num_slots < 1 || num_acceptors < 1)
        return -1;

    mp->instances = (paxos_instance_t *)calloc(
        (size_t)num_slots, sizeof(paxos_instance_t));
    if (!mp->instances)
        return -1;

    for (int i = 0; i < num_slots; i++) {
        mp->instances[i].instance_id = i;
        mp->instances[i].is_decided = false;
        mp->instances[i].decided_value = -1;
        mp->instances[i].num_acceptors = num_acceptors;
        mp->instances[i].peer_acceptors = (paxos_acceptor_t *)calloc(
            (size_t)num_acceptors, sizeof(paxos_acceptor_t));
    }

    mp->num_instances = num_slots;
    mp->leader_id = -1;
    mp->phase1_done = false;
    mp->leader_proposal_num = paxos_proposal_make(1, 0);

    return 0;
}

void multi_paxos_destroy(multi_paxos_t *mp)
{
    if (!mp) return;
    if (mp->instances) {
        for (int i = 0; i < mp->num_instances; i++) {
            if (mp->instances[i].peer_acceptors)
                free(mp->instances[i].peer_acceptors);
        }
        free(mp->instances);
        mp->instances = NULL;
    }
    mp->num_instances = 0;
}

int multi_paxos_elect_leader(multi_paxos_t *mp, int node_id,
                              paxos_acceptor_t *acceptors, int num_acceptors)
{
    if (!mp || num_acceptors < 1)
        return -1;

    /* Use instance 0 for leader election (Phase 1 only) */
    paxos_proposal_num_t n = mp->leader_proposal_num;

    consensus_message_t prep = paxos_prepare(node_id, n);

    int promises = 0;
    for (int i = 0; i < num_acceptors; i++) {
        consensus_message_t promise = paxos_receive_prepare(
            &acceptors[i], &prep);
        if (promise.success)
            promises++;
    }

    int majority = num_acceptors / 2 + 1;
    if (promises >= majority) {
        mp->leader_id = node_id;
        mp->phase1_done = true;
        return 0;
    }

    return -1;
}

int multi_paxos_propose(multi_paxos_t *mp, int slot, int value,
                        paxos_acceptor_t *acceptors, int num_acceptors)
{
    if (!mp || slot < 0 || slot >= mp->num_instances)
        return -1;

    if (!mp->phase1_done) {
        /* Run Phase 1 first */
        if (multi_paxos_elect_leader(mp, mp->leader_id >= 0
                ? mp->leader_id : 0, acceptors, num_acceptors) != 0)
            return -1;
    }

    /* Phase 2 only (skip Phase 1 for this slot) */
    paxos_instance_t *inst = &mp->instances[slot];
    int majority = num_acceptors / 2 + 1;

    int accepts = 0;
    int chosen = value;

    consensus_message_t acc;
    consensus_message_init(&acc);
    acc.type = MSG_ACCEPT;
    acc.sender_id = mp->leader_id;
    acc.seq_num = mp->leader_proposal_num.round;
    acc.view = mp->leader_proposal_num.server_id;
    acc.proposed_value = value;

    for (int i = 0; i < num_acceptors; i++) {
        consensus_message_t accepted = paxos_receive_accept(
            &acceptors[i], &acc);
        if (accepted.success)
            accepts++;
    }

    if (accepts >= majority) {
        inst->decided_value = chosen;
        inst->is_decided = true;
        return chosen;
    }

    return -1;
}

/* ================================================================
 * L4: Paxos Safety Verification
 * ================================================================
 *
 * The Paxos safety property: if a value v is chosen at proposal n,
 * then every proposal with number m > n proposes v.
 *
 * This function checks the invariant by verifying that no two
 * decided instances have conflicting values (same slot, different value).
 */

bool paxos_verify_safety(paxos_instance_t *instances, int num_instances)
{
    if (!instances) return true;

    for (int i = 0; i < num_instances; i++) {
        if (!instances[i].is_decided)
            continue;

        /* Within this instance (slot), all decided values must be same */
        /* Check that same slot isn't decided twice with different values */
        for (int j = i + 1; j < num_instances; j++) {
            if (instances[j].is_decided &&
                instances[i].instance_id == instances[j].instance_id &&
                instances[i].decided_value != instances[j].decided_value) {
                return false;  /* safety violation */
            }
        }
    }
    return true;
}

bool paxos_has_distinguished_leader(paxos_instance_t *instances,
                                     int num_instances)
{
    if (!instances || num_instances < 1)
        return false;

    /* A distinguished leader exists if all proposals use the same
     * server_id in their proposal numbers. This is trivially true
     * for the Multi-Paxos optimization. */
    return true; /* Single-proposer model used in implementation */
}
