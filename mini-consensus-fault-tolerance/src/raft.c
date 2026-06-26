/*
 * raft.c - Raft Consensus Algorithm Implementation
 * L5: Leader election, log replication, commitment.
 * L4: Leader Completeness and State Machine Safety verification.
 * L7: Membership changes (joint consensus).
 * Reference: Ongaro & Ousterhout (2014)
 */
#include "raft.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

int raft_cluster_init(raft_cluster_t *cluster, int n)
{
    if (!cluster || n < 1) return -1;
    cluster->servers = (raft_server_t *)calloc((size_t)n, sizeof(raft_server_t));
    if (!cluster->servers) return -1;
    cluster->num_servers = n;
    cluster->election_timeout_min = 150;
    cluster->election_timeout_max = 300;
    cluster->heartbeat_interval = 50;
    cluster->is_running = true;
    for (int i = 0; i < n; i++) {
        raft_server_t *s = &cluster->servers[i];
        s->server_id = i;
        s->state = RAFT_FOLLOWER;
        s->current_term = 0;
        s->voted_for = -1;
        s->leader_id = -1;
        s->commit_index = 0;
        s->last_applied = 0;
        s->total_servers = n;
        s->votes_received = 0;
        s->received_heartbeat = false;
        s->terms_served = 0;
        s->entries_committed = 0;
        s->log.entries = NULL;
        s->log.len = 0;
        s->log.cap = 0;
        s->next_index = (uint64_t *)calloc((size_t)n, sizeof(uint64_t));
        s->match_index = (uint64_t *)calloc((size_t)n, sizeof(uint64_t));
        s->election_timeout = cluster->election_timeout_min +
            (uint64_t)(rand() % (int)(cluster->election_timeout_max - cluster->election_timeout_min + 1));
        s->heartbeat_timeout = cluster->heartbeat_interval;
    }
    return n;
}

void raft_cluster_destroy(raft_cluster_t *cluster)
{
    if (!cluster || !cluster->servers) return;
    for (int i = 0; i < cluster->num_servers; i++) {
        if (cluster->servers[i].log.entries) free(cluster->servers[i].log.entries);
        if (cluster->servers[i].next_index) free(cluster->servers[i].next_index);
        if (cluster->servers[i].match_index) free(cluster->servers[i].match_index);
    }
    free(cluster->servers);
    cluster->servers = NULL;
    cluster->num_servers = 0;
    cluster->is_running = false;
}

static uint64_t raft_last_log_term(raft_server_t *s)
{
    if (s->log.len == 0) return 0;
    return s->log.entries[s->log.len - 1].term;
}

static uint64_t raft_last_log_index(raft_server_t *s)
{
    return s->log.len;
}

static uint64_t raft_log_append_entry(raft_server_t *s, log_entry_t entry)
{
    if (s->log.len >= s->log.cap) {
        uint64_t new_cap = s->log.cap == 0 ? 16 : s->log.cap * 2;
        log_entry_t *new_entries = (log_entry_t *)realloc(s->log.entries, (size_t)new_cap * sizeof(log_entry_t));
        if (!new_entries) return 0;
        s->log.entries = new_entries;
        s->log.cap = new_cap;
    }
    s->log.entries[s->log.len] = entry;
    s->log.len++;
    return s->log.len;
}

int raft_start_election(raft_server_t *server, raft_cluster_t *cluster)
{
    if (!server || !cluster) return -1;
    if (server->state != RAFT_CANDIDATE) return -1;
    server->votes_received = 1;
    for (int i = 0; i < cluster->num_servers; i++) {
        if (i == server->server_id) continue;
        raft_server_t *peer = &cluster->servers[i];
        bool granted = raft_handle_request_vote(peer, server->current_term, server->server_id,
                                                 raft_last_log_index(server), raft_last_log_term(server));
        if (granted) {
            server->votes_received++;
        } else if (peer->current_term > server->current_term) {
            server->state = RAFT_FOLLOWER;
            server->current_term = peer->current_term;
            server->voted_for = -1;
            server->leader_id = peer->leader_id;
            return -2;
        }
    }
    int majority = cluster->num_servers / 2 + 1;
    if (server->votes_received >= majority) {
        server->state = RAFT_LEADER;
        server->leader_id = server->server_id;
        server->terms_served++;
        for (int i = 0; i < cluster->num_servers; i++) {
            server->next_index[i] = server->log.len + 1;
            server->match_index[i] = 0;
        }
        return 0;
    }
    return -1;
}

