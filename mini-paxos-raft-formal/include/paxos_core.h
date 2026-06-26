/**
 * paxos_core.h — Paxos Consensus Algorithm
 *
 * Implements the Paxos protocol as described in Lamport's "Paxos Made
 * Simple" (2001) and "The Part-Time Parliament" (1998). Covers both
 * Basic Paxos (Synod) and Multi-Paxos optimizations.
 *
 * The protocol consists of two phases:
 *   Phase 1 (Prepare):  Proposer chooses ballot n, sends Prepare(n).
 *                       Acceptor, if n > its promise_ballot, responds
 *                       with Promise(n, accepted_ballot, accepted_value).
 *   Phase 2 (Accept):   If Proposer receives Promises from a majority,
 *                       it sends Accept(n, v) where v is either the value
 *                       from the highest-numbered accepted proposal among
 *                       the Promise responses, or any value.
 *                       Acceptor, if n ≥ promise_ballot, accepts.
 *
 * Safety Theorem (Lamport):
 *   If a value v is chosen at ballot n, then for any ballot m > n,
 *   the value proposed at ballot m is also v.
 *
 * Knowledge Coverage:
 *   L1 Definitions: proposer, acceptor, learner, ballot, chosen value
 *   L2 Core Concepts: two-phase commit, majority quorum
 *   L4 Fundamental Laws: Paxos safety theorem
 *   L5 Algorithms: Basic Paxos, Multi-Paxos
 */

#ifndef PAXOS_CORE_H
#define PAXOS_CORE_H

#include "paxos_raft_types.h"
#include <stdbool.h>

/* ─── Proposer Operations ──────────────────────────────────────────── */

/**
 * L5: Initialize a proposer with a unique node ID.
 */
void paxos_proposer_init(paxos_proposer_t *p, node_id_t id);

/**
 * L5 Phase 1a: Prepare.
 * Proposer selects ballot n = (round, node_id) to ensure uniqueness,
 * then sends Prepare(n) to all acceptors.
 *
 * @param proposer  proposer state
 * @param ballot    the proposal number (must be > any previously used)
 * @param num_acceptors  number of acceptors to prepare
 * @param out_msgs  output array of PREPARE messages
 * @return number of messages generated
 */
int paxos_prepare(paxos_proposer_t *proposer, ballot_t ballot,
                   int num_acceptors, const node_id_t *acceptor_ids,
                   paxos_message_t *out_msgs);

/**
 * L5 Phase 1b: Promise.
 * Acceptor receives Prepare(n). If n > promise_ballot, it updates
 * promise_ballot = n and responds with Promise(n, accepted_ballot,
 * accepted_value).
 *
 * @param acceptor  acceptor state
 * @param msg       incoming PREPARE message
 * @param out_msg   output PROMISE message (if applicable)
 * @return true if acceptor responds with a promise
 *
 * Lemma: promise_ballot monotonically increases.
 */
bool paxos_receive_prepare(paxos_acceptor_t *acceptor,
                            const paxos_message_t *msg,
                            paxos_message_t *out_msg);

/**
 * L5 Phase 2a: Accept.
 * After receiving Promises from a majority, the proposer selects
 * value v = value of highest-numbered accepted proposal among promises,
 * or its own proposed value if none accepted. Sends Accept(n, v).
 *
 * Value selection rule:
 *   Let P be the set of Promise responses. If any response contains
 *   (accepted_ballot, accepted_value), pick the value associated with
 *   the maximum accepted_ballot. Otherwise, use own proposed_value.
 *
 * @param proposer     proposer state
 * @param num_promises number of Promise messages received
 * @param promises      array of Promise messages
 * @param proposed_value proposer's proposed value
 * @param num_acceptors number of acceptors to send Accept to
 * @param out_msgs      output ACCEPT messages
 * @return number of messages generated
 */
int paxos_accept(paxos_proposer_t *proposer, int num_promises,
                  const paxos_message_t *promises,
                  const consensus_value_t *proposed_value,
                  int num_acceptors, const node_id_t *acceptor_ids,
                  paxos_message_t *out_msgs);

