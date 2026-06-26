/**
 * @file flp_common.c
 * @brief Common utilities for the FLP impossibility proof framework.
 *
 * Implements basic operations on fault models, process states,
 * and configuration-level queries.
 *
 * Reference: Fischer, Lynch, Paterson (1985), JACM 32(2):374-382.
 */

#include "flp_common.h"
#include <string.h>

/* ---------------------------------------------------------------------------
 * Fault Model
 * ---------------------------------------------------------------------------
 * The FLP proof assumes crash failures (fail-stop model).
 * At most one process may crash; the proof holds for any f >= 1.
 */

flp_fault_model flp_fault_model_init(int32_t f)
{
    flp_fault_model fm;
    fm.max_crashes = f;
    fm.byzantine = false;
    fm.authenticated = false;
    return fm;
}

/* ---------------------------------------------------------------------------
 * Process Status Queries
 * ---------------------------------------------------------------------------
 */

bool flp_process_is_correct(const flp_process_state *ps)
{
    if (ps == NULL) return false;
    return ps->status == FLP_PROCESS_CORRECT;
}

/* ---------------------------------------------------------------------------
 * Configuration-Level Queries
 * ---------------------------------------------------------------------------
 */

bool flp_config_has_decision(const flp_configuration *cfg)
{
    if (cfg == NULL) return false;
    for (int32_t i = 0; i < cfg->num_processes; i++) {
        if (flp_process_is_correct(&cfg->processes[i]) &&
            cfg->processes[i].has_decided) {
            return true;
        }
    }
    return false;
}

int32_t flp_config_correct_count(const flp_configuration *cfg)
{
    if (cfg == NULL) return 0;
    int32_t count = 0;
    for (int32_t i = 0; i < cfg->num_processes; i++) {
        if (flp_process_is_correct(&cfg->processes[i])) {
            count++;
        }
    }
    return count;
}

/* ---------------------------------------------------------------------------
 * Display Utilities
 * ---------------------------------------------------------------------------
 */

const char *flp_valence_to_string(flp_valence v)
{
    switch (v) {
        case FLP_VALENCE_0:        return "0-valent";
        case FLP_VALENCE_1:        return "1-valent";
        case FLP_VALENCE_BIVALENT: return "bivalent";
        case FLP_VALENCE_UNKNOWN:  return "unknown";
        default:                   return "INVALID";
    }
}

const char *flp_decision_to_string(flp_decision_value d)
{
    switch (d) {
        case FLP_DECIDE_0:    return "0";
        case FLP_DECIDE_1:    return "1";
        case FLP_UNDECIDED:   return "UNDECIDED";
        default:              return "INVALID";
    }
}

/* ---------------------------------------------------------------------------
 * Message Utilities
 * ---------------------------------------------------------------------------
 * Helper to construct messages easily.
 */

void flp_message_init(flp_message *msg, flp_process_id sender,
                       flp_process_id receiver, flp_msg_type type,
                       int32_t value, flp_round_t round, int32_t msg_id)
{
    if (msg == NULL) return;
    memset(msg, 0, sizeof(*msg));
    msg->sender = sender;
    msg->receiver = receiver;
    msg->type = type;
    msg->value = value;
    msg->round = round;
    msg->msg_id = msg_id;
    msg->payload_len = 0;
}

/* ---------------------------------------------------------------------------
 * Process State Utilities
 * ---------------------------------------------------------------------------
 */

void flp_process_state_init(flp_process_state *ps, flp_process_id pid,
                             int32_t input_value)
{
    if (ps == NULL) return;
    memset(ps, 0, sizeof(*ps));
    ps->pid = pid;
    ps->status = FLP_PROCESS_CORRECT;
    ps->decision = FLP_UNDECIDED;
    ps->input_value = input_value;
    ps->num_state_vars = 0;
    ps->current_round = 0;
    ps->msg_count = 0;
    ps->step_count = 0;
    ps->has_decided = false;
}

/* ---------------------------------------------------------------------------
 * Event Utilities
 * ---------------------------------------------------------------------------
 */

void flp_event_init(flp_event *ev, flp_process_id proc,
                     int32_t msg_idx, const flp_message *msg)
{
    if (ev == NULL) return;
    ev->process = proc;
    ev->msg_index = msg_idx;
    ev->is_null_event = (msg == NULL);
    if (msg != NULL) {
        memcpy(&ev->delivered_msg, msg, sizeof(flp_message));
    } else {
        memset(&ev->delivered_msg, 0, sizeof(flp_message));
    }
}

/* ---------------------------------------------------------------------------
 * Schedule Utilities
 * ---------------------------------------------------------------------------
 */

void flp_schedule_init(flp_schedule *sched)
{
    if (sched == NULL) return;
    memset(sched, 0, sizeof(*sched));
}

void flp_schedule_append(flp_schedule *sched, flp_process_id pid)
{
    if (sched == NULL) return;
    if (sched->length >= FLP_MAX_MESSAGES) return;
    sched->schedule[sched->length++] = pid;
}

void flp_schedule_append_schedule(flp_schedule *dst,
                                   const flp_schedule *src)
{
    if (dst == NULL || src == NULL) return;
    for (int32_t i = 0; i < src->length; i++) {
        flp_schedule_append(dst, src->schedule[i]);
    }
}

bool flp_schedule_contains_process(const flp_schedule *sched,
                                    flp_process_id pid)
{
    if (sched == NULL) return false;
    for (int32_t i = 0; i < sched->length; i++) {
        if (sched->schedule[i] == pid) return true;
    }
    return false;
}

/* ---------------------------------------------------------------------------
 * Configuration ID Generator
 * ---------------------------------------------------------------------------
 * Each configuration gets a unique ID for hashing and comparison.
 */

static int32_t g_next_config_id = 0;

int32_t flp_next_config_id(void)
{
    return g_next_config_id++;
}

void flp_reset_config_id_counter(void)
{
    g_next_config_id = 0;
}
