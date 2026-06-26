/**
 * raft_cluster_example.c — Raft Cluster Simulation Example
 *
 * Demonstrates a complete Raft cluster lifecycle:
 *   1. Cluster initialization (3 servers)
 *   2. Leader election with randomized timeouts
 *   3. Log replication (client commands)
 *   4. Commitment tracking
 *
 * This example shows how Raft achieves consensus through the
 * understandable decomposition into leader election, log replication,
 * and safety.
 *
 * L6 Canonical Problem: Log replication in partial synchrony
 *
 * References:
 *   - Ongaro & Ousterhout (2014)
 */

#include <stdio.h>
#include <string.h>
#include "../include/paxos_raft_types.h"
#include "../include/raft_core.h"

/**
 * Run a simplified Raft cluster with 3 servers.
 * We manually step through election and log replication.
 */
int main(void) {
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║   Raft Cluster Simulation Demo               ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");

    const int NUM_SERVERS = 3;
    raft_server_t servers[NUM_SERVERS];
    node_id_t all_ids[NUM_SERVERS];

    /* Initialize servers with staggered election timeouts */
    uint64_t timeouts[] = {150, 180, 210};  /* Different to avoid split votes */
    for (int i = 0; i < NUM_SERVERS; i++) {
        raft_server_init(&servers[i], (node_id_t)i, NUM_SERVERS, timeouts[i]);
        all_ids[i] = (node_id_t)i;
        printf("Server %d initialized, timeout=%llums\n",
               i, (unsigned long long)servers[i].election_timeout_ms);
    }

    /* ─── Phase 1: Leader Election ─── */
    printf("\n── Phase 1: Leader Election ──\n");

    node_id_t leader_id = MAX_NODES;
    int step = 0;

    while (leader_id == MAX_NODES && step < 50) {
        for (int s = 0; s < NUM_SERVERS; s++) {
            raft_message_t msgs[64];

            /* Advance timer by 10ms */
            servers[s].election_elapsed_ms += 10;

            /* Check for election timeout */
            if (servers[s].persistent.state == RAFT_FOLLOWER &&
                servers[s].election_elapsed_ms >= servers[s].election_timeout_ms) {
                raft_become_candidate(&servers[s]);
                printf("  Step %d: Server %d becomes CANDIDATE (term %llu)\n",
                       step, s,
                       (unsigned long long)servers[s].persistent.current_term);
            }

            /* Send RequestVote if candidate */
            if (servers[s].persistent.state == RAFT_CANDIDATE) {
                int msg_n = raft_tick(&servers[s], 0, msgs,
                                       NUM_SERVERS, all_ids);

                /* Count votes */
                int votes = 1; /* Self-vote */
                for (int m = 0; m < msg_n; m++) {
                    if (msgs[m].type == RAFT_RPC_REQUEST_VOTE) {
                        node_id_t candidate = s;
                        for (int t = 0; t < NUM_SERVERS; t++) {
                            if ((node_id_t)t == candidate) continue;

                            raft_request_vote_reply_t reply;
                            raft_handle_request_vote(&servers[t],
                                &msgs[m].body.request_vote, &reply);
                            if (reply.vote_granted) {
                                votes++;
                            }
                        }
                    }
                }

                /* Check if won */
                if (raft_has_election_majority(votes, NUM_SERVERS)) {
                    raft_become_leader(&servers[s], NUM_SERVERS);
                    leader_id = (node_id_t)s;
                    printf("  Step %d: Server %d wins election → LEADER (term %llu, votes %d)\n",
                           step, s,
                           (unsigned long long)servers[s].persistent.current_term,
                           votes);
                    break;
                }
            }
        }
        step++;
    }

    if (leader_id == MAX_NODES) {
        printf("  FAILED: No leader elected within 50 steps.\n");
        return 1;
    }

    /* ─── Phase 2: Log Replication ─── */
    printf("\n── Phase 2: Log Replication ──\n");
    printf("Leader: Server %d\n", leader_id);

    raft_server_t *leader = &servers[leader_id];

    /* Client commands */
    const char *commands[] = {"SET x=10", "SET y=20", "SET z=30", "INC x"};
    int num_commands = 4;

    for (int c = 0; c < num_commands; c++) {
        consensus_value_t cmd;
        memcpy(cmd.data, commands[c], strlen(commands[c]));
        cmd.length = (uint32_t)strlen(commands[c]);

        index_t idx = raft_append_command(leader, &cmd);
        if (idx > 0) {
            printf("\n  Command %d: \"%s\" appended at index %llu\n",
                   c + 1, commands[c], (unsigned long long)idx);
        }

        /* Replicate to followers */
        for (int f = 0; f < NUM_SERVERS; f++) {
            if ((node_id_t)f == leader_id) continue;

            raft_append_entries_args_t args;
            if (raft_prepare_append_entries(leader, (node_id_t)f, &args)) {
                raft_append_entries_reply_t reply;
                raft_handle_append_entries(&servers[f], &args, &reply);

                if (reply.success) {
                    leader->volatile_.match_index[f] = args.prev_log_index + args.num_entries;
                    leader->volatile_.next_index[f] = leader->volatile_.match_index[f] + 1;
                    printf("    Follower %d replicated (log_len=%llu)\n",
                           f, (unsigned long long)servers[f].persistent.log_length);
                }
            }
        }

        /* Advance commit index */
        index_t old_commit = leader->persistent.commit_index;
        raft_advance_commit_index(leader, NUM_SERVERS);
        if (leader->persistent.commit_index > old_commit) {
            printf("  Commit index advanced: %llu → %llu\n",
                   (unsigned long long)old_commit,
                   (unsigned long long)leader->persistent.commit_index);
        }
    }

    /* ─── Phase 3: Final State ─── */
    printf("\n── Phase 3: Final State ──\n");
    printf("Leader commit index: %llu\n",
           (unsigned long long)leader->persistent.commit_index);

    for (int i = 0; i < NUM_SERVERS; i++) {
        printf("  Server %d: log_len=%llu, commit=%llu, state=",
               i,
               (unsigned long long)servers[i].persistent.log_length,
               (unsigned long long)servers[i].persistent.commit_index);
        switch (servers[i].persistent.state) {
        case RAFT_FOLLOWER:  printf("Follower"); break;
        case RAFT_CANDIDATE: printf("Candidate"); break;
        case RAFT_LEADER:    printf("Leader"); break;
        }
        printf(", term=%llu\n",
               (unsigned long long)servers[i].persistent.current_term);
    }

    /* Verify all logs match */
    printf("\n── Safety Check ──\n");
    bool logs_match = true;
    for (int i = 0; i < NUM_SERVERS; i++) {
        for (int j = i + 1; j < NUM_SERVERS; j++) {
            if (!raft_verify_log_matching(
                    servers[i].persistent.log,
                    servers[i].persistent.log_length,
                    servers[j].persistent.log,
                    servers[j].persistent.log_length,
                    servers[i].persistent.log_length)) {
                logs_match = false;
                printf("  ✗ Log mismatch between servers %d and %d\n", i, j);
            }
        }
    }
    if (logs_match) {
        printf("  ✓ Log Matching property holds across all servers\n");
    }

    printf("\n╔══════════════════════════════════════════════╗\n");
    printf("║   Raft Cluster Simulation Complete!          ║\n");
    printf("╚══════════════════════════════════════════════╝\n");

    return 0;
}
