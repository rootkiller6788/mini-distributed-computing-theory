/**
 * @file flp_common.h
 * @brief FLP Impossibility Proof -- Common Types and Constants
 *
 * This module implements the formal system model from:
 *   Fischer, Lynch, Paterson. "Impossibility of Distributed Consensus
 *   with One Faulty Process." JACM, 32(2):374-382, 1985.
 *
 * Knowledge Coverage:
 *   L1: Asynchronous system model (processes, messages, events)
 *   L1: Crash fault model (fail-stop)
 *   L2: Deterministic protocols, safety vs liveness
 *   L3: Configuration = state vector x message multiset
 *
 * Course Alignment: MIT 6.852, Stanford CS244E, CMU 15-712
 */

#ifndef FLP_COMMON_H
#define FLP_COMMON_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define FLP_MAX_PROCESSES 64
#define FLP_MAX_MESSAGES 1024
#define FLP_MAX_STATE_SIZE 256

typedef int32_t flp_process_id;
typedef int32_t flp_msg_type;
typedef int32_t flp_round_t;

typedef enum {
    FLP_DECIDE_0 = 0,
    FLP_DECIDE_1 = 1,
    FLP_UNDECIDED = -1
} flp_decision_value;

typedef enum {
    FLP_VALENCE_0 = 0,
    FLP_VALENCE_1 = 1,
    FLP_VALENCE_BIVALENT = 2,
    FLP_VALENCE_UNKNOWN = -1
} flp_valence;

typedef enum {
    FLP_PROCESS_CORRECT = 0,
    FLP_PROCESS_CRASHED = 1,
    FLP_PROCESS_SUSPECTED = 2
} flp_process_status;

typedef struct {
    int32_t max_crashes;
    bool    byzantine;
    bool    authenticated;
} flp_fault_model;

typedef struct {
    flp_process_id  sender;
    flp_process_id  receiver;
    flp_msg_type    type;
    flp_round_t     round;
    int32_t         value;
    uint8_t         payload[64];
    int32_t         payload_len;
    int32_t         msg_id;
} flp_message;

typedef struct {
    flp_process_id      pid;
    flp_process_status  status;
    flp_decision_value  decision;
    int32_t             input_value;
    int32_t             state_vars[16];
    int32_t             num_state_vars;
    flp_round_t         current_round;
    int32_t             msg_count;
    int32_t             step_count;
    bool                has_decided;
} flp_process_state;

typedef struct {
    flp_process_state   processes[FLP_MAX_PROCESSES];
    int32_t             num_processes;
    flp_message         messages[FLP_MAX_MESSAGES];
    int32_t             num_messages;
    int32_t             config_id;
    flp_valence         valence;
    int32_t             depth;
} flp_configuration;

typedef struct {
    flp_process_id  process;
    int32_t         msg_index;
    flp_message     delivered_msg;
    bool            is_null_event;
} flp_event;

typedef struct {
    flp_process_id  schedule[FLP_MAX_MESSAGES];
    int32_t         length;
} flp_schedule;

flp_fault_model flp_fault_model_init(int32_t f);
bool flp_process_is_correct(const flp_process_state *ps);
bool flp_config_has_decision(const flp_configuration *cfg);
int32_t flp_config_correct_count(const flp_configuration *cfg);
const char *flp_valence_to_string(flp_valence v);
const char *flp_decision_to_string(flp_decision_value d);

/* Initialization helpers */
void flp_message_init(flp_message *msg, flp_process_id sender,
                       flp_process_id receiver, flp_msg_type type,
                       int32_t value, flp_round_t round, int32_t msg_id);
void flp_process_state_init(flp_process_state *ps, flp_process_id pid,
                             int32_t input_value);
void flp_event_init(flp_event *ev, flp_process_id proc,
                     int32_t msg_idx, const flp_message *msg);

/* Schedule operations */
void flp_schedule_init(flp_schedule *sched);
void flp_schedule_append(flp_schedule *sched, flp_process_id pid);
void flp_schedule_append_schedule(flp_schedule *dst,
                                   const flp_schedule *src);
bool flp_schedule_contains_process(const flp_schedule *sched,
                                    flp_process_id pid);

/* Global ID generators */
int32_t flp_next_config_id(void);
void flp_reset_config_id_counter(void);

#endif /* FLP_COMMON_H */