/**
 * L5 Phase 2b: Accepted.
 * Acceptor receives Accept(n, v). If n ≥ promise_ballot, it accepts:
 *   accepted_ballot = n, accepted_value = v.
 * Sends Accepted(n, v) to all learners.
 *
 * @param acceptor  acceptor state
 * @param msg       incoming ACCEPT message
 * @param num_learners number of learners
 * @param out_msgs   output ACCEPTED messages
 * @return number of messages generated
 */
int paxos_receive_accept(paxos_acceptor_t *acceptor,
                          const paxos_message_t *msg,
                          int num_learners, const node_id_t *learner_ids,
                          paxos_message_t *out_msgs);

/**
 * L5: Learner receives Accepted(n, v). Once a majority of acceptors
 * have accepted the same proposal (n, v), the value v is chosen (decided).
 *
 * @param learner   learner state
 * @param msg       incoming ACCEPTED message
 * @param num_acceptors total number of acceptors
 * @return true if a value has been chosen (decided)
 *
 * Theorem (Paxos Safety): If a learner learns a value v, no learner
 * can ever learn a different value v' ≠ v.
 */
bool paxos_learner_receive(paxos_learner_t *learner,
                            const paxos_message_t *msg,
                            int num_acceptors);

/* ─── Multi-Paxos ─────────────────────────────────────────────────────
 * L5 Algorithm: Multi-Paxos optimizes consensus for a sequence of
 * values (log). The first instance uses Basic Paxos; subsequent
 * instances skip Phase 1 because the same leader continues using
 * the same ballot number.
 */

/**
 * L5: Multi-Paxos — Start a new instance.
 * After a leader is elected (Phase 1 completes), subsequent proposals
 * skip Phase 1 and proceed directly to Phase 2.
 *
 * @param proposer   proposer state (already past Phase 1)
 * @param log        the replicated log
 * @param log_len    current log length
 * @param value      new value to append
 * @return true if the value can be proposed directly (Phase 1 already done)
 */
bool multipaxos_propose(paxos_proposer_t *proposer,
                         raft_log_entry_t *log,
                         index_t *log_len,
                         const consensus_value_t *value);

/**
 * L5: Multi-Paxos — Check if the current leader can propose.
 * A process is leader if its ballot number has been accepted by
 * a majority in Phase 1.
 */
bool multipaxos_is_leader(const paxos_proposer_t *proposer,
                           const paxos_acceptor_t *acceptors,
                           int num_acceptors);

/**
 * L5: Multi-Paxos — Gap handling.
 * When a follower has missing log entries, request retransmission
 * from the leader.
 *
 * @param log        local log
 * @param log_len    local log length
 * @param leader_log_len leader's log length
 * @param gaps_out   output array of missing indices
 * @return number of gaps
 */
int multipaxos_find_gaps(const raft_log_entry_t *log, index_t log_len,
                          index_t leader_log_len, index_t *gaps_out);

/* ─── Paxos Safety Verification ───────────────────────────────────── */

/**
 * L4 Theorem: Formal statement of Paxos safety.
 *
 * If a value v is chosen at ballot n, then any value proposed at
 * any ballot m > n must also be v.
 *
 * This function verifies the inductive invariant on a given system state.
 *
 * @param acceptors  all acceptor states
 * @param num_acceptors number of acceptors
 * @return true if the safety invariant holds
 */
bool paxos_verify_safety_invariant(const paxos_acceptor_t *acceptors,
                                     int num_acceptors);

/**
 * L4 Lemma: Leader completeness in Paxos.
 * If ballot n is the highest ballot for which a value has been chosen,
 * then for any ballot m > n, the proposer must propose the same value.
 *
 * This is the inductive step in Lamport's safety proof.
 */
bool paxos_leader_completeness_holds(const paxos_acceptor_t *acceptors,
                                       int num_acceptors,
                                       ballot_t highest_chosen);

/* ─── Paxos Configuration ─────────────────────────────────────────── */

/**
 * L5: Run a complete instance of Basic Paxos (Synod protocol).
 * Simulates message passing between proposer, acceptors, and learners.
 *
 * @param proposer_id  node ID of the proposer
 * @param value        proposed value
 * @param num_acceptors total acceptors
 * @param result       output: the decided value (if any)
 * @return true if a value was successfully chosen
 */
bool paxos_run_instance(node_id_t proposer_id,
                         const consensus_value_t *value,
                         int num_acceptors,
                         consensus_value_t *result);

#endif /* PAXOS_CORE_H */
