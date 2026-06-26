/**
 * invariant_checker.h — Inductive Invariant Checking for Consensus
 *
 * Provides systematic verification of safety invariants for Paxos and
 * Raft. An inductive invariant I is a predicate such that:
 *
 *   1. Init ⇒ I                    (I holds initially)
 *   2. I(s) ∧ Next(s, s') ⇒ I(s')  (I is preserved by every step)
 *
 * If I holds, then the safety property S (which follows from I) is
 * guaranteed in all reachable states.
 *
 * This module implements:
 *   - Inductive invariant formulation
 *   - Invariant checking via state enumeration (model checking)
 *   - Counterexample-guided invariant strengthening
 *   - Quorum-based invariant reasoning
 *
 * References:
 *   - Lamport, "The Part-Time Parliament" (1998) — inductive invariant
 *   - Ongaro, "Consensus: Bridging Theory and Practice" (2014) — Raft
 *     proof of safety
 *   - Padon et al., "Ivy: Safety Verification by Interactive
 *     Generalization" (PLDI 2016)
 *
 * Knowledge Coverage:
 *   L3 Math Structures: inductive invariants, predicate transformers
 *   L4 Fundamental Laws: safety = invariant × transitions
 *   L5 Algorithms: invariant checking, counterexample generation
 */

#ifndef INVARIANT_CHECKER_H
#define INVARIANT_CHECKER_H

#include "paxos_raft_types.h"
#include "consensus_model.h"
#include <stdbool.h>
#include <stdint.h>

/* ─── Invariant Definition ─────────────────────────────────────────── */

/**
 * L3: An invariant is a Boolean predicate over the system state.
 * It must hold in all reachable states.
 */
typedef bool (*invariant_predicate_t)(const system_state_t *state);

/**
 * L3: A transition is a relation between a state and its successor.
 * Returns true if s_next is a valid successor of s.
 */
typedef bool (*transition_relation_t)(const system_state_t *s,
                                       const system_state_t *s_next);

/**
 * L3: Inductive invariant structure.
 * An inductive invariant is a predicate I along with a proof that
 * (a) I holds in all initial states, and
 * (b) I is closed under the transition relation.
 */
typedef struct {
    const char             *name;
    const char             *description;
    invariant_predicate_t   predicate;
    transition_relation_t   transition;
    bool                    is_inductive;   /* verified inductiveness */
    int                     num_checks;     /* number of states checked */
} inductive_invariant_t;

/* ─── Paxos Invariants ─────────────────────────────────────────────── */

/**
 * L4: Paxos Agreement Invariant.
 *
 *   "At most one value is chosen."
 *
 * Formally: For any two learners ℓ₁, ℓ₂ that have learned values v₁
 * and v₂, we must have v₁ = v₂.
 *
 * This is the fundamental safety property of consensus.
 */
bool inv_paxos_agreement(const system_state_t *state);

/**
 * L4: Paxos Validity Invariant.
 *
 *   "If a value is chosen, it was proposed by some proposer."
 *
 * This prevents the trivial solution of always deciding a default value.
 */
bool inv_paxos_validity(const system_state_t *state);

/**
 * L4: Paxos Monotonicity Invariant (inductive).
 *
 *   "promise_ballot never decreases."
 *
 * This is the key inductive invariant used in Lamport's safety proof.
 * Because promise_ballot monotonically increases, the set of accepted
 * proposals satisfies the safety property.
 */
bool inv_paxos_monotonicity(const system_state_t *state);

/**
 * L4: Paxos Quorum Intersection Invariant.
 *
 *   "Any two majority quorums of acceptors intersect."
 *
 * This geometric fact is why Paxos achieves safety with f < n/2.
 * The intersecting acceptor ensures that Phase 2 cannot accept
 * conflicting values.
 */
bool inv_paxos_quorum_intersection(const system_state_t *state);

/* ─── Raft Invariants ──────────────────────────────────────────────── */

/**
 * L4: Raft Election Safety Invariant.
 *
 *   "At most one leader can be elected in a given term."
 *
 * Follows from the rule that each server votes at most once per term.
 */
bool inv_raft_election_safety(const system_state_t *state);

/**
 * L4: Raft Leader Append-Only Invariant.
 *
 *   "A leader never overwrites or deletes entries in its own log."
 *
 * Leaders only append; followers may truncate on conflict.
 */
bool inv_raft_leader_append_only(const system_state_t *state);

/**
 * L4: Raft Log Matching Invariant.
 *
 *   "If two logs contain an entry with the same index and term, then
 *    the logs are identical in all preceding entries."
 *
 * This is the key invariant for Raft's safety proof (Ongaro §5.4.1).
 */
bool inv_raft_log_matching(const system_state_t *state);

/**
 * L4: Raft Leader Completeness Invariant.
 *
 *   "If a log entry is committed in a given term, that entry is present
 *    in the logs of all leaders elected in higher terms."
 *
 * This is the most subtle Raft invariant (Ongaro §5.4.2). It's enforced
 * by the RequestVote restriction.
 */
bool inv_raft_leader_completeness(const system_state_t *state);

/**
 * L4: Raft State Machine Safety Invariant.
 *
 *   "If a server has applied a log entry at a given index to its
 *    state machine, no other server will ever apply a different log
 *    entry for the same index."
 */
