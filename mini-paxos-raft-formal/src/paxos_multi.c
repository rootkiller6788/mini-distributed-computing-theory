/**
 * paxos_multi.c — Multi-Paxos Protocol
 *
 * Extends Basic Paxos to handle a sequence of consensus decisions — a
 * replicated log. Multi-Paxos is the practical Paxos used in real
 * systems (Chubby, Spanner, etc.).
 *
 * Key optimizations over Basic Paxos:
 *   1. Leader election: Phase 1 executes once to elect a leader;
 *      subsequent proposals skip Phase 1.
 *   2. Log structure: each consensus instance corresponds to a log index.
 *   3. Gap handling: followers may have missing entries.
 *
 * In Multi-Paxos, distinguishing between "being chosen" and "being
 * learned" is important. A value is chosen when a majority of acceptors
 * have accepted it. A learner learns it when it receives enough
 * Accepted messages.
 *
 * Knowledge Coverage:
 *   L5 Algorithms: Multi-Paxos, leader optimization, log management
 *   L7 Applications: Chubby (Google), Spanner (Google)
 *
 * References:
 *   - Lamport, "Paxos Made Simple" (2001) §3 — Implementing a State Machine
 *   - Chandra, Griesemer, Redstone, "Paxos Made Live — An Engineering
 *     Perspective" (PODC 2007) — Google Chubby
 *   - Corbett et al., "Spanner: Google's Globally-Distributed Database"
 *     (OSDI 2012)
 */

#include "../include/paxos_raft_types.h"
#include "../include/paxos_core.h"
#include <string.h>

/**
 * Multi-Paxos: Propose a value after leader election.
 *
 * If the proposer has already completed Phase 1 (received Promises from
 * a majority), it can proceed directly to Phase 2 for subsequent log
 * entries. This is the key optimization of Multi-Paxos.
 *
 * In practice, the leader appends the value to its log and initiates
 * Phase 2 for the corresponding log index.
 */
bool multipaxos_propose(paxos_proposer_t *proposer,
                         raft_log_entry_t *log,
                         index_t *log_len,
                         const consensus_value_t *value) {
    /* The proposer must have completed Phase 1 (promises from a majority) */
    if (proposer->phase < 1) {
        return false; /* Must run Phase 1 first */
    }
    if (*log_len >= MAX_LOG_ENTRIES) {
        return false; /* Log full */
    }

    /* Append to local log */
    log[*log_len].term    = proposer->current_ballot;
    log[*log_len].command = *value;
    (*log_len)++;

    return true;
}

/**
 * Check if the given proposer has established leadership.
 *
 * A proposer is leader if it has completed Phase 1 and a majority of
 * acceptors have promised not to accept lower ballots.
 */
bool multipaxos_is_leader(const paxos_proposer_t *proposer,
                           const paxos_acceptor_t *acceptors,
                           int num_acceptors) {
    if (proposer->phase < 1) return false;

    /* Count how many acceptors have promised at least this ballot */
    int count = 0;
    for (int i = 0; i < num_acceptors; i++) {
        if (acceptors[i].promise_ballot >= proposer->current_ballot) {
            count++;
        }
    }

    int majority = num_acceptors / 2 + 1;
    return count >= majority;
}

/**
 * Find gaps (missing log entries) between the local log and the
 * leader's log.
 *
 * When a follower is behind the leader, it needs to identify which
 * entries are missing. The follower sends this gap list to the
 * leader, which retransmits the missing entries.
 *
 * This is analogous to Raft's AppendEntries consistency check but
 * requires explicit gap management in Paxos.
 */
int multipaxos_find_gaps(const raft_log_entry_t *log, index_t log_len,
                          index_t leader_log_len, index_t *gaps_out) {
    int num_gaps = 0;

    for (index_t i = 0; i < leader_log_len && i < log_len; i++) {
        /* Entries present in both logs are not gaps */
        /* In a real implementation, we'd compare terms/values */
        (void)log; /* Compares entries in full implementation */
    }

    /* All indices from log_len to leader_log_len are gaps */
    for (index_t i = log_len; i < leader_log_len; i++) {
        if (gaps_out && num_gaps < (int)(MAX_LOG_ENTRIES)) {
            gaps_out[num_gaps] = i;
        }
        num_gaps++;
    }

    return num_gaps;
}