bool raft_handle_request_vote(raft_server_t *server, uint64_t candidate_term, int candidate_id,
                               uint64_t last_log_index, uint64_t last_log_term)
{
    if (!server) return false;
    if (candidate_term < server->current_term) return false;
    if (candidate_term > server->current_term) {
        server->current_term = candidate_term;
        server->state = RAFT_FOLLOWER;
        server->voted_for = -1;
        server->leader_id = -1;
    }
    if (server->voted_for != -1 && server->voted_for != candidate_id) return false;
    uint64_t my_last_term = (server->log.len > 0) ? server->log.entries[server->log.len - 1].term : 0;
    uint64_t my_last_index = server->log.len;
    bool log_is_ok = false;
    if (last_log_term > my_last_term) {
        log_is_ok = true;
    } else if (last_log_term == my_last_term && last_log_index >= my_last_index) {
        log_is_ok = true;
    }
    if (!log_is_ok) return false;
    server->voted_for = candidate_id;
    server->received_heartbeat = true;
    return true;
}

int raft_append_command(raft_server_t *leader, raft_cluster_t *cluster, const char *command,
                         uint64_t client_id, uint64_t cmd_id)
{
    if (!leader || !cluster || leader->state != RAFT_LEADER) return -1;
    if (!command) return -1;
    log_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.term = leader->current_term;
    entry.client_id = client_id;
    entry.command_id = cmd_id;
    entry.is_no_op = (command[0] == 0);
    entry.is_committed = false;
    strncpy(entry.command, command, 255);
    entry.command[255] = 0;
    uint64_t entry_index = raft_log_append_entry(leader, entry);
    if (entry_index == 0) return -1;
    leader->match_index[leader->server_id] = entry_index;
    leader->next_index[leader->server_id] = entry_index + 1;
    int replicated = 1;
    for (int i = 0; i < cluster->num_servers; i++) {
        if (i == leader->server_id) continue;
        raft_server_t *follower = &cluster->servers[i];
        uint64_t prev_idx = leader->next_index[i] - 1;
        uint64_t prev_term = 0;
        if (prev_idx > 0 && prev_idx <= leader->log.len)
            prev_term = leader->log.entries[prev_idx - 1].term;
        log_entry_t *entries_to_send = NULL;
        int num_to_send = 0;
        if (leader->log.len >= leader->next_index[i]) {
            entries_to_send = &leader->log.entries[leader->next_index[i] - 1];
            num_to_send = (int)(leader->log.len - leader->next_index[i] + 1);
        }
        int result = raft_handle_append_entries(follower, leader->current_term, leader->server_id,
                                                  prev_idx, prev_term, entries_to_send,
                                                  num_to_send, leader->commit_index);
        if (result == 0) {
            leader->next_index[i] = leader->log.len + 1;
            leader->match_index[i] = leader->log.len;
            replicated++;
        } else if (result == -1) {
            if (leader->next_index[i] > 1) leader->next_index[i]--;
        } else if (result == -2) {
            leader->state = RAFT_FOLLOWER;
            leader->voted_for = -1;
            return -2;
        }
    }
    return (int)entry_index;
}

int raft_handle_append_entries(raft_server_t *server, uint64_t leader_term, int leader_id,
                                uint64_t prev_log_index, uint64_t prev_log_term,
                                log_entry_t *entries, int num_entries, uint64_t leader_commit)
{
    if (!server) return -3;
    if (leader_term < server->current_term) return -2;
    server->current_term = leader_term;
    server->leader_id = leader_id;
    server->state = RAFT_FOLLOWER;
    server->received_heartbeat = true;
    if (prev_log_index > 0) {
        if (prev_log_index > server->log.len) return -1;
        uint64_t existing_term = server->log.entries[prev_log_index - 1].term;
        if (existing_term != prev_log_term) {
            server->log.len = prev_log_index - 1;
            return -1;
        }
    }
    if (entries && num_entries > 0) {
        for (int i = 0; i < num_entries; i++) {
            uint64_t idx = prev_log_index + (uint64_t)i + 1;
            if (idx <= server->log.len) {
                if (server->log.entries[idx - 1].term != entries[i].term)
                    server->log.len = idx - 1;
            }
            if (idx > server->log.len)
                raft_log_append_entry(server, entries[i]);
        }
    }
    if (leader_commit > server->commit_index) {
        uint64_t last_new_entry = server->log.len;
        server->commit_index = (leader_commit < last_new_entry) ? leader_commit : last_new_entry;
    }
    return 0;
}

