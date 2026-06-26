/**
 * paxos_raft_types.h — Core Type Definitions for Consensus Protocols
 *
 * Defines the fundamental data types shared across Paxos, Raft, and
 * formal verification modules. Each type corresponds to a mathematical
 * definition from the consensus literature.
 *
 * References:
 *   - Lamport, "The Part-Time Parliament" (1998)
 *   - Lamport, "Paxos Made Simple" (2001)
 *   - Ongaro & Ousterhout, "In Search of an Understandable Consensus Algorithm" (2014)
 *   - Lamport, "Specifying Systems" (TLA+, 2002)
 *
 * Knowledge Coverage:
 *   L1 Definitions: ballot/term, proposal, decision, quorum, log entry
 *   L3 Math Structures: total orders, set systems, sequences
 */

#ifndef PAXOS_RAFT_TYPES_H
#define PAXOS_RAFT_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/* ─── Node Identity ───────────────────────────────────────────────────
 * L1 Definition: A node (process) in a distributed system.
 * Each node has a unique identifier and can play roles:
 * Proposer/Acceptor/Learner (Paxos) or Follower/Candidate/Leader (Raft).
 */
#define MAX_NODES        32
#define MAX_LOG_ENTRIES  256
#define MAX_VALUE_SIZE   128
#define MAX_MESSAGE_SIZE 512
#define MAX_QUORUMS      1024  /* max quorums in system (C(7,4)+...=64, safe margin) */

typedef uint32_t node_id_t;
typedef uint64_t ballot_t;    /* ballot number (Paxos) — totally ordered */
typedef uint64_t term_t;      /* term number (Raft) — totally ordered */
typedef uint64_t index_t;     /* log index */

/**
 * L1 Definition: A value proposed in consensus.
 * In the consensus problem, each process proposes a value,
 * and all correct processes must eventually decide on the same value.
 */
typedef struct {
    char data[MAX_VALUE_SIZE];
    uint32_t length;
} consensus_value_t;

/**
 * L1 Definition: A proposal in Paxos.
 * A proposal is a pair (ballot_number, value).
 * ── Arora & Barak §19.2, Lamport "Paxos Made Simple" §2
 */
typedef struct {
    ballot_t ballot;           /* proposal number, unique per proposer */
    consensus_value_t value;   /* proposed value */
} paxos_proposal_t;

/**
 * L1 Definition: An acceptor state in Paxos.
 * Each acceptor maintains:
 *   - promise_ballot: highest ballot it has promised to accept
 *   - accepted_ballot: highest ballot it has accepted
 *   - accepted_value: value accepted at accepted_ballot
 */
typedef struct {
    node_id_t node_id;
    ballot_t  promise_ballot;        /* min_ballot in Lamport's notation */
    ballot_t  accepted_ballot;       /* last accepted proposal number */
    consensus_value_t accepted_value; /* last accepted value */
    bool      has_accepted;          /* whether any value has been accepted */
} paxos_acceptor_t;

/**
 * L1 Definition: A proposer state in Paxos.
 * The proposer drives the two-phase protocol:
 * Phase 1 (Prepare/Promise), Phase 2 (Accept/Accepted).
 */
typedef struct {
    node_id_t node_id;
    ballot_t  current_ballot;        /* current proposal number */
    consensus_value_t proposed_value; /* value being proposed */
    int       phase;                 /* 0=idle, 1=phase1, 2=phase2 */
} paxos_proposer_t;

/**
 * L1 Definition: A learner in Paxos.
 * A learner determines when a value is chosen (decided).
 */
typedef struct {
    node_id_t node_id;
    consensus_value_t learned_value;
    bool      has_learned;
} paxos_learner_t;

/* ─── Paxos Message Types ────────────────────────────────────────────
 * L2 Core Concept: The Paxos protocol exchanges four message types.
 * These encode the two-phase commit structure.
 */
