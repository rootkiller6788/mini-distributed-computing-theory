/**
 * paxos_basic.c — Basic Paxos (Synod) Protocol Implementation
 *
 * Implements the two-phase Paxos consensus protocol as described in
 * Lamport's "Paxos Made Simple" (2001). This is the Synod protocol
 * — the core consensus primitive.
 *
 * The protocol operates in two phases, each with two sub-phases:
 *
 *   Phase 1a (Prepare):
 *     Proposer selects a ballot number n and sends Prepare(n) to
 *     a majority of acceptors.
 *
 *   Phase 1b (Promise):
 *     If acceptor receives Prepare(n) with n > promise_ballot:
 *       promise_ballot ← n
 *       responds with Promise(n, accepted_ballot, accepted_value)
 *     Else: ignores (or responds with Nack).
 *
 *   Phase 2a (Accept):
 *     If proposer receives Promises from a majority:
 *       Let V = highest-numbered accepted value among promises.
 *       If no value was accepted, proposer chooses its own value.
 *       Sends Accept(n, v) to all acceptors.
 *
 *   Phase 2b (Accepted):
 *     If acceptor receives Accept(n, v) with n ≥ promise_ballot:
 *       accepted_ballot ← n, accepted_value ← v
 *       Sends Accepted(n, v) to all learners.
 *
 *   Learner:
 *     If a learner receives Accepted(n, v) from a majority of
 *     acceptors, v is chosen (decided).
 *
 * Knowledge Coverage:
 *   L1 Definitions: two-phase protocol, ballot, promise, accept
 *   L2 Core Concepts: majority-based consensus, crash-fault tolerance
 *   L4 Fundamental Laws: induction on ballot numbers
 *   L5 Algorithms: Basic Paxos state machine
 *
 * References:
 *   - Lamport, "Paxos Made Simple" (ACM SIGACT News, 2001)
 *   - Lamport, "The Part-Time Parliament" (ACM TOCS, 1998)
 */

#include "../include/paxos_raft_types.h"
#include "../include/paxos_core.h"
#include "../include/consensus_model.h"
#include <string.h>
#include <stdio.h>

/* ─── Proposer Implementation ──────────────────────────────────────── */

void paxos_proposer_init(paxos_proposer_t *p, node_id_t id) {
    p->node_id = id;
    p->current_ballot = 0;
    memset(&p->proposed_value, 0, sizeof(p->proposed_value));
    p->phase = 0;
}

/**
 * Phase 1a: Prepare.
 *
 * The proposer generates a unique ballot number. Convention: ballot =
 * (round_number * max_nodes) + node_id ensures uniqueness across proposers.
 *
 * Generates Prepare messages to all acceptors.
 */
int paxos_prepare(paxos_proposer_t *proposer, ballot_t ballot,
                   int num_acceptors, const node_id_t *acceptor_ids,
                   paxos_message_t *out_msgs) {
    if (ballot <= proposer->current_ballot) {
        /* Ballot must be strictly greater than any previously used */
        return 0;
    }

    proposer->current_ballot = ballot;
    proposer->phase = 1;

    for (int i = 0; i < num_acceptors; i++) {
        out_msgs[i].type   = PAXOS_MSG_PREPARE;
        out_msgs[i].from   = proposer->node_id;
        out_msgs[i].to     = acceptor_ids[i];
        out_msgs[i].ballot = ballot;
    }

    return num_acceptors;
}

/**
 * Phase 1b: Promise.
 *
 * Acceptor receives Prepare(n):
 *   - If n > promise_ballot: accept the prepare, respond with promise
 *     containing the highest-numbered proposal already accepted.
 *   - Otherwise: ignore (implicit Nack).
 *
 * Invariant maintained: promise_ballot monotonically increases.
 */