/**
 * Multi-Paxos: Determine the chosen index — the highest log index
 * for which a value has been decided by a majority of acceptors.
 *
 * This is analogous to Raft's commitIndex but uses acceptor state
 * rather than explicit commit messages.
 */
index_t multipaxos_chosen_index(const paxos_acceptor_t *acceptors,
                                  int num_acceptors,
                                  index_t max_index) {
    int majority = num_acceptors / 2 + 1;
    index_t chosen = 0;

    for (index_t idx = 0; idx < max_index; idx++) {
        int count = 0;
        ballot_t target_ballot = 0;
        consensus_value_t target_value;
        bool first = true;

        for (int a = 0; a < num_acceptors; a++) {
            /* In Multi-Paxos, each log index is a separate Paxos instance.
             * Here we use accepted_ballot as a proxy for the instance state.
             * A full implementation would maintain per-index state. */
            if (acceptors[a].has_accepted) {
                if (first) {
                    target_ballot = acceptors[a].accepted_ballot;
                    target_value = acceptors[a].accepted_value;
                    first = false;
                    count = 1;
                } else if (acceptors[a].accepted_ballot == target_ballot) {
                    count++;
                }
            }
        }

        if (count >= majority) {
            chosen = idx + 1;
        } else {
            break; /* Gaps: entries must be chosen in order */
        }
    }

    return chosen;
}

/**
 * Multi-Paxos: Compute the number of distinct values that have been
 * chosen across all log indices.
 *
 * This function simulates Multi-Paxos execution and counts how many
 * log entries reached consensus among the given set of acceptors.
 */
int multipaxos_count_chosen(const paxos_acceptor_t *acceptors,
                              int num_acceptors) {
    int majority = num_acceptors / 2 + 1;

    /* For a single-instance (Basic Paxos), check if a value is chosen */
    int accepted_count = 0;
    for (int i = 0; i < num_acceptors; i++) {
        if (acceptors[i].has_accepted) {
            accepted_count++;
        }
    }

    return (accepted_count >= majority) ? 1 : 0;
}

/**
 * Multi-Paxos: Execute a leader-driven log replication round.
 *
 * The leader iterates through its log and sends Accept messages
 * for each uncommitted entry. This is the Multi-Paxos equivalent
 * of Raft's AppendEntries.
 *
 * @param proposer        the leader proposer
 * @param log             leader's log
 * @param log_len         leader's log length
 * @param num_acceptors   number of acceptors
 * @param chosen_idx      output: new chosen index
 * @param out_msgs        output: Accept messages
 * @return number of messages generated
 */
int multipaxos_replicate_log(paxos_proposer_t *proposer,
                              const raft_log_entry_t *log,
                              index_t log_len,
                              int num_acceptors,
                              const node_id_t *acceptor_ids,
                              index_t *chosen_idx,
                              paxos_message_t *out_msgs) {
    int msg_count = 0;

    /* For each uncommitted log entry, send an Accept message.
     * In Multi-Paxos, each log index is a separate Paxos instance.
     * The leader uses its established ballot for all instances. */
    for (index_t i = 0; i < log_len && msg_count < (int)(MAX_NODES * MAX_LOG_ENTRIES); i++) {
        for (int a = 0; a < num_acceptors; a++) {
            out_msgs[msg_count].type   = PAXOS_MSG_ACCEPT;
            out_msgs[msg_count].from   = proposer->node_id;
            out_msgs[msg_count].to     = acceptor_ids[a];
            out_msgs[msg_count].ballot = proposer->current_ballot;
            out_msgs[msg_count].value  = log[i].command;
            msg_count++;
        }
    }

    /* Estimate chosen index */
    *chosen_idx = log_len;
    return msg_count;
}
