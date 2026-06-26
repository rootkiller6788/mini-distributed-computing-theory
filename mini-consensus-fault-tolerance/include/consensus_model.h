/*
 * consensus_model.h - System Models for Distributed Consensus
 * L2: Synchronous, asynchronous, partially synchronous models.
 * L4: FLP impossibility, lower bounds on rounds.
 * L3: Failure detectors (Chandra & Toueg 1996).
 *
 * Reference: Dwork, Lynch, Stockmeyer (1988) "Consensus in the Presence
 *            of Partial Synchrony"
 *            Chandra & Toueg (1996) "Unreliable Failure Detectors for
 *            Reliable Distributed Systems"
 */
#ifndef CONSENSUS_MODEL_H
#define CONSENSUS_MODEL_H

#include "consensus_types.h"
#include <stdint.h>
#include <stdbool.h>

/* === Synchronous Model ===
 * Message delay bounded by known Delta.
 * Processing time bounded by known Phi.
 * Consensus possible with n > f (for crash) or n > 3f (for Byzantine).
 * Lower bound: f+1 rounds needed (Dolev & Strong 1983). */

typedef struct {
    uint64_t delta;      /* max message delay */
    uint64_t phi;        /* max processing time */
    uint64_t round;      /* current round number */
    bool     round_based; /* in synchronous model, algorithms proceed in rounds */
} sync_model_t;

/* === Asynchronous Model ===
 * No bounds on message delay or processing time.
 * FLP: deterministic consensus impossible with even 1 crash fault.
 * Circumvention: randomization, failure detectors, partial synchrony. */

typedef struct {
    bool has_failure_detector;   /* e.g., <>W (eventually weak) */
    bool is_randomized;          /* e.g., Ben-Or (1983) */
    double termination_prob;    /* prob of termination per round (randomized) */
} async_model_t;

/* === Partially Synchronous Model (Dwork, Lynch, Stockmeyer 1988) ===
 * One of two assumptions:
 *   1) Bound Delta exists but is unknown (algorithms must work for any Delta)
 *   2) Bound Delta is known but only holds after GST (Global Stabilization Time)
 *
 * After GST, the system behaves synchronously. Consensus is solvable. */

typedef struct {
    bool     gst_reached;      /* has Global Stabilization Time been reached? */
    uint64_t gst_time;         /* when GST occurred (unknown to algorithm) */
    uint64_t unknown_delta;    /* exists but unknown bound */
    bool     bound_eventually_holds; /* model 2: bound holds after GST */
} partial_sync_model_t;

/* === Failure Detectors (Chandra & Toueg 1996) ===
 *
 * A failure detector is a distributed oracle that provides hints about
 * which processes have crashed. Classified by completeness and accuracy.
 *
 * Completeness: crashed processes are eventually suspected by all correct.
 * Accuracy: correct processes are not suspected (or only finitely often).
 *
 * Omega (Omega): Eventually, all correct processes trust the same correct
 * process. Omega is the weakest failure detector for consensus. */

typedef enum {
    FD_PERFECT_P    = 0,  /* Strong Completeness + Strong Accuracy (sync only) */
    FD_EVENTUALLY_P = 1,  /* Strong Completeness + Eventual Accuracy */
    FD_OMEGA        = 2   /* Eventual leader election (weakest for consensus) */
} failure_detector_class_t;

typedef struct {
    failure_detector_class_t fd_class;
    int    *suspected;       /* per-process: 1 if suspected, 0 if trusted */
    int     num_processes;
    int     leader;          /* for Omega: all correct trust this process */
    bool    is_accurate;
} failure_detector_t;

/* === System Model API === */

void sync_model_init(sync_model_t *sm, uint64_t delta, uint64_t phi);
void async_model_init(async_model_t *am);
void partial_sync_model_init(partial_sync_model_t *psm);

/* Check if consensus is solvable in the given model. */
bool consensus_is_solvable(system_model_t model, fault_type_t faults,
                            int n, int f, bool randomized);

/* Get the minimum number of rounds for consensus (synchronous model). */
int sync_min_rounds(int n, int f, fault_type_t fault_type);

/* Failure detector operations */
void failure_detector_init(failure_detector_t *fd, failure_detector_class_t cls, int n);
int  failure_detector_query(failure_detector_t *fd, int process_id);
void failure_detector_update(failure_detector_t *fd, int process_id, bool suspected);

/* Deep audit: check FLP conditions formally */
typedef struct {
    bool flp_applies;
    bool randomized_circumvention;
    bool failure_detector_circumvention;
    bool partial_sync_circumvention;
    char explanation[512];
} flp_audit_t;

void flp_audit_run(flp_audit_t *audit, int n, int f, system_model_t model,
                    bool randomized, bool has_fd);

#endif /* CONSENSUS_MODEL_H */
