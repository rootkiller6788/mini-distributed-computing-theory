/*
 * raft.h — Raft Consensus Algorithm (Ongaro & Ousterhout 2014)
 *
 * L5: Raft consensus — designed for understandability.
 * L6: Leader election, log replication, safety, membership changes.
 *
 * Raft decomposes consensus into:
 *   1. Leader election
 *   2. Log replication
 *   3. Safety — leader commits only entries stored on a majority
 *
 * Reference: Ongaro & Ousterhout (2014) "In Search of an Understandable
 *            Consensus Algorithm (Extended Version)"
 */

#ifndef RAFT_H
#define RAFT_H

#include "consensus_types.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    RAFT_FOLLOWER   = 0,
    RAFT_CANDIDATE  = 1,
    RAFT_LEADER     = 2
} raft_state_t;

typedef struct {
    log_entry_t *entries;
    uint64_t     len;
    uint64_t     cap;
} raft_log_t;

typedef struct {
    /* Persistent state on all servers */
    uint64_t    current_term;
    int         voted_for;
    raft_log_t  log;

    /* Volatile state on all servers */
    uint64_t    commit_index;
    uint64_t    last_applied;

    /* Volatile state on leaders */
    uint64_t   *next_index;
    uint64_t   *match_index;

    /* Server identity */
    int         server_id;
    raft_state_t state;
    int         leader_id;

    /* Election state */
    int         votes_received;
    int         total_servers;

    /* Timing */
    uint64_t    election_timeout;
    uint64_t    heartbeat_timeout;
    bool        received_heartbeat;

    /* Statistics */
    uint64_t    terms_served;
    uint64_t    entries_committed;
} raft_server_t;

typedef struct {
    raft_server_t  *servers;
    int             num_servers;
    uint64_t        election_timeout_min;
    uint64_t        election_timeout_max;
    uint64_t        heartbeat_interval;
    bool            is_running;
} raft_cluster_t;

/* Core API */
int  raft_cluster_init(raft_cluster_t *cluster, int n);
void raft_cluster_destroy(raft_cluster_t *cluster);

/* Leader Election */
int  raft_start_election(raft_server_t *server, raft_cluster_t *cluster);
bool raft_handle_request_vote(raft_server_t *server,
                               uint64_t candidate_term, int candidate_id,
                               uint64_t last_log_index, uint64_t last_log_term);

/* Log Replication */
int  raft_append_command(raft_server_t *leader, raft_cluster_t *cluster,
                          const char *command, uint64_t client_id, uint64_t cmd_id);
int  raft_handle_append_entries(raft_server_t *server,
                                 uint64_t leader_term, int leader_id,
                                 uint64_t prev_log_index, uint64_t prev_log_term,
                                 log_entry_t *entries, int num_entries,
                                 uint64_t leader_commit);

/* Safety Verification */
bool raft_verify_leader_completeness(raft_cluster_t *cluster);
bool raft_verify_state_machine_safety(raft_cluster_t *cluster);

/* Commitment */
int  raft_advance_commit(raft_server_t *leader, raft_cluster_t *cluster);
int  raft_apply_committed(raft_server_t *server);

/* Membership Changes */
typedef struct {
    int  *old_config;
    int   old_count;
    int  *new_config;
    int   new_count;
    bool  in_transition;
} raft_membership_t;

int  raft_begin_membership_change(raft_cluster_t *cluster,
                                   int *new_servers, int new_count);
int  raft_complete_membership_change(raft_cluster_t *cluster);

/* Diagnostics */
const char *raft_state_to_string(raft_state_t state);
void raft_get_cluster_status(raft_cluster_t *cluster, int *num_leaders,
                              int *num_candidates, int *num_followers);

#endif /* RAFT_H */