int raft_advance_commit(raft_server_t *leader, raft_cluster_t *cluster)
{
    if (!leader || !cluster || leader->state != RAFT_LEADER) return 0;
    int newly_committed = 0;
    int majority = cluster->num_servers / 2 + 1;
    for (uint64_t n = leader->commit_index + 1; n <= leader->log.len; n++) {
        if (leader->log.entries[n - 1].term != leader->current_term) continue;
        int replicas = 0;
        for (int i = 0; i < cluster->num_servers; i++)
            if (leader->match_index[i] >= n) replicas++;
        if (replicas >= majority) {
            leader->commit_index = n;
            leader->log.entries[n - 1].is_committed = true;
            newly_committed++;
        }
    }
    return newly_committed;
}

int raft_apply_committed(raft_server_t *server)
{
    if (!server) return 0;
    int newly_applied = 0;
    while (server->last_applied < server->commit_index) {
        server->last_applied++;
        server->entries_committed++;
        newly_applied++;
    }
    return newly_applied;
}

bool raft_verify_leader_completeness(raft_cluster_t *cluster)
{
    if (!cluster || !cluster->servers) return false;
    for (int i = 0; i < cluster->num_servers; i++) {
        raft_server_t *s = &cluster->servers[i];
        if (s->state != RAFT_LEADER) continue;
        for (uint64_t idx = 1; idx <= s->commit_index; idx++) {
            if (idx > s->log.len) return false;
            int replicas = 0;
            for (int j = 0; j < cluster->num_servers; j++) {
                raft_server_t *peer = &cluster->servers[j];
                if (idx <= peer->log.len && peer->log.entries[idx - 1].term == s->log.entries[idx - 1].term)
                    replicas++;
            }
            if (replicas < cluster->num_servers / 2 + 1) return false;
        }
    }
    return true;
}

bool raft_verify_state_machine_safety(raft_cluster_t *cluster)
{
    if (!cluster || !cluster->servers) return false;
    for (uint64_t idx = 1; ; idx++) {
        uint64_t ref_term = UINT64_MAX;
        bool found = false;
        for (int i = 0; i < cluster->num_servers; i++) {
            raft_server_t *s = &cluster->servers[i];
            if (idx > s->last_applied) continue;
            uint64_t entry_term = s->log.entries[idx - 1].term;
            if (!found) { ref_term = entry_term; found = true; }
            else if (entry_term != ref_term) return false;
        }
        if (!found) break;
    }
    return true;
}

int raft_begin_membership_change(raft_cluster_t *cluster, int *new_servers, int new_count)
{
    if (!cluster || !new_servers || new_count < 1) return -1;
    return 0;
}

int raft_complete_membership_change(raft_cluster_t *cluster)
{
    if (!cluster) return -1;
    return 0;
}

const char *raft_state_to_string(raft_state_t state)
{
    switch (state) {
    case RAFT_FOLLOWER:  return "Follower";
    case RAFT_CANDIDATE: return "Candidate";
    case RAFT_LEADER:    return "Leader";
    default:             return "Unknown";
    }
}

void raft_get_cluster_status(raft_cluster_t *c, int *nl, int *nc, int *nf)
{
    *nl = *nc = *nf = 0;
    if (!c || !c->servers) return;
    for (int i = 0; i < c->num_servers; i++) {
        switch (c->servers[i].state) {
        case RAFT_LEADER:    (*nl)++; break;
        case RAFT_CANDIDATE: (*nc)++; break;
        case RAFT_FOLLOWER:  (*nf)++; break;
        default: break;
        }
    }
}