typedef enum {
    PAXOS_MSG_PREPARE,       /* Phase 1a: Proposer → Acceptor */
    PAXOS_MSG_PROMISE,       /* Phase 1b: Acceptor → Proposer */
    PAXOS_MSG_ACCEPT,        /* Phase 2a: Proposer → Acceptor */
    PAXOS_MSG_ACCEPTED,      /* Phase 2b: Acceptor → Learner */
} paxos_msg_type_t;

typedef struct {
    paxos_msg_type_t type;
    node_id_t        from;
    node_id_t        to;
    ballot_t         ballot;
    ballot_t         accepted_ballot;  /* for PROMISE */
    consensus_value_t accepted_value;  /* for PROMISE */
    consensus_value_t value;           /* for ACCEPT, ACCEPTED */
} paxos_message_t;

/* ─── Raft State Types ───────────────────────────────────────────────
 * L1 Definition: Raft node states (Follower, Candidate, Leader).
 * ── Ongaro & Ousterhout (2014) §5.1
 */
typedef enum {
    RAFT_FOLLOWER,
    RAFT_CANDIDATE,
    RAFT_LEADER,
} raft_state_t;

/**
 * L1 Definition: A log entry in Raft.
 * Each entry contains a term number and a command (value).
 * ── Ongaro & Ousterhout (2014) §5.3
 */
typedef struct {
    term_t            term;       /* term when entry was received by leader */
    consensus_value_t command;    /* state machine command */
} raft_log_entry_t;

/**
 * L1 Definition: Raft persistent state (per server).
 * ── Ongaro & Ousterhout (2014) Figure 2
 */
typedef struct {
    node_id_t         node_id;
    raft_state_t      state;
    term_t            current_term;      /* latest term server has seen */
    node_id_t         voted_for;         /* candidateId that received vote */
    raft_log_entry_t  log[MAX_LOG_ENTRIES];
    index_t           log_length;        /* number of entries in log */
    index_t           commit_index;      /* highest log entry committed */
    index_t           last_applied;      /* highest log entry applied to state machine */
} raft_persistent_state_t;

/**
 * L1 Definition: Raft volatile state (per server).
 * ── Ongaro & Ousterhout (2014) Figure 2
 */
typedef struct {
    index_t           next_index[MAX_NODES];   /* for leader: next entry to send */
    index_t           match_index[MAX_NODES];  /* for leader: highest entry replicated */
} raft_volatile_state_t;

/**
 * L1 Definition: Complete Raft server state.
 */
typedef struct {
    raft_persistent_state_t persistent;
    raft_volatile_state_t   volatile_;
    /* election timer (not persisted) */
    uint64_t          election_timeout_ms;
    uint64_t          election_elapsed_ms;
    uint64_t          heartbeat_timeout_ms;
} raft_server_t;

/* ─── Raft RPC Types ─────────────────────────────────────────────────
 * L2 Core Concept: Raft uses two RPCs: RequestVote and AppendEntries.
 */
typedef enum {
    RAFT_RPC_REQUEST_VOTE,
    RAFT_RPC_REQUEST_VOTE_REPLY,
    RAFT_RPC_APPEND_ENTRIES,
    RAFT_RPC_APPEND_ENTRIES_REPLY,
} raft_rpc_type_t;

/**
 * L1 Definition: RequestVote RPC arguments.
 * Invoked by candidates to gather votes.
 * ── Ongaro & Ousterhout (2014) Figure 2
 */
typedef struct {
    term_t    term;          /* candidate's term */
    node_id_t candidate_id;  /* candidate requesting vote */
    index_t   last_log_index;/* index of candidate's last log entry */
    term_t    last_log_term; /* term of candidate's last log entry */
} raft_request_vote_args_t;

typedef struct {
    term_t    term;          /* currentTerm, for candidate to update itself */
    bool      vote_granted;  /* true if candidate received vote */
} raft_request_vote_reply_t;

