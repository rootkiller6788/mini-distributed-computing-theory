/**
 * raft_simulator.c — Raft Cluster Simulation & Timer Tick
 *
 * Implements the Raft server time-stepping function (raft_tick) and
 * a full cluster simulation (raft_simulate_cluster). These are the
 * entry points for running the Raft protocol end-to-end.
 *
 * The tick function models the passage of time:
 *   - Followers/Candidates increment their election timer
 *   - If the election timeout is reached, transition to Candidate
 *   - Leader sends periodic heartbeats
 *
 * The cluster simulation coordinates multiple servers, delivering
 * messages and processing RPCs in a round-based fashion.
 *
 * Knowledge Coverage:
 *   L5 Algorithms: Raft cluster simulation, time-based protocol steps
 *   L6 Canonical Problems: consensus in cluster with failures
 *
 * References:
 *   - Ongaro & Ousterhout (2014) §5.2, §5.3
 */

#include "../include/paxos_raft_types.h"
#include "../include/raft_core.h"
#include <string.h>
#include <stdio.h>

/* ─── Raft Tick ────────────────────────────────────────────────────── */

/**
 * Step the Raft server state machine by elapsed_ms.
 *
 * This function handles:
 *   - Timeout detection and state transitions
 *   - Leader heartbeat generation
 *   - Message output
 *
 * Return value: number of messages generated.
 */
int raft_tick(raft_server_t *s, uint64_t elapsed_ms,
               raft_message_t *out_msgs,
               int num_nodes, const node_id_t *all_ids) {
    int msg_count = 0;

    /* Update election timer */
    s->election_elapsed_ms += elapsed_ms;

    switch (s->persistent.state) {

    case RAFT_FOLLOWER:
        /* If election timeout elapses, become candidate */
        if (s->election_elapsed_ms >= s->election_timeout_ms) {
            raft_become_candidate(s);

            /* Send RequestVote RPCs to all other nodes */
            for (int i = 0; i < num_nodes; i++) {
                if (all_ids[i] == s->persistent.node_id) continue;

                out_msgs[msg_count].type = RAFT_RPC_REQUEST_VOTE;
                out_msgs[msg_count].body.request_vote.term = s->persistent.current_term;
                out_msgs[msg_count].body.request_vote.candidate_id = s->persistent.node_id;

                /* Set last log info */
                if (s->persistent.log_length > 0) {
                    index_t last = s->persistent.log_length;
                    out_msgs[msg_count].body.request_vote.last_log_index = last;
                    out_msgs[msg_count].body.request_vote.last_log_term =
                        s->persistent.log[last - 1].term;
                } else {
                    out_msgs[msg_count].body.request_vote.last_log_index = 0;
                    out_msgs[msg_count].body.request_vote.last_log_term = 0;
                }
                msg_count++;
            }
        }
        break;

    case RAFT_CANDIDATE:
        /* If election timeout elapses, start new election */
        if (s->election_elapsed_ms >= s->election_timeout_ms) {
            raft_become_candidate(s);

            /* Resend RequestVote RPCs */
            for (int i = 0; i < num_nodes; i++) {
                if (all_ids[i] == s->persistent.node_id) continue;

                out_msgs[msg_count].type = RAFT_RPC_REQUEST_VOTE;
                out_msgs[msg_count].body.request_vote.term = s->persistent.current_term;
                out_msgs[msg_count].body.request_vote.candidate_id = s->persistent.node_id;
                if (s->persistent.log_length > 0) {
                    index_t last = s->persistent.log_length;
                    out_msgs[msg_count].body.request_vote.last_log_index = last;
                    out_msgs[msg_count].body.request_vote.last_log_term =
                        s->persistent.log[last - 1].term;
                } else {
                    out_msgs[msg_count].body.request_vote.last_log_index = 0;
                    out_msgs[msg_count].body.request_vote.last_log_term = 0;
                }
                msg_count++;
            }
        }
        break;

    case RAFT_LEADER:
        /* Leader sends periodic heartbeats (empty AppendEntries) */
        if (s->election_elapsed_ms >= s->heartbeat_timeout_ms) {
            s->election_elapsed_ms = 0;

            for (int i = 0; i < num_nodes; i++) {
                if (all_ids[i] == s->persistent.node_id) continue;

                raft_append_entries_args_t args;
                if (raft_prepare_append_entries(s, all_ids[i], &args)) {
                    out_msgs[msg_count].type = RAFT_RPC_APPEND_ENTRIES;
                    out_msgs[msg_count].body.append_entries = args;
                    msg_count++;
                }
            }
        }
        break;
    }

    return msg_count;
}

/* ─── Cluster Simulation ───────────────────────────────────────────── */

/**
 * Simulate a complete Raft cluster through leader election and log
 * replication.
 *
 * This function:
 *   1. Initializes num_servers Raft servers
 *   2. Assigns randomized election timeouts
 *   3. Iteratively steps each server, delivers RPCs, and tracks progress
 *   4. Stops when all commands are committed or max_steps is reached
 *
 * This is a simplified round-based simulation (not real-time).
 * Each step, one server processes its timer, possibly generating
 * messages. Messages are delivered in the next step.
 */
