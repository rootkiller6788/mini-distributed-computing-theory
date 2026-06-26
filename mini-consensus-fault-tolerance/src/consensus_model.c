/*
 * consensus_model.c - System Model Implementations
 * L2: Synchronous, asynchronous, partially synchronous models.
 * L4: FLP impossibility analysis, round lower bounds.
 * L3: Failure detector simulation.
 */
#include "consensus_model.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void sync_model_init(sync_model_t *sm, uint64_t delta, uint64_t phi)
{
    if (!sm) return;
    sm->delta = delta;
    sm->phi = phi;
    sm->round = 0;
    sm->round_based = true;
}

void async_model_init(async_model_t *am)
{
    if (!am) return;
    am->has_failure_detector = false;
    am->is_randomized = false;
    am->termination_prob = 0.0;
}

void partial_sync_model_init(partial_sync_model_t *psm)
{
    if (!psm) return;
    psm->gst_reached = false;
    psm->gst_time = 0;
    psm->unknown_delta = 100;
    psm->bound_eventually_holds = true;
}

bool consensus_is_solvable(system_model_t model, fault_type_t faults,
                            int n, int f, bool randomized)
{
    if (n <= 0 || f < 0 || f >= n) return false;
    switch (model) {
    case MODEL_SYNCHRONOUS:
        if (faults == FAULT_BYZANTINE) return (n > 3 * f);
        return (n > f);  /* even crash-omission: need just 1 correct */
    case MODEL_ASYNCHRONOUS:
        if (faults == FAULT_NONE) return true;
        return randomized;  /* FLP: deterministic impossible, randomized possible */
    case MODEL_PARTIALLY_SYNCHRONOUS:
        if (faults == FAULT_BYZANTINE) return (n > 3 * f);
        return (n > 2 * f);  /* Paxos/Raft bound */
    default:
        return false;
    }
}

/* L4: Synchronous Round Lower Bounds
 *
 * Dolev & Strong (1983): f+1 rounds are necessary and sufficient
 * for Byzantine agreement in the synchronous model.
 *
 * For crash faults: f+1 rounds needed (Lamport & Fischer 1982).
 * For authenticated Byzantine: f+1 rounds (Dolev & Strong).
 * For unauthenticated Byzantine: f+1 rounds with n > 3f (Lamport et al.).
 */
int sync_min_rounds(int n, int f, fault_type_t fault_type)
{
    if (n <= 0 || f < 0) return -1;
    if (fault_type == FAULT_NONE) return 1;  /* trivial: 1 round */
    /* f+1 rounds needed for any fault model */
    return f + 1;
}

/* === Failure Detector Implementation === */

void failure_detector_init(failure_detector_t *fd, failure_detector_class_t cls, int n)
{
    if (!fd || n < 1) return;
    fd->fd_class = cls;
    fd->num_processes = n;
    fd->suspected = (int *)calloc((size_t)n, sizeof(int));
    fd->leader = 0;
    fd->is_accurate = (cls == FD_PERFECT_P);
}

int failure_detector_query(failure_detector_t *fd, int process_id)
{
    if (!fd || !fd->suspected || process_id < 0 || process_id >= fd->num_processes)
        return -1;
    return fd->suspected[process_id];
}

void failure_detector_update(failure_detector_t *fd, int process_id, bool suspected)
{
    if (!fd || !fd->suspected || process_id < 0 || process_id >= fd->num_processes)
        return;
    fd->suspected[process_id] = suspected ? 1 : 0;
    /* For Omega: elect the lowest non-suspected process as leader */
    if (fd->fd_class == FD_OMEGA) {
        for (int i = 0; i < fd->num_processes; i++) {
            if (!fd->suspected[i]) {
                fd->leader = i;
                return;
            }
        }
        fd->leader = -1;  /* no correct process found */
    }
}

/* === FLP Audit ===
 *
 * Audits a given configuration against the FLP impossibility result.
 * FLP (Fischer, Lynch, Paterson 1985):
 *   "In a fully asynchronous message-passing distributed system,
 *    even a single unannounced process crash makes it impossible
 *    to achieve deterministic consensus."
 */

