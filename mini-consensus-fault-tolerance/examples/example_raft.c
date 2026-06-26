/*
 * example_raft.c - End-to-end Raft cluster simulation.
 * L6: Leader election, log replication, and commitment.
 *
 * Simulates a 5-server Raft cluster: elects a leader, replicates
 * log entries, commits, and applies them.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "raft.h"

int main(void)
{
    srand((unsigned int)time(NULL));
    printf("=== Raft Consensus Demonstration ===\n\n");

    int num_servers = 5;
    printf("Starting Raft cluster with %d servers.\n", num_servers);

    raft_cluster_t cluster;
    if (raft_cluster_init(&cluster, num_servers) < 0) {
        printf("Failed to initialize cluster.\n");
        return 1;
    }

    /* Step 1: Leader Election */
    printf("\n--- Step 1: Leader Election ---\n");
    /* Start election on server 0 */
    cluster.servers[0].state = RAFT_CANDIDATE;
    cluster.servers[0].current_term = 1;
    cluster.servers[0].voted_for = 0;

    int elect_rc = raft_start_election(&cluster.servers[0], &cluster);
    if (elect_rc == 0) {
        printf("Server 0 elected as LEADER for term %llu.\n",
               (unsigned long long)cluster.servers[0].current_term);
    } else {
        printf("Election result: %d\n", elect_rc);
    }

    /* Step 2: Log Replication */
    printf("\n--- Step 2: Log Replication ---\n");
    const char *commands[] = {
        "SET balance=1000",
        "SET owner=Alice",
        "APPEND log='started'",
        "SET version=2.0"
    };
    int num_cmds = 4;

    raft_server_t *leader = &cluster.servers[0];
    if (leader->state != RAFT_LEADER) {
        printf("No leader available for replication.\n");
        raft_cluster_destroy(&cluster);
        return 1;
    }

    for (int i = 0; i < num_cmds; i++) {
        int idx = raft_append_command(leader, &cluster,
                                       commands[i], 1, (uint64_t)(i + 1));
        printf("  Command '%s' -> log index %d\n",
               commands[i], idx);
    }

    printf("\nLeader log length: %llu\n",
           (unsigned long long)leader->log.len);

    /* Step 3: Commitment */
    printf("\n--- Step 3: Commitment ---\n");
    int committed = raft_advance_commit(leader, &cluster);
    printf("Newly committed entries: %d\n", committed);
    printf("Commit index: %llu\n",
           (unsigned long long)leader->commit_index);

    /* Step 4: Apply to state machine */
    printf("\n--- Step 4: Apply to State Machine ---\n");
    int applied = raft_apply_committed(leader);
    printf("Newly applied entries: %d\n", applied);
    printf("Last applied: %llu\n",
           (unsigned long long)leader->last_applied);

    /* Step 5: Safety Verification */
    printf("\n--- Step 5: Safety Verification ---\n");
    bool lc = raft_verify_leader_completeness(&cluster);
    bool sm = raft_verify_state_machine_safety(&cluster);
    printf("Leader Completeness holds: %s\n", lc ? "YES" : "NO");
    printf("State Machine Safety holds: %s\n", sm ? "YES" : "NO");

    /* Cluster status */
    int nl, nc, nf;
    raft_get_cluster_status(&cluster, &nl, &nc, &nf);
    printf("\nCluster: %d leaders, %d candidates, %d followers\n",
           nl, nc, nf);

    /* Follower log states */
    printf("\nFollower log lengths:\n");
    for (int i = 0; i < num_servers; i++) {
        printf("  Server %d (%s): log_len=%llu, commit=%llu\n",
               i, raft_state_to_string(cluster.servers[i].state),
               (unsigned long long)cluster.servers[i].log.len,
               (unsigned long long)cluster.servers[i].commit_index);
    }

    raft_cluster_destroy(&cluster);
    printf("\n=== Raft demo complete ===\n");
    return 0;
}