bool raft_simulate_cluster(int num_servers,
                            const consensus_value_t *commands,
                            int num_commands,
                            int steps,
                            raft_log_entry_t out_logs[][MAX_LOG_ENTRIES]) {
    if (num_servers < 1 || num_servers > MAX_NODES) return false;
    if (num_commands < 1) return false;

    raft_server_t servers[MAX_NODES];
    node_id_t all_ids[MAX_NODES];
    raft_message_t msg_queue[4096];
    int msg_queue_len = 0;

    /* Initialize all servers with randomized timeouts */
    for (int i = 0; i < num_servers; i++) {
        node_id_t id = (node_id_t)i;
        all_ids[i] = id;
        uint64_t base_timeout = 150 + (uint64_t)i * 37; /* Deterministic "randomization" */
        raft_server_init(&servers[i], id, num_servers, base_timeout);

        /* Ensure timeouts are different to avoid split votes */
        raft_set_election_timeout(&servers[i], 150, 1.0 + (double)i * 0.15);
    }

    /* Find the leader (simplified: first server to win election) */
    node_id_t leader_id = MAX_NODES; /* Sentinel: no leader yet */
    int commands_proposed = 0;
    int commands_committed = 0;

    for (int step = 0; step < steps; step++) {
        /* --- Phase 1: Each server ticks --- */
        msg_queue_len = 0;

        for (int s = 0; s < num_servers; s++) {
            raft_message_t tick_msgs[64];
            int n = raft_tick(&servers[s], 10, /* 10ms per step */
                              tick_msgs, num_servers, all_ids);

            for (int m = 0; m < n && msg_queue_len < 4096; m++) {
                msg_queue[msg_queue_len++] = tick_msgs[m];
            }
        }

        /* --- Phase 2: Deliver messages --- */
        for (int m = 0; m < msg_queue_len; m++) {
            raft_message_t *msg = &msg_queue[m];

            /* Determine target — RequestVote goes to all, AppendEntries to one */
            switch (msg->type) {
            case RAFT_RPC_REQUEST_VOTE: {
                /* Deliver to each server that is not the sender */
                node_id_t candidate_id = msg->body.request_vote.candidate_id;
                int votes = 0;

                for (int t = 0; t < num_servers; t++) {
                    if (servers[t].persistent.node_id == candidate_id) {
                        votes++; /* Self-vote */
                        continue;
                    }

                    raft_request_vote_reply_t reply;
                    raft_handle_request_vote(&servers[t],
                                              &msg->body.request_vote,
                                              &reply);

                    if (reply.vote_granted) {
                        votes++;
                    }

                    /* Deliver reply back to candidate */
                    raft_handle_request_vote_reply(&servers[candidate_id], &reply);
                }

                /* Check if candidate won */
                if (raft_has_election_majority(votes, num_servers)) {
                    raft_become_leader(&servers[candidate_id], num_servers);
                    leader_id = candidate_id;
                }
                break;
            }

            case RAFT_RPC_APPEND_ENTRIES: {
                /* Deliver to each follower */
                for (int t = 0; t < num_servers; t++) {
                    if ((node_id_t)t == msg->body.append_entries.leader_id) continue;

                    raft_append_entries_reply_t reply;
                    raft_handle_append_entries(&servers[t],
                                                &msg->body.append_entries,
                                                &reply);

                    /* Deliver reply back to leader */
                    raft_handle_append_entries_reply(
                        &servers[msg->body.append_entries.leader_id],
                        (node_id_t)t, &reply, num_servers);
                }
                break;
            }

            default:
                break;
            }
        }

        /* --- Phase 3: Leader proposes commands --- */
        if (leader_id < (node_id_t)num_servers &&
            commands_proposed < num_commands) {
            raft_server_t *leader = &servers[leader_id];
            if (leader->persistent.state == RAFT_LEADER) {
                index_t idx = raft_append_command(leader,
                                                   &commands[commands_proposed]);
                if (idx > 0) {
                    commands_proposed++;
                }
            }
        }

        /* --- Phase 4: Check commit progress --- */
        commands_committed = 0;
        if (leader_id < (node_id_t)num_servers) {
            raft_server_t *leader = &servers[leader_id];
            commands_committed = (int)leader->persistent.commit_index;
        }

        if (commands_committed >= num_commands) {
            break; /* All commands committed — done! */
        }

        /* Heartbeat/sync: leader sends AppendEntries periodically */
        if (leader_id < (node_id_t)num_servers && step % 5 == 0) {
            raft_server_t *leader = &servers[leader_id];
            if (leader->persistent.state == RAFT_LEADER) {
                raft_advance_commit_index(leader, num_servers);
            }
        }
    }

    /* Copy output logs */
    for (int i = 0; i < num_servers; i++) {
        for (index_t j = 0; j < servers[i].persistent.log_length; j++) {
            out_logs[i][j] = servers[i].persistent.log[j];
        }
        /* Mark end of log */
        if (servers[i].persistent.log_length < MAX_LOG_ENTRIES) {
            out_logs[i][servers[i].persistent.log_length].term = (term_t)(-1);
        }
    }

    /* Success if all commands were committed */
    return commands_committed >= num_commands;
}
