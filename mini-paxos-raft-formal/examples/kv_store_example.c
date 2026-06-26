/**
 * kv_store_example.c — Distributed Key-Value Store using Raft
 *
 * Demonstrates a replicated key-value store built on top of the Raft
 * consensus protocol. This is the canonical application of consensus
 * in distributed systems — replicated state machines.
 *
 * Architecture:
 *   - Raft layer: ensures all servers agree on command order
 *   - State machine: a simple in-memory key-value store
 *   - Client: proposes operations to the Raft leader
 *
 * This mirrors real systems like:
 *   - etcd (CoreOS/Kubernetes) — uses Raft
 *   - ZooKeeper (Apache) — uses Zab (Paxos variant)
 *   - Consul (HashiCorp) — uses Raft
 *
 * L7 Application: Distributed key-value store
 *
 * References:
 *   - Ongaro & Ousterhout (2014)
 *   - etcd GitHub: github.com/etcd-io/etcd
 */

#include <stdio.h>
#include <string.h>
#include "../include/paxos_raft_types.h"
#include "../include/raft_core.h"

/* ─── Simple Key-Value Store ──────────────────────────────────────── */
#define KV_MAX_KEYS 100
#define KV_KEY_SIZE 32
#define KV_VAL_SIZE 64

typedef struct {
    char key[KV_KEY_SIZE];
    char value[KV_VAL_SIZE];
    bool occupied;
} kv_entry_t;

typedef struct {
    kv_entry_t entries[KV_MAX_KEYS];
} kv_store_t;

static void kv_init(kv_store_t *kv) {
    memset(kv, 0, sizeof(*kv));
}

static void kv_put(kv_store_t *kv, const char *key, const char *value) {
    for (int i = 0; i < KV_MAX_KEYS; i++) {
        if (kv->entries[i].occupied &&
            strcmp(kv->entries[i].key, key) == 0) {
            /* Update existing */
            strncpy(kv->entries[i].value, value, KV_VAL_SIZE - 1);
            return;
        }
    }
    /* Insert new */
    for (int i = 0; i < KV_MAX_KEYS; i++) {
        if (!kv->entries[i].occupied) {
            strncpy(kv->entries[i].key, key, KV_KEY_SIZE - 1);
            strncpy(kv->entries[i].value, value, KV_VAL_SIZE - 1);
            kv->entries[i].occupied = true;
            return;
        }
    }
}

static const char *kv_get(const kv_store_t *kv, const char *key) {
    for (int i = 0; i < KV_MAX_KEYS; i++) {
        if (kv->entries[i].occupied &&
            strcmp(kv->entries[i].key, key) == 0) {
            return kv->entries[i].value;
        }
    }
    return NULL;
}

static void kv_apply_log(kv_store_t *kv, const raft_log_entry_t *log,
                          index_t log_len) {
    for (index_t i = 0; i < log_len; i++) {
        const consensus_value_t *cmd = &log[i].command;
        /* Parse command: "PUT key:value" or "GET key" */
        char cmd_str[MAX_VALUE_SIZE];
        memcpy(cmd_str, cmd->data, cmd->length);
        cmd_str[cmd->length] = '\0';

        if (strncmp(cmd_str, "PUT ", 4) == 0) {
            char key[KV_KEY_SIZE], value[KV_VAL_SIZE];
            if (sscanf(cmd_str + 4, "%31[^:]:%63s", key, value) == 2) {
                kv_put(kv, key, value);
            }
        }
    }
}

