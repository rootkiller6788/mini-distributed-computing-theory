/**
 * raft_core.h — Raft Consensus Algorithm
 *
 * Implements the Raft consensus protocol as described in Ongaro &
 * Ousterhout, "In Search of an Understandable Consensus Algorithm"
 * (USENIX ATC 2014). Raft decomposes consensus into three subproblems:
 *
 *   1. Leader Election — elect a distinguished leader for each term
 *   2. Log Replication — leader replicates log entries to followers
 *   3. Safety — ensures State Machine Safety property
 *
 * Raft's key design principle: stronger leadership (only leader handles
 * client requests and sends log entries) makes the protocol easier to
 * understand than Paxos while providing equivalent guarantees.
 *
 * References:
 *   - Ongaro & Ousterhout (2014) "In Search of an Understandable
 *     Consensus Algorithm", USENIX ATC
 *   - Ongaro, "Consensus: Bridging Theory and Practice" (PhD thesis, 2014)
 *
 * Knowledge Coverage:
 *   L1 Definitions: term, leader, follower, candidate, log, commit
 *   L2 Core Concepts: leader election, log replication, safety
 *   L4 Fundamental Laws: Raft safety proof (Leader Completeness)
 *   L5 Algorithms: Raft state machine, election, AppendEntries
 */

#ifndef RAFT_CORE_H
#define RAFT_CORE_H

#include "paxos_raft_types.h"
#include <stdbool.h>

/* ─── Initialization ───────────────────────────────────────────────── */

/**
 * L5: Initialize a Raft server to Follower state with term 0.
 *
 * @param s          server to initialize
 * @param id         node ID
 * @param num_nodes  total number of nodes (for quorum computation)
 * @param election_timeout_ms  base election timeout (randomized per node)
 */
void raft_server_init(raft_server_t *s, node_id_t id,
                       int num_nodes, uint64_t election_timeout_ms);

/**
 * L5: Configure election timeout randomization.
 * Raft requires randomized election timeouts to prevent split votes.
 * Each server picks timeout ∈ [T, 2T] where T is the base timeout.
 *
 * @param s           server
 * @param base_ms     base timeout in ms
 * @param random_mul  random multiplier in [1.0, 2.0]
 */
void raft_set_election_timeout(raft_server_t *s, uint64_t base_ms,
                                double random_mul);

/* ─── Leader Election ────────────────────────────────────────────────
 * L5 Algorithm: Raft leader election (Ongaro §5.2)
 *
 * Servers start as Followers. If a follower doesn't hear from the
 * leader within the election timeout, it becomes a Candidate and
 * starts a new election:
 *   1. Increment currentTerm
 *   2. Vote for self
 *   3. Send RequestVote RPCs to all other servers
 *   4. If majority votes received → become Leader
 *   5. If AppendEntries received from new leader → revert to Follower
 *   6. If election timeout elapses → start new election
 */

/**
 * L5: Transition server to Candidate state and start an election.
 * Increments currentTerm, votes for self, resets election timer.
 *
 * @param s  server transitioning to candidate
 * @return true if transition is valid
 */
bool raft_become_candidate(raft_server_t *s);

/**
 * L5: Process a RequestVote RPC as a follower.
 *
 * Voting rules (Ongaro §5.2, Figure 2):
 *   1. Reply false if term < currentTerm
 *   2. If votedFor is null or candidateId, and candidate's log is at
 *      least as up-to-date as receiver's log, grant vote.
 *
 * Up-to-date criterion: the candidate's last log entry has higher term,
 * or same term and greater or equal index.
 *
 * @param s      receiver's server state
 * @param args   RequestVote arguments
 * @param reply  output: RequestVote reply
 * @return true if vote is granted
 */
bool raft_handle_request_vote(raft_server_t *s,
                               const raft_request_vote_args_t *args,
                               raft_request_vote_reply_t *reply);

/**
 * L5: Process RequestVote replies as a candidate.
 * If votes received from majority → become Leader.
 *
 * @param s       candidate server state
 * @param reply   received RequestVote reply
 * @return true if candidate becomes leader
 */
bool raft_handle_request_vote_reply(raft_server_t *s,
                                     const raft_request_vote_reply_t *reply);

/**
 * L5: Become leader (called after winning election).
 * Initializes nextIndex[] and matchIndex[] for all followers.
 *
 * @param s          new leader
 * @param num_nodes  total number of servers
 */
void raft_become_leader(raft_server_t *s, int num_nodes);

/* ─── Log Replication ────────────────────────────────────────────────
 * L5 Algorithm: Raft log replication (Ongaro §5.3)
 *
 * Leader receives client commands, appends to its log, then sends
 * AppendEntries RPCs to followers. Once a majority have replicated
 * the entry, the leader commits it and applies to its state machine.
 */

/**
 * L5: Leader appends a command to its log.
 *
 * @param s       leader server
 * @param command state machine command to append
 * @return index of the new log entry
 */
index_t raft_append_command(raft_server_t *s, const consensus_value_t *command);

/**
 * L5: Leader prepares an AppendEntries RPC for a specific follower.
 *
 * @param s            leader server
 * @param follower_id  target follower
 * @param args         output: AppendEntries arguments
 * @return true if AppendEntries should be sent (heartbeat or entries)
 */
bool raft_prepare_append_entries(const raft_server_t *s,
                                  node_id_t follower_id,
                                  raft_append_entries_args_t *args);