bool paxos_receive_prepare(paxos_acceptor_t *acceptor,
                            const paxos_message_t *msg,
                            paxos_message_t *out_msg) {
    if (msg->type != PAXOS_MSG_PREPARE) return false;
    if (msg->ballot <= acceptor->promise_ballot) {
        /* Ignore: already promised a higher ballot.
         * In practice, a Nack could be sent to help the proposer
         * learn of the higher ballot. */
        return false;
    }

    /* Update promise_ballot (monotonically increases) */
    acceptor->promise_ballot = msg->ballot;

    /* Prepare Promise response */
    out_msg->type            = PAXOS_MSG_PROMISE;
    out_msg->from            = acceptor->node_id;
    out_msg->to              = msg->from;
    out_msg->ballot          = msg->ballot;

    if (acceptor->has_accepted) {
        out_msg->accepted_ballot = acceptor->accepted_ballot;
        out_msg->accepted_value  = acceptor->accepted_value;
    } else {
        out_msg->accepted_ballot = 0;
        memset(&out_msg->accepted_value, 0, sizeof(consensus_value_t));
    }

    return true;
}

/**
 * Phase 2a: Accept.
 *
 * After receiving Promises from a majority, the proposer must select
 * a value v according to the rule:
 *
 *   If any Promise carries a previously accepted value, choose the
 *   value associated with the highest accepted_ballot among them.
 *   Otherwise, choose the proposer's own proposed value.
 *
 * This rule is the crux of Paxos safety — it ensures that if any value
 * was previously chosen, the same value will be chosen in any future ballot.
 */
int paxos_accept(paxos_proposer_t *proposer, int num_promises,
                  const paxos_message_t *promises,
                  const consensus_value_t *proposed_value,
                  int num_acceptors, const node_id_t *acceptor_ids,
                  paxos_message_t *out_msgs) {
    if (num_promises <= 0) return 0;

    /* Determine the value to propose:
     * Find the promise with the highest accepted_ballot. */
    ballot_t highest_accepted_ballot = 0;
    int highest_idx = -1;

    for (int i = 0; i < num_promises; i++) {
        if (promises[i].accepted_ballot > highest_accepted_ballot) {
            highest_accepted_ballot = promises[i].accepted_ballot;
            highest_idx = i;
        }
    }

    consensus_value_t value_to_propose;
    if (highest_idx >= 0 && highest_accepted_ballot > 0) {
        /* Use the value from the highest-numbered accepted proposal */
        value_to_propose = promises[highest_idx].accepted_value;
    } else {
        /* No value previously accepted; proposer chooses its own */
        value_to_propose = *proposed_value;
    }

    proposer->proposed_value = value_to_propose;
    proposer->phase = 2;

    /* Send Accept messages to all acceptors */
    for (int i = 0; i < num_acceptors; i++) {
        out_msgs[i].type   = PAXOS_MSG_ACCEPT;
        out_msgs[i].from   = proposer->node_id;
        out_msgs[i].to     = acceptor_ids[i];
        out_msgs[i].ballot = proposer->current_ballot;
        out_msgs[i].value  = value_to_propose;
    }

    return num_acceptors;
}

/**
 * Phase 2b: Accepted.
 *
 * Acceptor receives Accept(n, v):
 *   - If n ≥ promise_ballot: accept the proposal:
 *       accepted_ballot ← n, accepted_value ← v
 *       Send Accepted(n, v) to all learners.
 *   - Otherwise: ignore.
 */
int paxos_receive_accept(paxos_acceptor_t *acceptor,
                          const paxos_message_t *msg,
                          int num_learners, const node_id_t *learner_ids,
                          paxos_message_t *out_msgs) {
    if (msg->type != PAXOS_MSG_ACCEPT) return 0;
    if (msg->ballot < acceptor->promise_ballot) {
        /* Reject: we already promised a higher ballot */
        return 0;
    }

    /* Accept the proposal */
    acceptor->accepted_ballot = msg->ballot;
    acceptor->accepted_value  = msg->value;
    acceptor->has_accepted    = true;

    /* Notify all learners */
    for (int i = 0; i < num_learners; i++) {
        out_msgs[i].type   = PAXOS_MSG_ACCEPTED;
        out_msgs[i].from   = acceptor->node_id;
        out_msgs[i].to     = learner_ids[i];
        out_msgs[i].ballot = msg->ballot;
        out_msgs[i].value  = msg->value;
    }

    return num_learners;
}

