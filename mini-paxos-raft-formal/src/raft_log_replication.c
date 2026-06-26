/**
 * raft_log_replication.c — Raft Log Replication
 *
 * Implements the Raft log replication mechanism (§5.3, Ongaro &
 * Ousterhout 2014). Log replication is the core data path of Raft:
 * the leader accepts client commands, appends them to its log, and
 * replicates them to followers via AppendEntries RPCs.
 *
 * Log Structure:
 *   Each log entry has:
 *     - term: the term when the entry was created by the leader
 *     - command: the state machine command
 *   Entries are indexed 1, 2, ..., n.
 *
 * Consistency Check:
 *   AppendEntries includes (prevLogIndex, prevLogTerm). The follower
 *   checks that its log matches at that point before appending.
 *   If not, the leader decrements nextIndex and retries.
 *
 * Commitment Rule:
 *   A log entry is committed once a majority of servers have
 *   replicated it AND at least one entry from the leader's current
 *   term has been replicated. This is the "Figure 8" problem
 *   resolution (Ongaro §5.4.2).
 *
 * Knowledge Coverage:
 *   L1 Definitions: log entry, commit index, AppendEntries
 *   L2 Core Concepts: log matching, consistency check, committed vs applied
 *   L4 Fundamental Laws: Log Matching Property, Leader Completeness
 *   L5 Algorithms: AppendEntries, commit advancement
 *
 * References:
 *   - Ongaro & Ousterhout (2014) §§5.3–5.4
 *   - Ongaro, "Consensus: Bridging Theory and Practice" (2014) §3.6
 */

#include "../include/paxos_raft_types.h"
#include "../include/raft_core.h"
#include <string.h>

/* ─── Command Append ───────────────────────────────────────────────── */

/**
 * Leader appends a command to its log.
 *
 * The leader does NOT immediately commit the entry. It must first
 * replicate the entry to a majority of followers via AppendEntries,
 * then advance the commit index.
 *
 * @return the index of the newly appended entry (1-based), or 0 on error.
 */
index_t raft_append_command(raft_server_t *s, const consensus_value_t *command) {
    if (s->persistent.state != RAFT_LEADER) {
        return 0; /* Only the leader appends commands */
    }
    if (s->persistent.log_length >= MAX_LOG_ENTRIES) {
        return 0; /* Log is full */
    }

    index_t idx = s->persistent.log_length + 1;
    s->persistent.log[idx - 1].term    = s->persistent.current_term;
    s->persistent.log[idx - 1].command = *command;
    s->persistent.log_length = idx;

    /* Update leader's own matchIndex */
    s->volatile_.match_index[s->persistent.node_id] = idx;

    return idx;
}

/* ─── Prepare AppendEntries ────────────────────────────────────────── */

/**
 * Leader prepares an AppendEntries RPC for a specific follower.
 *
 * The leader determines which log entries to send based on the
 * follower's nextIndex. If nextIndex is behind the leader's log,
 * entries are included. Otherwise, an empty AppendEntries (heartbeat)
 * is sent.
 *
 * prevLogIndex = nextIndex[follower] - 1
 * prevLogTerm  = log[prevLogIndex - 1].term (0 if prevLogIndex == 0)
 */
bool raft_prepare_append_entries(const raft_server_t *s,
                                  node_id_t follower_id,
                                  raft_append_entries_args_t *args) {
    if (s->persistent.state != RAFT_LEADER) {
        return false; /* Only the leader sends AppendEntries */
    }

    args->term      = s->persistent.current_term;
    args->leader_id = s->persistent.node_id;
    args->leader_commit = s->persistent.commit_index;

    index_t next_idx = s->volatile_.next_index[follower_id];

    /* prevLogIndex is the entry just before the first new entry */
    if (next_idx == 0) {
        args->prev_log_index = 0;
        args->prev_log_term  = 0;
    } else {
        args->prev_log_index = next_idx - 1;
        if (args->prev_log_index > 0 && args->prev_log_index <= s->persistent.log_length) {
            args->prev_log_term = s->persistent.log[args->prev_log_index - 1].term;
        } else if (args->prev_log_index == 0) {
            args->prev_log_term = 0;
        } else {
            args->prev_log_term = 0; /* No log entry at prev_log_index */
        }
    }

    /* Pack log entries from next_idx to log_length */
    args->num_entries = 0;
    for (index_t i = next_idx; i <= s->persistent.log_length &&
         args->num_entries < MAX_LOG_ENTRIES; i++) {
        args->entries[args->num_entries] = s->persistent.log[i - 1];
        args->num_entries++;
    }

    return true;
}

/* ─── Handle AppendEntries (Follower) ──────────────────────────────── */

/**
 * Follower processes an incoming AppendEntries RPC.
 *
 * The five rules from Figure 2 (Ongaro §5.3):
 *
 * R1: Reply false if term < currentTerm
 * R2: Reply false if log doesn't contain entry at prevLogIndex whose
 *     term matches prevLogTerm
 * R3: If an existing entry conflicts with a new one (same index but
 *     different term), delete the existing entry and all following entries
 * R4: Append any new entries not already in the log
 * R5: If leaderCommit > commitIndex, set commitIndex =
 *     min(leaderCommit, index of last new entry)
 *
 * Upon receiving a valid AppendEntries, the follower resets its
 * election timer (even if the RPC contains no entries, i.e., heartbeat).
 */
