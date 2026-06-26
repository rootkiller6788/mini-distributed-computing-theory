/**
 * consensus_model.h — Formal Consensus Model & Quorum Theory
 *
 * This module formalizes the mathematical foundations of consensus:
 * quorum systems, the consensus problem specification, and the
 * asynchronous system model in which FLP impossibility holds.
 *
 * References:
 *   - Fischer, Lynch, Paterson, "Impossibility of Distributed Consensus
 *     with One Faulty Process" (JACM 1985) — FLP theorem
 *   - Lamport, "The Part-Time Parliament" (ACM TOCS 1998)
 *   - Malkhi & Reiter, "Byzantine Quorum Systems" (1998)
 *   - Pease, Shostak, Lamport, "Reaching Agreement in the Presence of
 *     Faults" (JACM 1980) — Byzantine generals
 *
 * Knowledge Coverage:
 *   L1 Definitions: consensus problem, quorum, asynchronous model
 *   L2 Core Concepts: safety vs liveness, crash-stop vs Byzantine
 *   L3 Math Structures: quorum intersection, majority properties
 *   L4 Fundamental Laws: FLP impossibility, quorum intersection lemma
 */

#ifndef CONSENSUS_MODEL_H
#define CONSENSUS_MODEL_H

#include "paxos_raft_types.h"

/* ─── System Models ──────────────────────────────────────────────────
 * L1 Definition: Three canonical system models for distributed computing
 *
 * Synchronous:   message delay ≤ known bound Δ, process speed bounded
 * Asynchronous:  no bounds on message delay or process speed
 * Partially synchronous: eventually synchronous (known bound after GST)
 *
 * FLP shows consensus is impossible in purely asynchronous systems
 * with even one crash failure. Paxos/Raft work in partial synchrony.
 */

typedef enum {
    MODEL_SYNCHRONOUS,
    MODEL_ASYNCHRONOUS,
    MODEL_PARTIALLY_SYNCHRONOUS,
} system_model_t;

/**
 * L1 Definition: Fault model types.
 * Crash-stop: a faulty process simply stops (Paxos/Raft assumption)
 * Byzantine: a faulty process can behave arbitrarily
 * Omission: a faulty process may drop messages
 */
typedef enum {
    FAULT_CRASH_STOP,
    FAULT_BYZANTINE,
    FAULT_OMISSION,
} fault_model_t;

/**
 * L1 Definition: Consensus problem specification.
 *
 * Given n processes, each proposing a value, a consensus protocol must satisfy:
 *   (C1) Agreement:    All correct processes decide the same value.
 *   (C2) Validity:     The decided value was proposed by some process.
 *   (C3) Termination:  Every correct process eventually decides.
 *
 * In asynchronous systems with crash faults, FLP proves (C1)+(C2)+(C3)
 * cannot all be satisfied simultaneously. Paxos/Raft satisfy (C1)+(C2)
 * (safety) unconditionally and (C3) (liveness) under partial synchrony.
 */
typedef struct {
    node_id_t proposers[MAX_NODES];
    int       num_proposers;
    consensus_value_t proposed_values[MAX_NODES];
    int       num_proposed;
    consensus_value_t decided_value;
    bool      decided;
    system_model_t model;
    fault_model_t   fault_model;
} consensus_problem_t;

/* ─── Quorum Theory ──────────────────────────────────────────────────
 * L3 Math Structure: A quorum system Q is a set of subsets (quorums)
 * of processes such that:
 *   ∀ Q1, Q2 ∈ Q : Q1 ∩ Q2 ≠ ∅   (Intersection property)
 *
 * For majority quorum systems (Paxos/Raft):
 *   |Q| > n/2 for each Q ∈ Q
 *   |Q1 ∩ Q2| ≥ 1 guaranteed
 */

/**
 * L3: Initialize a majority quorum system for n nodes.
 *
 * Theorem (Majority Quorum Intersection):
 *   For any two subsets A, B of {1..n} with |A| > n/2 and |B| > n/2,
 *   we have A ∩ B ≠ ∅.
 *
 * Proof: |A ∪ B| = |A| + |B| - |A ∩ B| ≤ n
 *        ⇒ |A ∩ B| = |A| + |B| - |A ∪ B| ≥ n/2+1 + n/2+1 - n = 2
 *        Thus |A ∩ B| ≥ 2 ≥ 1. ∎
 */
