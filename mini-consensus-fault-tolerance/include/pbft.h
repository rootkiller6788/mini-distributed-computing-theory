/*
 * pbft.h - Practical Byzantine Fault Tolerance (Castro & Liskov 1999)
 * L5: PBFT algorithm - tolerates Byzantine faults with 3f+1 replicas.
 * L4: Safety proof via quorum intersection.
 * Reference: Castro & Liskov (1999) "Practical Byzantine Fault Tolerance"
 */
#ifndef PBFT_H
#define PBFT_H
#include "consensus_types.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    PBFT_PHASE_IDLE = 0, PBFT_PHASE_PRE_PREPARE = 1,
    PBFT_PHASE_PREPARE = 2, PBFT_PHASE_COMMIT = 3, PBFT_PHASE_REPLY = 4
} pbft_phase_t;

typedef struct {
    int        replica_id;
    uint64_t   current_view;
    uint64_t   last_seq_num;
    uint64_t   low_water_mark;
    uint64_t   high_water_mark;
    bool      *pre_prepared;
    int       *prepare_count;
    int       *commit_count;
    int       *prepared_cert;
    uint64_t   last_checkpoint;
    uint8_t   *checkpoint_digest;
    int        total_replicas;
    int        max_faulty;
    int        quorum_size;
    consensus_message_t *msg_log;
    int                   msg_log_capacity;
    int                   msg_log_count;
    void      *service_state;
    uint64_t   last_executed_seq;
} pbft_replica_t;

typedef struct {
    pbft_replica_t *replicas;
    int             num_replicas;
    int             max_faulty;
    int             checkpoint_interval;
    int             log_checkpoint_win;
    uint64_t        request_timeout;
    bool            is_running;
} pbft_system_t;

int  pbft_system_init(pbft_system_t *sys, int num_replicas, int max_faulty);
void pbft_system_destroy(pbft_system_t *sys);
int  pbft_send_pre_prepare(pbft_system_t *sys, int primary_id, int client_id, int request_value);
bool pbft_handle_pre_prepare(pbft_replica_t *replica, consensus_message_t *msg);
consensus_message_t pbft_send_prepare(pbft_replica_t *replica);
bool pbft_handle_prepare(pbft_replica_t *replica, consensus_message_t *msg);
consensus_message_t pbft_send_commit(pbft_replica_t *replica);
bool pbft_handle_commit(pbft_replica_t *replica, consensus_message_t *msg);
int  pbft_execute_request(pbft_replica_t *replica, uint64_t seq_num);
bool pbft_verify_safety(pbft_system_t *sys);
int  pbft_initiate_view_change(pbft_replica_t *replica, pbft_system_t *sys);
bool pbft_handle_view_change(pbft_replica_t *replica, consensus_message_t *msg);
int  pbft_send_new_view(pbft_system_t *sys, int new_primary_id);
int  pbft_create_checkpoint(pbft_replica_t *replica);
bool pbft_handle_checkpoint(pbft_replica_t *replica, consensus_message_t *msg);
bool pbft_is_making_progress(pbft_system_t *sys, uint64_t timeout_ms);
void pbft_get_stats(pbft_system_t *sys, int *committed, int *pending);

#endif /* PBFT_H */
