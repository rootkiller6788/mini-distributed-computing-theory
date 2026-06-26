/**
 * tla_encoding.h — TLA+ Encoding & Model Checking
 *
 * Encodes the Paxos/Raft consensus protocols in a TLA+-like formal
 * specification framework. Provides model checking through state space
 * exploration to verify safety invariants.
 *
 * TLA+ (Temporal Logic of Actions) is a formal specification language
 * developed by Leslie Lamport for describing and reasoning about
 * concurrent and distributed systems. It uses:
 *   - State predicates (invariants)
 *   - Action formulas (state transitions)
 *   - Temporal formulas (liveness properties)
 *
 * This module provides a C-based model checker that enumerates states
 * and checks invariants — equivalent to TLC (the TLA+ model checker)
 * for small finite models.
 *
 * References:
 *   - Lamport, "Specifying Systems" (Addison-Wesley, 2002)
 *   - Lamport, "The TLA+ Video Course" (2018)
 *   - Newcombe et al., "How Amazon Web Services Uses Formal Methods"
 *     (CACM 2015) — TLA+ in industry
 *
 * Knowledge Coverage:
 *   L3 Math Structures: TLA+ action formulas, temporal logic
 *   L5 Algorithms: Model checking via state space exploration
 *   L7 Applications: AWS use of TLA+ (S3, DynamoDB)
 *   L8 Advanced Topics: BFS state enumeration, symmetry reduction
 */

#ifndef TLA_ENCODING_H
#define TLA_ENCODING_H

#include "paxos_raft_types.h"
#include "consensus_model.h"
#include <stdbool.h>
#include <stdint.h>

/* ─── TLA+ State Variables ───────────────────────────────────────────
 * L3: In TLA+, the state of a system is an assignment of values to
 * variables. Here we encode the Paxos/Raft state as TLA+ variables.
 */

typedef enum {
    TLA_VAR_BALLOT,          /* ballot number (Paxos) */
    TLA_VAR_ACCEPTED,        /* set of accepted proposals */
    TLA_VAR_PROMISED,        /* set of promised ballots */
    TLA_VAR_DECIDED,         /* decided value (or nil) */
    TLA_VAR_TERM,            /* current term (Raft) */
    TLA_VAR_STATE,           /* server state (Follower/Candidate/Leader) */
    TLA_VAR_LOG,             /* replicated log */
    TLA_VAR_COMMIT_INDEX,    /* commit index */
    TLA_VAR_VOTED_FOR,       /* votedFor */
} tla_variable_t;

#define TLA_MAX_VARS 16

/**
 * L3: A TLA+ state — assignment of values to all variables.
 */
typedef struct {
    ballot_t  ballot;
    int       num_accepted;
    paxos_proposal_t accepted[256];
    ballot_t  promised[MAX_NODES];
    int       num_promised;
    consensus_value_t decided;
    bool      has_decided;
    term_t    term;
    raft_state_t raft_state[MAX_NODES];
    raft_log_entry_t log[MAX_NODES][MAX_LOG_ENTRIES];
    index_t   log_len[MAX_NODES];
    index_t   commit_index[MAX_NODES];
    node_id_t voted_for[MAX_NODES];
    node_id_t leader_id;
    bool      has_leader;
} tla_state_t;

/**
 * L3: A TLA+ action — a predicate relating two states (current vs next).
 * An action A(s, s') is true iff s' is a legal successor of s according
 * to the protocol specification.
 */
typedef struct {
    const char *name;          /* action name (e.g., "Phase1a", "BecomeLeader") */
    const char *description;   /* human-readable description */
    bool (*enabled)(const tla_state_t *s);  /* precondition */
    void (*execute)(const tla_state_t *s, tla_state_t *s_next); /* transition */
} tla_action_t;

/**
 * L3: A TLA+ specification — Initial predicate + Next action + Invariant.
 */
typedef struct {
    const char *name;
    bool (*init)(tla_state_t *s);            /* Init predicate */
    tla_action_t *actions;                    /* Next = ∃ a ∈ Actions: a */
    int num_actions;
    bool (*invariant)(const tla_state_t *s);  /* Invariant to check */
} tla_specification_t;

/* ─── Model Checking ─────────────────────────────────────────────────
 * L5 Algorithm: Explicit-state model checking via BFS/DFS state
 * space exploration.
 *
 * Given a finite-state specification (Init, Next, Invariant):
 *   1. Start from all initial states satisfying Init
 *   2. Compute all successors via Next (each enabled action)
 *   3. For each reached state, check Invariant
 *   4. If Invariant violated, report counterexample trace
 *   5. Continue until all reachable states are explored
 *
 * For small models (≤3 nodes, ≤2 ballot numbers), this is tractable.
 * For larger models, symmetry reduction or bounded model checking is needed.
 */

#define TLA_MAX_STATES 100000
#define TLA_MAX_TRACE_LEN 100