void quorum_system_init(quorum_system_t *qs, int n);

/**
 * L3: Check whether a given subset is a valid (majority) quorum.
 * Returns true iff |subset| > n/2.
 */
bool quorum_is_valid(const quorum_system_t *qs, const quorum_t *q);

/**
 * L3: Check the intersection property: do any two quorums in the system
 * have non-empty intersection? This is the fundamental safety guarantee.
 *
 * Lemma: ∀ Q1, Q2 ∈ Q, Q1 ∩ Q2 ≠ ∅
 */
bool quorum_intersection_holds(const quorum_system_t *qs);

/**
 * L3: Compute the intersection of two quorums.
 * Returns the size of the intersection.
 * Used in safety proofs to show at least one correct node witnesses
 * protocol decisions.
 */
int quorum_intersection_size(const quorum_t *a, const quorum_t *b,
                              int *intersection_out);

/**
 * L3: Generate all quorums (all majority subsets) for a given n.
 * The number of majority subsets = Σ_{k=⌊n/2⌋+1}^{n} C(n,k).
 *
 * For n=5: C(5,3)+C(5,4)+C(5,5) = 10+5+1 = 16 quorums.
 */
int quorum_generate_all(quorum_system_t *qs);

/**
 * L4 FLP Impossibility: Check if a configuration is bivalent.
 *
 * A configuration C is 0-valent if all possible schedules from C
 * lead to decision 0. It is 1-valent if all lead to decision 1.
 * It is bivalent if neither holds — meaning the decision is not
 * yet determined.
 *
 * FLP constructs an initial bivalent configuration and shows there
 * exists an infinite schedule that never reaches a decision.
 *
 * @param values: array of n proposed values (0 or 1)
 * @param n: number of processes
 * @return true if initial config is bivalent
 */
bool flp_is_bivalent_initial(const int *values, int n, int f);

/**
 * L4: FLP — Check if a configuration is 0-valent/1-valent/bivalent
 * under all possible single-step schedules.
 */
typedef enum { VALENCE_0 = 0, VALENCE_1 = 1, VALENCE_BIVALENT = -1 } valence_t;

valence_t flp_valence(const int *proposed_values, int n, int f,
                       const int *crashed, int num_crashed,
                       const int *decided, int num_decided);

/**
 * L2 Core Concept: Partial synchrony model parameter.
 * GST = Global Stabilization Time, after which the system behaves
 * synchronously (message delays bounded by Δ).
 *
 * This is the model in which Paxos/Raft achieve liveness.
 */
typedef struct {
    system_model_t model;
    uint64_t       gst_ms;       /* global stabilization time (∞ = async) */
    uint64_t       delta_ms;     /* message delay bound after GST */
    double         clock_drift;  /* maximum clock drift ratio between nodes */
} partial_synchrony_model_t;

/* ─── Quorum Utility Functions ────────────────────────────────────── */

/**
 * Check if a node is in a quorum.
 */
bool quorum_contains(const quorum_t *q, node_id_t node);

/**
 * Compute the maximum number of faults a quorum system can tolerate.
 */
int quorum_max_fault_tolerance(const quorum_system_t *qs);

/**
 * Check if a set of responding nodes constitutes a quorum.
 */
bool quorum_achieved(const bool *responders, int n, const quorum_system_t *qs);

/**
 * Count the number of quorums containing a given node.
 */
int quorum_count_containing_node(const quorum_system_t *qs, node_id_t node);

/* ─── Byzantine Quorum Functions ──────────────────────────────────── */

/**
 * L8: Byzantine quorum size = 2f + 1.
 */
int byzantine_quorum_size(int f);

/**
 * L8: Total nodes needed for f Byzantine faults: 3f + 1.
 */
int byzantine_total_nodes(int f);

/**
 * L8: Verify Byzantine quorum intersection property.
 */
bool byzantine_quorum_intersection_check(int total_nodes,
                                          const quorum_t *a,
                                          const quorum_t *b);

/**
 * L8: Check if n nodes suffice for f Byzantine faults.
 */
bool byzantine_nodes_sufficient(int n, int f);

/**
 * L8: Compute max Byzantine faults tolerated by n nodes.
 */
int byzantine_max_faults(int n);

#endif /* CONSENSUS_MODEL_H */