bool raft_handle_append_entries(raft_server_t *s,
                                 const raft_append_entries_args_t *args,
                                 raft_append_entries_reply_t *reply) {
    reply->term    = s->persistent.current_term;
    reply->success = false;

    /* R1: Reply false if term < currentTerm */
    if (args->term < s->persistent.current_term) {
        return true;
    }

    /* If term > currentTerm, update and become follower */
    if (args->term > s->persistent.current_term) {
        s->persistent.current_term = args->term;
        s->persistent.state = RAFT_FOLLOWER;
        s->persistent.voted_for = MAX_NODES;
        reply->term = args->term;
    }

    /* Recognize the leader — reset election timer */
    if (s->persistent.state == RAFT_CANDIDATE) {
        s->persistent.state = RAFT_FOLLOWER;
    }
    s->election_elapsed_ms = 0;

    /* R2: Check log consistency at prevLogIndex */
    if (args->prev_log_index > 0) {
        if (args->prev_log_index > s->persistent.log_length) {
            /* Follower's log is too short — missing entries */
            return true;
        }
        if (s->persistent.log[args->prev_log_index - 1].term !=
            args->prev_log_term) {
            /* Term mismatch — conflicting entry */
            /* Delete the conflicting entry and all following entries */
            s->persistent.log_length = args->prev_log_index;
            return true;
        }
    }

    /* R3 & R4: Append new entries, resolving conflicts */
    for (index_t i = 0; i < args->num_entries; i++) {
        index_t entry_index = args->prev_log_index + i + 1;

        if (entry_index <= s->persistent.log_length) {
            /* Entry exists — check for conflict */
            if (s->persistent.log[entry_index - 1].term !=
                args->entries[i].term) {
                /* Conflict: delete this and all following entries */
                s->persistent.log_length = entry_index - 1;
            }
        }

        if (entry_index > s->persistent.log_length) {
            /* Append new entry */
            if (s->persistent.log_length >= MAX_LOG_ENTRIES) {
                break; /* Log full */
            }
            s->persistent.log[entry_index - 1] = args->entries[i];
            s->persistent.log_length = entry_index;
        }
    }

    /* R5: Advance commitIndex */
    if (args->leader_commit > s->persistent.commit_index) {
        index_t last_new_entry = args->prev_log_index + args->num_entries;
        index_t new_commit = args->leader_commit;
        if (new_commit > last_new_entry) {
            new_commit = last_new_entry;
        }
        if (new_commit > s->persistent.commit_index) {
            s->persistent.commit_index = new_commit;
        }
    }

    reply->success = true;
    return true;
}

/* ─── Handle AppendEntries Reply (Leader) ──────────────────────────── */

/**
 * Leader processes an AppendEntries reply.
 *
 * On success:
 *   - nextIndex[follower] = last_new_entry + 1
 *   - matchIndex[follower] = last_new_entry
 *   - Try to advance commitIndex
 *
 * On failure:
 *   - Decrement nextIndex[follower] and retry
 *   - This implements the binary-search-like optimization for finding
 *     the point of divergence
 *
 * Returns the new commitIndex (may be unchanged).
 */
index_t raft_handle_append_entries_reply(raft_server_t *s,
                                          node_id_t follower_id,
                                          const raft_append_entries_reply_t *reply,
                                          int num_nodes) {
    if (s->persistent.state != RAFT_LEADER) {
        return s->persistent.commit_index;
    }

    /* Check for higher term (step down) */
    if (reply->term > s->persistent.current_term) {
        s->persistent.current_term = reply->term;
        s->persistent.state = RAFT_FOLLOWER;
        s->persistent.voted_for = MAX_NODES;
        return s->persistent.commit_index;
    }

    if (reply->success) {
        /* Success: update matchIndex and nextIndex */
        index_t last_entry = s->persistent.log_length;
        if (last_entry > s->volatile_.match_index[follower_id]) {
            s->volatile_.match_index[follower_id] = last_entry;
        }
        s->volatile_.next_index[follower_id] = last_entry + 1;
    } else {
        /* Failure: decrement nextIndex for retry */
        if (s->volatile_.next_index[follower_id] > 1) {
            s->volatile_.next_index[follower_id]--;
        }
    }

    /* Advance commit index */
    return raft_advance_commit_index(s, num_nodes);
}

/* ─── Commit Index Advancement ─────────────────────────────────────── */

/**
 * Leader advances commitIndex.
 *
 * Raft's commitment rule (Ongaro §5.4.2):
 *   If there exists an N > commitIndex such that:
 *     (a) A majority of matchIndex[i] ≥ N
 *     (b) log[N].term == currentTerm
 *   Then set commitIndex = N.
 *
 * Condition (b) is crucial — it prevents the "Figure 8" problem where
 * a leader could commit entries from a previous term that might be
 * overwritten by a future leader.
 *
 * The leader commits entries by counting how many followers have
 * replicated each index, from highest to lowest.
 */
index_t raft_advance_commit_index(raft_server_t *s, int num_nodes) {
    if (s->persistent.state != RAFT_LEADER) {
        return s->persistent.commit_index;
    }

    int majority = num_nodes / 2 + 1;

    /* Scan from highest index down to commit_index+1 */
    for (index_t n = s->persistent.log_length; n > s->persistent.commit_index; n--) {
        /* Condition (b): must be from leader's current term */
        if (n == 0) continue;
        if (s->persistent.log[n - 1].term != s->persistent.current_term) {
            continue; /* Can only commit entries from own term */
        }

        /* Condition (a): majority replicated */
        int count = 0;
        for (int i = 0; i < num_nodes; i++) {
            if (s->volatile_.match_index[i] >= n) {
                count++;
            }
        }

        if (count >= majority) {
            s->persistent.commit_index = n;
            return n;
        }
    }

    return s->persistent.commit_index;
}