/**
 * L5: A counterexample trace — sequence of states leading to invariant
 * violation.
 */
typedef struct {
    tla_state_t states[TLA_MAX_TRACE_LEN];
    int length;
    int violated_at;  /* index where invariant was violated */
} tla_counterexample_t;

/**
 * L5: Model checking result.
 */
typedef struct {
    bool invariant_holds;      /* true if invariant holds in all reachable states */
    int  states_explored;      /* number of states visited */
    int  states_total;         /* total reachable states */
    tla_counterexample_t counterexample; /* if invariant_holds==false */
} tla_model_check_result_t;

/**
 * L5: Run model checking on a TLA+ specification.
 *
 * Uses BFS to enumerate all reachable states. For each state, checks
 * the invariant. If a violation is found, constructs a counterexample
 * trace back to an initial state.
 *
 * @param spec       the TLA+ specification
 * @param max_states maximum states to explore (prevents infinite loops)
 * @return model checking result
 */
tla_model_check_result_t tla_model_check(const tla_specification_t *spec,
                                           int max_states);

/**
 * L5: Print a counterexample trace (human-readable).
 * Outputs each state in the trace with the action that led to it.
 */
void tla_print_counterexample(const tla_counterexample_t *ce);

/* ─── Paxos TLA+ Specification ───────────────────────────────────────
 * L3: Encode the Paxos protocol as TLA+ actions.
 *
 * Variables: ballot, accepted, promised, decided
 *
 * Init:       ballot = 0, accepted = ∅, promised = 0, decided = nil
 * Phase1a(b): enabled if b > ballot, sets ballot = b
 * Phase1b(a): if ballot > promised[a], set promised[a] = ballot
 * Phase2a(v): if majority promised, set accepted += (ballot, v)
 * Phase2b:    if majority accepted same v, set decided = v
 */

/**
 * L3: Paxos TLA+ Init predicate.
 */
bool tla_paxos_init(tla_state_t *s);

/**
 * L3: Paxos TLA+ Next action (disjunction of all sub-actions).
 * Returns the number of enabled actions and fills the enabled[] array.
 */
int tla_paxos_actions_enabled(const tla_state_t *s, bool *enabled);

/**
 * L3: Paxos TLA+ invariant: Agreement.
 *
 *   □(∀ a,b : decided[a] ≠ nil ∧ decided[b] ≠ nil ⇒ decided[a] = decided[b])
 *
 * At most one value can be decided.
 */
bool tla_paxos_agreement_invariant(const tla_state_t *s);

/**
 * L5: Paxos specification packaged for model checking.
 */
tla_specification_t tla_paxos_specification(void);

/* ─── Raft TLA+ Specification ────────────────────────────────────────
 * L3: Raft TLA+ specification variables.
 *
 * Variables: term, state, log, commitIndex, votedFor, leaderId
 *
 * The Raft TLA+ spec was developed by Ongaro as part of his PhD thesis
 * and is the basis for the verified implementation.
 */

/**
 * L3: Raft TLA+ Init predicate.
 */
bool tla_raft_init(tla_state_t *s);

/**
 * L3: Raft TLA+ Next action.
 */
int tla_raft_actions_enabled(const tla_state_t *s, bool *enabled);

/**
 * L3: Raft TLA+ invariant: Election Safety.
 *
 *   □(∀ t : |{s : state[s] = Leader ∧ term[s] = t}| ≤ 1)
 *
 * At most one leader per term.
 */
bool tla_raft_election_safety_invariant(const tla_state_t *s);

/**
 * L5: Raft specification packaged for model checking.
 */
tla_specification_t tla_raft_specification(void);

/* ─── State Space Exploration ────────────────────────────────────────
 * L8 Advanced Topic: State space optimization techniques.
 */

/**
 * L8: Compute state hash for efficient duplicate detection.
 * In BFS model checking, we need O(1) lookup to check if a state
 * has already been visited.
 */
uint64_t tla_state_hash(const tla_state_t *s);

/**
 * L8: State equality check (used for hash collision resolution).
 */
bool tla_state_equal(const tla_state_t *a, const tla_state_t *b);

/**
 * L8: Symmetry reduction — canonize states under node permutation.
 * Reduces state space by identifying states that are equivalent up
 * to renaming of node IDs. For n nodes, this reduces the explored
 * state space by up to factor n!.
 *
 * @param s  state to canonize (modified in place)
 */
void tla_canonize_state(tla_state_t *s);

/**
 * L8: Bounded model checking — check invariant up to depth k.
 * Uses iterative deepening to find counterexamples of minimum length.
 */
typedef struct {
    int bound;           /* depth bound */
    bool invariant_holds;/* true if no violation found within bound */
    int states_explored;
} tla_bounded_check_result_t;

tla_bounded_check_result_t tla_bounded_model_check(
    const tla_specification_t *spec, int max_depth);

#endif /* TLA_ENCODING_H */
