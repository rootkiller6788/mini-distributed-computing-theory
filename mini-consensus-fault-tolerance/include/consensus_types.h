/*
 * consensus_types.h �� Core Type Definitions for Consensus & Fault Tolerance
 *
 * L1 Definitions: Byzantine fault, crash fault, consensus, safety,
 * liveness, quorum, view, leader, term, log entry, state machine replication.
 *
 * Reference: Lamport, Shostak, Pease (1982) The Byzantine Generals Problem
 *            Fischer, Lynch, Paterson (1985) Impossibility of Distributed Consensus
 */

#ifndef CONSENSUS_TYPES_H
#define CONSENSUS_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ================================================================
 * L1: Fault Models
 * ================================================================ */

typedef enum {
    FAULT_NONE        = 0,
    FAULT_CRASH       = 1,
    FAULT_OMISSION    = 2,
    FAULT_TIMING      = 3,
    FAULT_BYZANTINE   = 4
} fault_type_t;

extern const char *fault_type_names[5];

/*
 * L4 Fundamental Laws: Fault tolerance bounds.
 * - Crash faults: n > 2f (Paxos/Raft)
 * - Byzantine:    n > 3f (PBFT / Lamport et al.)
 */
#define CRASH_TOLERANCE_BOUND(n,f)     (((n) > 2 * (f)) ? 1 : 0)
#define BYZANTINE_TOLERANCE_BOUND(n,f) (((n) > 3 * (f)) ? 1 : 0)

typedef enum {
    PROP_TERMINATION = 0,
    PROP_AGREEMENT   = 1,
    PROP_VALIDITY    = 2,
    PROP_INTEGRITY   = 3
} consensus_property_t;

typedef enum {
    STATE_UNDECIDED = 0,
    STATE_PROPOSED  = 1,
    STATE_ACCEPTED  = 2,
    STATE_DECIDED   = 3
} decision_state_t;

typedef enum {
    MODEL_SYNCHRONOUS            = 0,
    MODEL_ASYNCHRONOUS           = 1,
    MODEL_PARTIALLY_SYNCHRONOUS  = 2
} system_model_t;

typedef struct flp_config {
    int            num_processes;
    fault_type_t   fault_model;
    system_model_t system;
    bool           is_randomized;
    bool           is_impossible;
    const char    *proof_reference;
} flp_config_t;

typedef struct {
    int  quorum_id;
    int  members[64];
    int  num_members;
    int  weight;
} quorum_t;

typedef struct {
    quorum_t *quorums;
    int       num_quorums;
    int       total_processes;
    int       quorum_size;
    bool      has_intersection;
} quorum_system_t;

typedef struct {
    uint64_t term;
    uint64_t index;
    uint64_t client_id;
    uint64_t command_id;
    char     command[256];
    bool     is_no_op;
    bool     is_committed;
} log_entry_t;

typedef struct {
    log_entry_t *entries;
    uint64_t     length;
    uint64_t     capacity;
    uint64_t     commit_index;
    uint64_t     last_applied;
} replicated_log_t;

typedef struct {
    uint64_t view_number;
    int      leader_id;
    uint64_t start_time;
    bool     is_stable;
} view_state_t;

typedef enum {
    BYZ_LOYAL                = 0,
    BYZ_SILENT               = 1,
    BYZ_WRONG_ANSWER         = 2,
    BYZ_CONFLICTING_ANSWERS  = 3,
    BYZ_COLLUSION            = 4
} byzantine_behavior_t;

typedef struct {
    int  sender;
    int  value;
    int  round;
    int  received_from[64];
    int  path_length;
} oral_message_t;

typedef enum {
    MSG_PREPARE       = 0,
    MSG_PROMISE       = 1,
    MSG_ACCEPT        = 2,
    MSG_ACCEPTED      = 3,
    MSG_COMMIT        = 4,
    MSG_REQUEST_VOTE  = 5,
    MSG_VOTE          = 6,
    MSG_APPEND_ENTRIES= 7,
    MSG_APPEND_RESP   = 8,
    MSG_PRE_PREPARE   = 9,
    MSG_VIEW_CHANGE   = 10,
    MSG_NEW_VIEW      = 11,
    MSG_CHECKPOINT    = 12,
    MSG_CLIENT_REQ    = 13,
    MSG_CLIENT_REPLY  = 14,
    MSG_BLOCK_PROPOSE = 15,
    MSG_BLOCK_VOTE    = 16
} message_type_t;

typedef struct {
    message_type_t type;
    int            sender_id;
    int            receiver_id;
    uint64_t       term;
    uint64_t       view;
    uint64_t       seq_num;
    uint64_t       log_index;
    uint64_t       prev_log_index;
    uint64_t       prev_log_term;
    int            proposed_value;
    int64_t        timestamp;
    bool           success;
    char           data[128];
} consensus_message_t;

typedef struct {
    bool agreement_holds;
    bool validity_holds;
    bool termination_holds;
    int  violated_property;
    char violation_description[256];
} consensus_safety_t;

typedef struct {
    int              node_id;
    fault_type_t     fault_status;
    decision_state_t decide_state;
    int              decided_value;
    uint64_t         current_term;
    uint64_t         current_view;
    replicated_log_t log;
    bool             is_leader;
    bool             is_alive;
    uint64_t         msgs_sent;
    uint64_t         msgs_received;
    uint64_t         rounds_participated;
} consensus_node_t;

typedef struct {
    int              num_nodes;
    consensus_node_t *nodes;
    int              max_faulty;
    fault_type_t     fault_model;
    system_model_t   system_model;
    bool             allow_leader_change;
    uint64_t         timeout_ms;
} consensus_config_t;

/* Constructor / Destructor */

int  consensus_config_init(consensus_config_t *cfg, int n, int f,
                            fault_type_t fault_model, system_model_t sys_model);
void consensus_config_destroy(consensus_config_t *cfg);
int  consensus_bound_check(int n, int f, fault_type_t model);
const char *fault_type_to_string(fault_type_t ft);
const char *system_model_to_string(system_model_t sm);
bool flp_impossible(int n, fault_type_t ft, system_model_t sm, bool is_randomized);

/* Replicated log operations */
int          replicated_log_append(replicated_log_t *log, log_entry_t entry);
log_entry_t  replicated_log_get(replicated_log_t *log, uint64_t index);
int          replicated_log_truncate(replicated_log_t *log, uint64_t from_index);

/* Message helpers */
void consensus_message_init(consensus_message_t *msg);

/* View management */
void view_state_init(view_state_t *view);
int  view_state_advance(view_state_t *view, int new_leader);

/* Consensus property verification */
int  consensus_verify_properties(consensus_node_t *nodes, int num_nodes,
                                  consensus_safety_t *safety);

#endif /* CONSENSUS_TYPES_H */