/**
 * L1 Definition: AppendEntries RPC arguments.
 * Invoked by leader to replicate log entries and as heartbeat.
 * ── Ongaro & Ousterhout (2014) Figure 2
 */
typedef struct {
    term_t    term;          /* leader's term */
    node_id_t leader_id;     /* so follower can redirect clients */
    index_t   prev_log_index;/* index of log entry preceding new ones */
    term_t    prev_log_term; /* term of prevLogIndex entry */
    raft_log_entry_t entries[MAX_LOG_ENTRIES];
    index_t   num_entries;   /* number of entries in this RPC */
    index_t   leader_commit; /* leader's commitIndex */
} raft_append_entries_args_t;

typedef struct {
    term_t    term;          /* currentTerm, for leader to update itself */
    bool      success;       /* true if follower contained matching entry */
} raft_append_entries_reply_t;

/**
 * L1 Definition: Abstract Raft RPC message.
 */
typedef struct {
    raft_rpc_type_t type;
    union {
        raft_request_vote_args_t      request_vote;
        raft_request_vote_reply_t     request_vote_reply;
        raft_append_entries_args_t    append_entries;
        raft_append_entries_reply_t   append_entries_reply;
    } body;
} raft_message_t;

/* ─── Quorum Types ───────────────────────────────────────────────────
 * L3 Math Structure: A quorum system is a set of subsets of processes
 * such that any two quorums intersect.
 *
 * For Paxos/Raft with n nodes tolerating f failures:
 *   quorum size = floor(n/2) + 1
 *   fault tolerance f < n/2  (i.e., n = 2f + 1 at minimum)
 *
 * ── Lamport, "Paxos Made Simple" §2; Malkhi & Reiter (1998)
 */
typedef struct {
    int  nodes[MAX_NODES];  /* sorted array of node IDs in this quorum */
    int  size;              /* number of nodes in this quorum */
} quorum_t;

/**
 * L3 Math Structure: A quorum system over a universe of n nodes.
 */
typedef struct {
    node_id_t universe[MAX_NODES];
    int       universe_size;
    quorum_t  quorums[MAX_QUORUMS]; /* all possible quorums */
    int       num_quorums;
    int       quorum_size;        /* minimum quorum size (majority) */
} quorum_system_t;

/* ─── Invariant Types ────────────────────────────────────────────────
 * L4 Fundamental Law: Safety invariants that must hold in all
 * reachable states of a consensus protocol.
 */
typedef enum {
    INV_AGREEMENT,          /* at most one value is decided */
    INV_VALIDITY,           /* decided value was proposed */
    INV_INTEGRITY,          /* no process decides twice */
    INV_LEADER_AT_MOST_ONE, /* at most one leader per term */
    INV_LOG_MATCHING,       /* if two logs have an entry at index i,
                               they are identical up to i */
    INV_LEADER_COMPLETENESS,/* leader has all committed entries */
    INV_STATE_MACHINE_SAFETY,/* at most one value per log index */
} invariant_type_t;

/**
 * L3 Math Structure: A global system state (configuration).
 * Used for model checking and invariant verification.
 */
typedef struct {
    /* Paxos state */
    paxos_proposer_t proposers[MAX_NODES];
    int              num_proposers;
    paxos_acceptor_t acceptors[MAX_NODES];
    int              num_acceptors;
    paxos_learner_t  learners[MAX_NODES];
    int              num_learners;
    /* Raft state */
    raft_server_t    raft_servers[MAX_NODES];
    int              num_raft_servers;
    /* Shared */
    int              num_nodes;       /* total number of nodes */
    int              max_faults;      /* maximum tolerated faults */
    /* message queues (simplified: one global queue) */
    paxos_message_t  paxos_queue[1024];
    int              paxos_queue_len;
    raft_message_t   raft_queue[1024];
    int              raft_queue_len;
} system_state_t;

#endif /* PAXOS_RAFT_TYPES_H */