void flp_audit_run(flp_audit_t *audit, int n, int f, system_model_t model,
                    bool randomized, bool has_fd)
{
    if (!audit) return;
    memset(audit, 0, sizeof(flp_audit_t));

    /* FLP applies only to asynchronous model with at least 1 crash fault */
    audit->flp_applies = (model == MODEL_ASYNCHRONOUS && f >= 1 && !randomized && !has_fd);

    audit->randomized_circumvention = (model == MODEL_ASYNCHRONOUS && f >= 1 && randomized);
    audit->failure_detector_circumvention = (model == MODEL_ASYNCHRONOUS && f >= 1 && has_fd);
    audit->partial_sync_circumvention = (model == MODEL_PARTIALLY_SYNCHRONOUS && f >= 1);

    if (model == MODEL_SYNCHRONOUS) {
        snprintf(audit->explanation, 512,
                 "Synchronous model: FLP does not apply. "
                 "Consensus solvable with %d processes and %d faults. "
                 "Round lower bound: f+1 = %d rounds.",
                 n, f, f + 1);
    } else if (audit->flp_applies) {
        snprintf(audit->explanation, 512,
                 "FLP IMPOSSIBILITY APPLIES: asynchronous model with %d "
                 "processes and %d crash faults. Deterministic consensus "
                 "is PROVABLY IMPOSSIBLE. Circumvention: use randomization "
                 "(probability of termination approaches 1), failure detectors "
                 "(e.g., Omega), or partial synchrony (GST assumption).",
                 n, f);
    } else if (audit->randomized_circumvention) {
        snprintf(audit->explanation, 512,
                 "Randomized circumvention of FLP: asynchronous model with "
                 "%d processes and %d faults. Using randomized consensus "
                 "(e.g., Ben-Or algorithm), termination probability -> 1 "
                 "as rounds -> infinity.", n, f);
    } else if (audit->failure_detector_circumvention) {
        snprintf(audit->explanation, 512,
                 "Failure detector circumvention of FLP: using an unreliable "
                 "failure detector (e.g., Omega) to provide eventual synchrony "
                 "assumptions. Consensus becomes solvable with %d processes.",
                 n);
    } else if (audit->partial_sync_circumvention) {
        snprintf(audit->explanation, 512,
                 "Partial synchrony circumvention: after GST, bounds on "
                 "message delay and processing hold. Consensus solvable "
                 "with n=%d, f=%d (e.g., Paxos for crash, PBFT for Byzantine).",
                 n, f);
    } else {
        snprintf(audit->explanation, 512,
                 "Consensus is trivially solvable: no faults in the system.");
    }
}

/* === Message Channel Simulation ===
 *
 * Simulates network behavior based on system model.
 */

typedef struct {
    consensus_message_t *buffer;
    int                  capacity;
    int                  count;
    int                  head;
    int                  tail;
} message_queue_t;

void mq_init(message_queue_t *mq, int capacity)
{
    if (!mq || capacity < 1) return;
    mq->buffer = (consensus_message_t *)calloc((size_t)capacity, sizeof(consensus_message_t));
    mq->capacity = capacity;
    mq->count = 0;
    mq->head = 0;
    mq->tail = 0;
}

void mq_destroy(message_queue_t *mq)
{
    if (!mq) return;
    if (mq->buffer) free(mq->buffer);
    mq->buffer = NULL;
    mq->capacity = 0;
    mq->count = 0;
}

int mq_enqueue(message_queue_t *mq, consensus_message_t msg)
{
    if (!mq || !mq->buffer || mq->count >= mq->capacity) return -1;
    mq->buffer[mq->tail] = msg;
    mq->tail = (mq->tail + 1) % mq->capacity;
    mq->count++;
    return 0;
}

int mq_dequeue(message_queue_t *mq, consensus_message_t *msg)
{
    if (!mq || !mq->buffer || mq->count <= 0) return -1;
    *msg = mq->buffer[mq->head];
    mq->head = (mq->head + 1) % mq->capacity;
    mq->count--;
    return 0;
}

/* Deliver messages with delay determined by system model.
 * Synchronous: deliver within Delta.
 * Asynchronous: may be arbitrarily delayed.
 * Partially synchronous: deliver within Delta after GST. */

int model_deliver_messages(message_queue_t *mq, consensus_node_t *nodes,
                            int num_nodes, system_model_t model, uint64_t time_now)
{
    (void)model;
    (void)time_now;
    if (!mq || !nodes) return 0;
    int delivered = 0;
    consensus_message_t msg;
    while (mq_dequeue(mq, &msg) == 0) {
        int receiver = msg.receiver_id;
        if (receiver >= 0 && receiver < num_nodes && nodes[receiver].is_alive) {
            nodes[receiver].msgs_received++;
            delivered++;
        }
    }
    return delivered;
}