/**
 * L5: Follower handles an incoming AppendEntries RPC.
 *
 * Consistency check (Ongaro §5.3):
 *   1. Reply false if term < currentTerm
 *   2. Reply false if log doesn't contain entry at prevLogIndex
 *      with term matching prevLogTerm
 *   3. If existing entry conflicts with a new one, delete it and all
 *      following entries
 *   4. Append new entries not already in the log
 *   5. If leaderCommit > commitIndex, set commitIndex = min(leaderCommit,
 *      index of last new entry)
 *
 * @param s      follower server state
 * @param args   AppendEntries arguments
 * @param reply  output: AppendEntries reply
 * @return true if the AppendEntries was handled
 */
bool raft_handle_append_entries(raft_server_t *s,
                                 const raft_append_entries_args_t *args,
                                 raft_append_entries_reply_t *reply);

/**
 * L5: Leader handles an AppendEntries reply.
 * On success: update nextIndex and matchIndex for the follower.
 * On failure: decrement nextIndex and retry.
 * Advances commitIndex if a majority has replicated an entry.
 *
 * @param s            leader server
 * @param follower_id  follower that replied
 * @param reply        AppendEntries reply
 * @param num_nodes    total number of servers
 * @return the new commitIndex
 */
index_t raft_handle_append_entries_reply(raft_server_t *s,
                                          node_id_t follower_id,
                                          const raft_append_entries_reply_t *reply,
                                          int num_nodes);

/**
 * L5: Leader advances commitIndex.
 * If there exists an N > commitIndex such that a majority of
 * matchIndex[i] ≥ N and log[N].term == currentTerm:
 *   set commitIndex = N.
 *
 * This is Raft's key safety mechanism: leaders only commit entries
 * from their own term (Figure 8, Ongaro §5.4.2).
 *
 * @param s          leader server
 * @param num_nodes  total servers
 * @return new commitIndex
 */
index_t raft_advance_commit_index(raft_server_t *s, int num_nodes);

/* ─── Safety Properties ──────────────────────────────────────────────
 * L4 Fundamental Law: Raft safety properties.
 *
 * Raft guarantees:
 *   1. Election Safety: at most one leader per term
 *   2. Leader Append-Only: leader never overwrites its own log entries
 *   3. Log Matching: if two logs contain an entry with same index and
 *      term, the logs are identical in all preceding entries
 *   4. Leader Completeness: committed entries are in all future leaders' logs
 *   5. State Machine Safety: same index → same command applied
 */

/**
 * L4: Verify the Log Matching Property.
 *
 * Property: If log[i].term == log'[i].term, then for all j ≤ i,
 * log[j] == log'[j].
 *
 * @param a  first log
 * @param a_len length of first log
 * @param b  second log
 * @param b_len length of second log
 * @param i  index to check
 * @return true if log matching holds at index i
 */
bool raft_verify_log_matching(const raft_log_entry_t *a, index_t a_len,
                                const raft_log_entry_t *b, index_t b_len,
                                index_t i);

/**
 * L4: Verify Leader Completeness Property.
 *
 * If a log entry is committed in a given term, then that entry will
 * be present in the logs of all leaders elected in higher terms.
 *
 * This is enforced by the RequestVote restriction: a voter only votes
 * for a candidate whose log is at least as up-to-date as its own.
 *
 * @param committed_log  the log of a committed entry
 * @param committed_term term of the committed entry
 * @param leader_log     the log of the leader
 * @param leader_len     length of leader's log
 * @return true if leader contains the committed entry
 */
bool raft_verify_leader_completeness(const raft_log_entry_t *committed_log,
                                       index_t committed_len,
                                       term_t committed_term,
                                       const raft_log_entry_t *leader_log,
                                       index_t leader_len);

/**
 * L4: Verify State Machine Safety.
 *
 * If any server has applied a log entry at a given index to its state
 * machine, no other server will ever apply a different log entry at
 * the same index.
 *
 * @param servers array of server states
 * @param num_servers number of servers
 * @return true if state machine safety holds
 */
bool raft_verify_state_machine_safety(const raft_server_t *servers,
                                        int num_servers);

/**
 * L5: Raft — Step the server's state machine timer.
 * Called periodically. Checks for election timeout and triggers
 * leader election if needed. Leader sends heartbeats.
 *
 * @param s         server state
 * @param elapsed_ms time elapsed since last step
 * @param out_msgs   output: RPC messages to send
 * @param num_nodes  total number of servers
 * @param all_ids    all server node IDs
 * @return number of messages generated
 */
int raft_tick(raft_server_t *s, uint64_t elapsed_ms,
               raft_message_t *out_msgs,
               int num_nodes, const node_id_t *all_ids);

/* ─── Election Utility ────────────────────────────────────────────── */

/**
 * L2: Check if votes constitute an election majority.
 * @return true if votes_received > num_servers / 2
 */
bool raft_has_election_majority(int votes_received, int num_servers);

/* ─── Cluster Simulation ───────────────────────────────────────────── */

/**
 * L5: Simulate a complete Raft cluster.
 * Runs multiple servers through leader election and log replication.
 *
 * @param num_servers   number of servers in the cluster
 * @param commands      array of commands to propose
 * @param num_commands  number of commands
 * @param steps         maximum simulation steps
 * @param out_logs      output: final log for each server
 * @return true if cluster reaches consensus on all commands
 */
bool raft_simulate_cluster(int num_servers,
                            const consensus_value_t *commands,
                            int num_commands,
                            int steps,
                            raft_log_entry_t out_logs[][MAX_LOG_ENTRIES]);

#endif /* RAFT_CORE_H */