/* ─── Learner Implementation ───────────────────────────────────────── */

/**
 * Learner receives Accepted messages.
 *
 * A value is chosen (decided) when a majority of acceptors have sent
 * Accepted messages for the same proposal (ballot, value).
 *
 * For simplicity, this implementation tracks counts per value rather
 * than per (ballot, value) pair. In the full protocol, only one ballot
 * can achieve a majority per instance.
 *
 * Theorem (Paxos Safety): If a learner learns v, no learner can ever
 * learn v' ≠ v.
 */
bool paxos_learner_receive(paxos_learner_t *learner,
                            const paxos_message_t *msg,
                            int num_acceptors) {
    /* In a full implementation, the learner would track Accepted messages
     * from individual acceptors. Here we use a simplified check: the
     * caller is responsible for counting acceptors. */

    if (learner->has_learned) {
        /* Already learned — check consistency */
        if (msg->value.length != learner->learned_value.length ||
            memcmp(msg->value.data, learner->learned_value.data,
                   msg->value.length) != 0) {
            /* Safety violation: conflicting values learned */
            return false;
        }
        return true;
    }

    /* First value learned */
    learner->learned_value = msg->value;
    learner->has_learned = true;
    return true;
}

/* ─── Complete Paxos Instance ──────────────────────────────────────── */

/**
 * Run a complete instance of Basic Paxos.
 *
 * This function simulates the entire protocol execution for a single
 * consensus instance. The proposer, acceptors, and learners interact
 * via message passing.
 *
 * For simplicity, we assume:
 *   - No message loss (synchronous execution)
 *   - All nodes are correct
 *   - A single proposer (no contention)
 *
 * The function goes through:
 *   1. Phase 1a: Proposer → Prepare → Acceptors
 *   2. Phase 1b: Acceptors → Promise → Proposer
 *   3. Phase 2a: Proposer → Accept → Acceptors
 *   4. Phase 2b: Acceptors → Accepted → Learners
 *   5. Learner detects majority → Value is chosen
 */
bool paxos_run_instance(node_id_t proposer_id,
                         const consensus_value_t *value,
                         int num_acceptors,
                         consensus_value_t *result) {
    if (num_acceptors <= 0 || num_acceptors > MAX_NODES) return false;

    paxos_proposer_t proposer;
    paxos_acceptor_t acceptors[MAX_NODES];
    paxos_learner_t learners[MAX_NODES];
    paxos_message_t msgs[MAX_NODES];
    paxos_message_t promises[MAX_NODES];

    /* Initialize */
    paxos_proposer_init(&proposer, proposer_id);

    node_id_t acceptor_ids[MAX_NODES];
    node_id_t learner_ids[MAX_NODES];
    for (int i = 0; i < num_acceptors; i++) {
        acceptor_ids[i] = (node_id_t)(i + 1);
        acceptors[i].node_id = acceptor_ids[i];
        acceptors[i].promise_ballot = 0;
        acceptors[i].accepted_ballot = 0;
        acceptors[i].has_accepted = false;

        learner_ids[i] = (node_id_t)(num_acceptors + i + 1);
        learners[i].node_id = learner_ids[i];
        learners[i].has_learned = false;
    }

    /* Phase 1a: Prepare */
    ballot_t ballot = (ballot_t)(1 * MAX_NODES + proposer_id);
    int num_prepare = paxos_prepare(&proposer, ballot,
                                     num_acceptors, acceptor_ids, msgs);
    if (num_prepare != num_acceptors) return false;

    /* Phase 1b: Process Prepare → Promise */
    int num_promises = 0;
    for (int i = 0; i < num_prepare; i++) {
        paxos_message_t response;
        if (paxos_receive_prepare(&acceptors[msgs[i].to - 1],
                                   &msgs[i], &response)) {
            promises[num_promises++] = response;
        }
    }

    /* Check if majority promised */
    int majority = num_acceptors / 2 + 1;
    if (num_promises < majority) return false;

    /* Phase 2a: Accept */
    int num_accept = paxos_accept(&proposer, num_promises, promises,
                                   value, num_acceptors, acceptor_ids, msgs);
    if (num_accept != num_acceptors) return false;

    /* Phase 2b: Process Accept → Accepted */
    int accepted_count = 0;
    for (int i = 0; i < num_accept; i++) {
        paxos_message_t accepted_msgs[MAX_NODES];
        int n = paxos_receive_accept(&acceptors[msgs[i].to - 1],
                                      &msgs[i], num_acceptors,
                                      learner_ids, accepted_msgs);
        accepted_count += n;
    }

    /* Learner: check if value chosen */
    if (accepted_count >= majority * num_acceptors) {
        /* A value has been chosen — extract from any acceptor */
        for (int i = 0; i < num_acceptors; i++) {
            if (acceptors[i].has_accepted &&
                acceptors[i].accepted_ballot == ballot) {
                *result = acceptors[i].accepted_value;
                return true;
            }
        }
    }

    return false;
}

