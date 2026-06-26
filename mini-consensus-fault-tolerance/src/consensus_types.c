/*
 * consensus_types.c - Implementation of core consensus type operations.
 * L1-L4: Fault models, consensus properties, FLP impossibility check.
 */

#include "consensus_types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Fault type names for diagnostics */
const char *fault_type_names[5] = {
    "NONE",
    "CRASH",
    "OMISSION",
    "TIMING",
    "BYZANTINE"
};

/* === L1: Consensus Configuration Initialization === */

int consensus_config_init(consensus_config_t *cfg, int n, int f,
                           fault_type_t fault_model, system_model_t sys_model)
{
    if (!cfg || n < 1 || f < 0 || f >= n)
        return -1;

    cfg->num_nodes = n;
    cfg->max_faulty = f;
    cfg->fault_model = fault_model;
    cfg->system_model = sys_model;
    cfg->allow_leader_change = true;
    cfg->timeout_ms = 1000;  /* default 1 second timeout */

    cfg->nodes = (consensus_node_t *)calloc((size_t)n, sizeof(consensus_node_t));
    if (!cfg->nodes)
        return -1;

    for (int i = 0; i < n; i++) {
        cfg->nodes[i].node_id = i;
        cfg->nodes[i].fault_status = FAULT_NONE;
        cfg->nodes[i].decide_state = STATE_UNDECIDED;
        cfg->nodes[i].decided_value = -1;
        cfg->nodes[i].current_term = 0;
        cfg->nodes[i].current_view = 0;
        cfg->nodes[i].is_leader = false;
        cfg->nodes[i].is_alive = true;
        cfg->nodes[i].msgs_sent = 0;
        cfg->nodes[i].msgs_received = 0;
        cfg->nodes[i].rounds_participated = 0;

        /* Initialize log */
        cfg->nodes[i].log.entries = NULL;
        cfg->nodes[i].log.length = 0;
        cfg->nodes[i].log.capacity = 0;
        cfg->nodes[i].log.commit_index = 0;
        cfg->nodes[i].log.last_applied = 0;
    }

    return n;
}

void consensus_config_destroy(consensus_config_t *cfg)
{
    if (!cfg) return;
    if (cfg->nodes) {
        for (int i = 0; i < cfg->num_nodes; i++) {
            if (cfg->nodes[i].log.entries)
                free(cfg->nodes[i].log.entries);
        }
        free(cfg->nodes);
        cfg->nodes = NULL;
    }
    cfg->num_nodes = 0;
}

/* === L4: Fault Tolerance Bound Check === */

int consensus_bound_check(int n, int f, fault_type_t model)
{
    if (n <= 0 || f < 0 || f >= n)
        return 0;  /* impossible */

    switch (model) {
    case FAULT_NONE:
        return 1;  /* always possible with no faults */
    case FAULT_CRASH:
    case FAULT_OMISSION:
    case FAULT_TIMING:
        /* Paxos/Raft style: need n > 2f for consensus */
        return (n > 2 * f) ? 1 : 0;
    case FAULT_BYZANTINE:
        /* PBFT style: need n > 3f for consensus */
        return (n > 3 * f) ? 2 : 0;
    default:
        return 0;
    }
}

/* === L4: FLP Impossibility Theorem Check ===
 *
 * Fischer, Lynch, Paterson (1985): In an asynchronous system,
 * it is impossible to achieve deterministic consensus with even
 * a single crash fault.
 *
 * The result is circumvented by:
 *   - Using randomization (Ben-Or 1983)
 *   - Using failure detectors (Chandra & Toueg 1996)
 *   - Using partial synchrony (Dwork, Lynch, Stockmeyer 1988)
 */

bool flp_impossible(int n, fault_type_t ft, system_model_t sm, bool is_randomized)
{
    /* FLP only applies to the asynchronous model */
    if (sm != MODEL_ASYNCHRONOUS)
        return false;

    /* FLP requires at least one fault */
    if (ft == FAULT_NONE)
        return false;

    /* FLP only restricts deterministic algorithms */
    if (is_randomized)
        return false;

    /* n >= 2 still impossible: FLP holds for any n with >= 1 crash fault */
    if (n >= 2 && ft != FAULT_NONE)
        return true;

    return false;
}

/* === L1: Diagnostic string functions === */

const char *fault_type_to_string(fault_type_t ft)
{
    if (ft >= 0 && ft <= 4)
        return fault_type_names[ft];
    return "UNKNOWN";
}