bool inv_raft_state_machine_safety(const system_state_t *state);

/* ─── Invariant Checking Engine ────────────────────────────────────── */

/**
 * L5: Check whether an invariant is inductive.
 *
 * An invariant I is inductive iff:
 *   1. I holds in all initial states (base case)
 *   2. For any state s where I(s) holds, and any successor s' of s
 *      (i.e., transition(s, s') holds), I(s') also holds (inductive step)
 *
 * This function enumerates all possible states (up to a bound) to
 * verify inductiveness.
 *
 * @param inv          the invariant to check
 * @param init_states  array of initial states
 * @param num_inits    number of initial states
 * @param max_states   maximum states to enumerate
 * @return true if invariant is inductive (within the bound)
 */
bool invariant_check_inductive(inductive_invariant_t *inv,
                                const system_state_t *init_states,
                                int num_inits, int max_states);

/**
 * L5: Generate a counterexample to an inductive invariant.
 *
 * If the invariant is not inductive, this finds a state s where I(s)
 * holds but I(s') fails for some successor s'.
 *
 * @param inv            invariant to check
 * @param bad_state      output: s where I(s) holds
 * @param bad_successor  output: s' where I(s') fails
 * @return true if a counterexample was found
 */
bool invariant_find_counterexample(const inductive_invariant_t *inv,
                                    system_state_t *bad_state,
                                    system_state_t *bad_successor);

/**
 * L5: Strengthen an invariant to make it inductive.
 *
 * Given a non-inductive invariant I, find the weakest strengthening
 * I' such that:
 *   (a) I'(s) ⇒ I(s) for all s  (I' is stronger than I)
 *   (b) I' is inductive
 *
 * This is the core algorithm in many formal verification tools
 * (Ivy, UPDR, etc.).
 *
 * @param inv  the (non-inductive) invariant
 * @return number of conjuncts added to strengthen the invariant
 */
int invariant_strengthen(inductive_invariant_t *inv,
                          const system_state_t *init_states,
                          int num_inits);

/**
 * L5: Run all consensus invariants and report violations.
 *
 * @param state       current system state
 * @param violations  output array of violated invariant names
 * @return number of violated invariants (0 = all hold)
 */
int invariant_check_all(const system_state_t *state,
                         const char *violations[]);

/**
 * L5: Print a violation report with the specific invariant that failed
 * and the relevant state variables.
 */
void invariant_report_violation(const char *name, const system_state_t *state);

/* ─── Inductive Invariant for Paxos (Full) ─────────────────────────── */

/**
 * L4: The complete inductive invariant for Paxos.
 *
 * This is the invariant from Lamport's proof (Lamport 1998, 2001):
 *
 * I ≜ ∀ a : promise_ballot[a] ≥ promise_ballot_prev[a]   (mono)
 *      ∧ ∀ a₁, a₂ : (accepted_ballot[a₁] = accepted_ballot[a₂] = n)
 *                    ⇒ accepted_value[a₁] = accepted_value[a₂]
 *      ∧ ∀ n : chosen(n, v) ⇒ ∃ majority Q : ∀ a ∈ Q : accepted[a] = (n, v)
 *
 * The full invariant is: Init ⇒ I and I ∧ Next ⇒ I.
 */
inductive_invariant_t invariant_paxos_full(void);

/**
 * L4: The complete inductive invariant for Raft.
 *
 * From Ongaro's proof (Ongaro 2014, Appendix A):
 *
 * The Raft inductive invariant includes:
 *   - Election Safety
 *   - Log Matching
 *   - Leader Completeness
 *   - State Machine Safety
 *
 * These are proven by induction over the protocol steps.
 */
inductive_invariant_t invariant_raft_full(void);

/* ─── Safety Lemma Functions (from safety_proof.c) ────────────────── */

/**
 * Lemma: Any two majorities must intersect (pigeonhole principle).
 */
bool safety_intersecting_majorities_lemma(int n,
                                           const int *majority_a, int size_a,
                                           const int *majority_b, int size_b);

/**
 * Lemma: Safe value propagation in Paxos.
 */
bool safety_value_propagation_lemma(const paxos_acceptor_t *acceptors,
                                     int num_acceptors,
                                     ballot_t chosen_ballot,
                                     const consensus_value_t *chosen_value,
                                     const int *query_quorum,
                                     int query_quorum_size);

/**
 * Full Paxos safety proof check.
 */
bool paxos_safety_proof_full(const paxos_acceptor_t *acceptors,
                               int num_acceptors);

/**
 * Aggregate safety check: verify all properties on system state.
 * Returns 0 if all hold, or bitmask of violated properties.
 */
#define SAFETY_OK                    0
#define SAFETY_AGREEMENT_FAILED      (1 << 0)
#define SAFETY_VALIDITY_FAILED       (1 << 1)
#define SAFETY_LOG_MATCHING_FAILED   (1 << 2)
#define SAFETY_LEADER_COMPLETE_FAILED (1 << 3)
#define SAFETY_STATE_MACHINE_FAILED  (1 << 4)
#define SAFETY_ELECTION_FAILED       (1 << 5)

int safety_check_all(const system_state_t *state);

#endif /* INVARIANT_CHECKER_H */