/* ─── Paxos Safety Invariant Check ─────────────────────────────────── */

/**
 * Verify the Paxos safety invariant on a set of acceptor states.
 *
 * Invariant: If any two acceptors have accepted proposals with the
 * same ballot number, they must have accepted the same value.
 *
 * More generally: For any ballots m < n, if a value v is chosen at
 * ballot m, then only v can be proposed at ballot n.
 *
 * This function checks the simpler condition on the current state.
 */
bool paxos_verify_safety_invariant(const paxos_acceptor_t *acceptors,
                                     int num_acceptors) {
    /* Check: for any two acceptors that accepted at the same ballot,
     * the values must agree. */
    for (int i = 0; i < num_acceptors; i++) {
        if (!acceptors[i].has_accepted) continue;
        for (int j = i + 1; j < num_acceptors; j++) {
            if (!acceptors[j].has_accepted) continue;
            if (acceptors[i].accepted_ballot == acceptors[j].accepted_ballot) {
                if (acceptors[i].accepted_value.length !=
                    acceptors[j].accepted_value.length ||
                    memcmp(acceptors[i].accepted_value.data,
                           acceptors[j].accepted_value.data,
                           acceptors[i].accepted_value.length) != 0) {
                    return false; /* Safety violation! */
                }
            }
        }
    }
    return true;
}

/**
 * Leader completeness lemma for Paxos.
 *
 * If a value v is chosen at ballot n, then for any ballot m > n,
 * the proposer of m must propose v.
 *
 * This is enforced by the Phase 2a value selection rule.
 */
bool paxos_leader_completeness_holds(const paxos_acceptor_t *acceptors,
                                       int num_acceptors,
                                       ballot_t highest_chosen) {
    /* Find the highest ballot for which any acceptor has accepted */
    ballot_t max_accepted = 0;
    for (int i = 0; i < num_acceptors; i++) {
        if (acceptors[i].has_accepted &&
            acceptors[i].accepted_ballot > max_accepted) {
            max_accepted = acceptors[i].accepted_ballot;
        }
    }

    /* If no value was ever chosen, leader completeness vacuously holds */
    if (highest_chosen == 0) return true;

    /* Check: all accepted values at ballots ≥ highest_chosen are equal */
    consensus_value_t chosen_value;
    bool found = false;
    for (int i = 0; i < num_acceptors; i++) {
        if (acceptors[i].has_accepted &&
            acceptors[i].accepted_ballot == highest_chosen) {
            chosen_value = acceptors[i].accepted_value;
            found = true;
            break;
        }
    }
    if (!found) return true; /* chosen ballot not in this set */

    /* Verify all later acceptances have the same value */
    for (int i = 0; i < num_acceptors; i++) {
        if (acceptors[i].has_accepted &&
            acceptors[i].accepted_ballot > highest_chosen) {
            if (acceptors[i].accepted_value.length != chosen_value.length ||
                memcmp(acceptors[i].accepted_value.data,
                       chosen_value.data,
                       chosen_value.length) != 0) {
                return false;
            }
        }
    }
    (void)num_acceptors;
    (void)max_accepted;
    return true;
}