const char *system_model_to_string(system_model_t sm)
{
    switch (sm) {
    case MODEL_SYNCHRONOUS:           return "Synchronous";
    case MODEL_ASYNCHRONOUS:          return "Asynchronous";
    case MODEL_PARTIALLY_SYNCHRONOUS: return "Partially Synchronous";
    default:                          return "Unknown";
    }
}

/* === L2: Consensus Property Verifier ===
 *
 * Verifies the three consensus properties on a set of nodes.
 * Note: This checks instantaneous state, not execution traces.
 */

int consensus_verify_properties(consensus_node_t *nodes, int num_nodes,
                                 consensus_safety_t *safety)
{
    if (!nodes || !safety || num_nodes < 1)
        return -1;

    safety->agreement_holds = true;
    safety->validity_holds = true;
    safety->termination_holds = true;
    safety->violated_property = -1;
    safety->violation_description[0] = '\0';

    int first_decided_value = -1;
    bool first_found = false;

    for (int i = 0; i < num_nodes; i++) {
        if (nodes[i].fault_status != FAULT_NONE || !nodes[i].is_alive)
            continue;

        /* Check termination: all correct nodes should have decided */
        if (nodes[i].decide_state != STATE_DECIDED) {
            safety->termination_holds = false;
            if (safety->violated_property == -1) {
                safety->violated_property = PROP_TERMINATION;
                snprintf(safety->violation_description, 256,
                         "Correct node %d has not decided (state=%d)",
                         nodes[i].node_id, nodes[i].decide_state);
            }
        }

        /* Check agreement: all correct nodes must decide the same value */
        int dv = nodes[i].decided_value;
        if (dv >= 0) {
            if (!first_found) {
                first_decided_value = dv;
                first_found = true;
            } else if (dv != first_decided_value) {
                safety->agreement_holds = false;
                if (safety->violated_property == -1) {
                    safety->violated_property = PROP_AGREEMENT;
                    snprintf(safety->violation_description, 256,
                             "Agreement violated: node %d decided %d but node "
                             "with first decision decided %d",
                             nodes[i].node_id, dv, first_decided_value);
                }
            }
        }

        /* Check integrity: at most one decision per node (by construction) */
    }

    return 0;
}

/* === L2: Initialize a message with default values === */

void consensus_message_init(consensus_message_t *msg)
{
    if (!msg) return;
    memset(msg, 0, sizeof(consensus_message_t));
    msg->sender_id = -1;
    msg->receiver_id = -1;
    msg->proposed_value = -1;
    msg->timestamp = 0;
}

/* === L3: Replicated Log Operations === */

int replicated_log_append(replicated_log_t *log, log_entry_t entry)
{
    if (!log) return -1;

    /* Grow capacity if needed */
    if (log->length >= log->capacity) {
        uint64_t new_cap = log->capacity == 0 ? 16 : log->capacity * 2;
        log_entry_t *new_entries = (log_entry_t *)realloc(
            log->entries, (size_t)new_cap * sizeof(log_entry_t));
        if (!new_entries) return -1;
        log->entries = new_entries;
        log->capacity = new_cap;
    }

    log->entries[log->length] = entry;
    log->length++;
    return 0;
}

log_entry_t replicated_log_get(replicated_log_t *log, uint64_t index)
{
    log_entry_t empty;
    memset(&empty, 0, sizeof(empty));

    if (!log || index == 0 || index > log->length)
        return empty;

    return log->entries[index - 1];
}

int replicated_log_truncate(replicated_log_t *log, uint64_t from_index)
{
    if (!log || from_index > log->length)
        return -1;

    /* Truncate all entries from from_index to end */
    log->length = from_index - 1;

    /* Adjust commit_index and last_applied if needed */
    if (log->commit_index >= from_index)
        log->commit_index = from_index > 0 ? from_index - 1 : 0;
    if (log->last_applied >= from_index)
        log->last_applied = from_index > 0 ? from_index - 1 : 0;

    return 0;
}

/* === L2: View Management === */

void view_state_init(view_state_t *view)
{
    if (!view) return;
    view->view_number = 0;
    view->leader_id = -1;
    view->start_time = 0;
    view->is_stable = false;
}

int view_state_advance(view_state_t *view, int new_leader)
{
    if (!view) return -1;
    view->view_number++;
    view->leader_id = new_leader;
    view->start_time = 0;  /* will be set by caller */
    view->is_stable = false;
    return 0;
}