/* ─── Main Demo ────────────────────────────────────────────────────── */
int main(void) {
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║   Distributed KV Store (Raft-based)          ║\n");
    printf("║   Modeled after etcd / ZooKeeper             ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");

    const int NUM_SERVERS = 3;
    kv_store_t kv_stores[NUM_SERVERS];

    for (int i = 0; i < NUM_SERVERS; i++) {
        kv_init(&kv_stores[i]);
    }

    /* ─── Bootstrap Raft cluster ─── */
    printf("── Bootstrapping Raft cluster ──\n");

    raft_server_t servers[NUM_SERVERS];
    for (int i = 0; i < NUM_SERVERS; i++) {
        raft_server_init(&servers[i], (node_id_t)i, NUM_SERVERS,
                          150 + (uint64_t)i * 30);
    }

    /* Manual election: server 0 as leader */
    raft_become_candidate(&servers[0]);
    servers[0].persistent.state = RAFT_LEADER;
    raft_become_leader(&servers[0], NUM_SERVERS);
    servers[1].persistent.current_term = servers[0].persistent.current_term;
    servers[2].persistent.current_term = servers[0].persistent.current_term;

    printf("Server 0 elected as Leader (term %llu)\n",
           (unsigned long long)servers[0].persistent.current_term);

    /* ─── Client Operations ─── */
    printf("\n── Client Operations ──\n");

    const char *operations[] = {
        "PUT name:Alice",
        "PUT age:30",
        "PUT city:Beijing",
        "PUT status:active",
    };
    int num_ops = 4;

    raft_server_t *leader = &servers[0];

    for (int op = 0; op < num_ops; op++) {
        consensus_value_t cmd;
        memcpy(cmd.data, operations[op], strlen(operations[op]));
        cmd.length = (uint32_t)strlen(operations[op]);

        /* Leader appends command */
        index_t idx = raft_append_command(leader, &cmd);
        printf("  [%llu] %s → appended at index %llu\n",
               (unsigned long long)(op + 1), operations[op],
               (unsigned long long)idx);

        /* Replicate to followers */
        for (int f = 1; f < NUM_SERVERS; f++) {
            raft_append_entries_args_t args;
            if (raft_prepare_append_entries(leader, (node_id_t)f, &args)) {
                raft_append_entries_reply_t reply;
                raft_handle_append_entries(&servers[f], &args, &reply);
            }
        }

        /* Advance commit index */
        raft_advance_commit_index(leader, NUM_SERVERS);
    }

    /* ─── Apply commands to state machines ── */
    printf("\n── Applying commands to state machines ──\n");

    for (int i = 0; i < NUM_SERVERS; i++) {
        kv_apply_log(&kv_stores[i],
                      servers[i].persistent.log,
                      servers[i].persistent.commit_index);
        printf("  Server %d: applied %llu entries\n",
               i,
               (unsigned long long)servers[i].persistent.commit_index);
    }

    /* ─── Read from state machines ── */
    printf("\n── Reading from replicated KV stores ──\n");

    const char *keys[] = {"name", "age", "city", "status"};
    for (int k = 0; k < 4; k++) {
        printf("  Key \"%s\":\n", keys[k]);
        for (int i = 0; i < NUM_SERVERS; i++) {
            const char *val = kv_get(&kv_stores[i], keys[k]);
            printf("    Server %d → \"%s\"\n", i, val ? val : "(nil)");
        }
    }

    /* ─── Verify consistency ── */
    printf("\n── Consistency Check ──\n");
    bool consistent = true;

    for (int k = 0; k < 4; k++) {
        const char *ref = kv_get(&kv_stores[0], keys[k]);
        for (int i = 1; i < NUM_SERVERS; i++) {
            const char *val = kv_get(&kv_stores[i], keys[k]);
            if ((ref == NULL) != (val == NULL) ||
                (ref && val && strcmp(ref, val) != 0)) {
                consistent = false;
                printf("  ✗ Mismatch for key \"%s\" at server %d\n",
                       keys[k], i);
            }
        }
    }

    if (consistent) {
        printf("  ✓ All replicas are consistent!\n");
    }

    /* ─── Safety Verification ── */
    printf("\n── Protocol Safety Verification ──\n");

    if (raft_verify_state_machine_safety(servers, NUM_SERVERS)) {
        printf("  ✓ State Machine Safety holds\n");
    }

    bool logs_ok = true;
    for (int i = 0; i < NUM_SERVERS; i++) {
        for (int j = i + 1; j < NUM_SERVERS; j++) {
            if (!raft_verify_log_matching(
                    servers[i].persistent.log,
                    servers[i].persistent.log_length,
                    servers[j].persistent.log,
                    servers[j].persistent.log_length,
                    servers[i].persistent.commit_index)) {
                logs_ok = false;
            }
        }
    }
    printf("  %s Log Matching Property\n", logs_ok ? "✓" : "✗");

    printf("\n╔══════════════════════════════════════════════╗\n");
    printf("║   KV Store Demo Complete!                    ║\n");
    printf("║   %d keys replicated across %d servers       ║\n",
           4, NUM_SERVERS);
    printf("╚══════════════════════════════════════════════╝\n");

    return 0;
}
