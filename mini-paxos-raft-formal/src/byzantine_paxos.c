/**
 * byzantine_paxos.c — Byzantine Paxos & Byzantine Fault Tolerance
 *
 * Extends the Paxos consensus protocol to tolerate Byzantine (arbitrary)
 * faults, where faulty processes can behave maliciously — sending
 * contradictory messages, lying about their state, or colluding.
 *
 * Byzantine Paxos (Lamport, 2011) requires:
 *   - n = 3f + 1 nodes to tolerate f Byzantine faults
 *   - Quorum size q = 2f + 1
 *   - Any two quorums intersect in ≥ f+1 nodes (at least 1 correct)
 *
 * This is in contrast to crash-fault Paxos which requires only:
 *   - n = 2f + 1 nodes to tolerate f crash faults
 *   - Quorum size q = f + 1
 *
 * The additional n = 3f+1 requirement stems from the need to
 * outvote Byzantine nodes that may send contradictory information.
 *
 * Knowledge Coverage:
 *   L1 Definitions: Byzantine fault, 3f+1 bound, quorum certificate
 *   L2 Core Concepts: Byzantine vs crash fault tolerance
 *   L8 Advanced Topics: Byzantine Paxos, PBFT, cross-fault tolerance
 *
 * References:
 *   - Lamport, Shostak, Pease, "The Byzantine Generals Problem"
 *     (ACM TOPLAS, 1982)
 *   - Castro & Liskov, "Practical Byzantine Fault Tolerance" (OSDI 1999)
 *   - Lamport, "Byzantizing Paxos by Refinement" (DISC 2011)
 */

#include "../include/paxos_raft_types.h"
#include "../include/consensus_model.h"
#include <string.h>
#include <stdio.h>

/* ─── Byzantine Quorum System ─────────────────────────────────────── */

/**
 * Check if n satisfies the 3f+1 Byzantine threshold.
 *
 * Theorem: To tolerate f Byzantine faults, need n ≥ 3f + 1.
 *
 * Intuition: With 3f+1 nodes, even if f are faulty and send conflicting
 * messages, the 2f+1 correct nodes form a majority that can outvote
 * the faulty ones. Two quorums of size 2f+1 intersect in f+1 nodes,
 * guaranteeing at least one correct node in the intersection.
 */
bool byzantine_nodes_sufficient(int n, int f) {
    return n >= 3 * f + 1;
}

/**
 * Compute the maximum Byzantine faults tolerated for n nodes.
 *
 * f_max = ⌊(n - 1) / 3⌋
 *
 * Examples:
 *   n=4 → f_max = 1 (PBFT minimum deployment)
 *   n=7 → f_max = 2
 *   n=10 → f_max = 3
 */
int byzantine_max_faults(int n) {
    if (n < 4) return 0;
    return (n - 1) / 3;
}

/* ─── Byzantine Paxos Protocol ─────────────────────────────────────── */

/**
 * Byzantine Paxos state for a single proposer.
 * In Byzantine Paxos, the proposer must collect a quorum certificate
 * — proof that 2f+1 acceptors have promised/accepted.
 */
typedef struct {
    node_id_t id;
    ballot_t  ballot;
    consensus_value_t proposed_value;

    /* Phase 1 quorum certificate */
    int       prepare_cert_count;
    node_id_t prepare_cert[MAX_NODES];
    ballot_t  prepare_cert_ballot;

    /* Phase 2 quorum certificate */
    int       accept_cert_count;
    node_id_t accept_cert[MAX_NODES];
} byzantine_proposer_t;

/**
 * Byzantine Paxos Phase 1 (Prepare):
 *
 * The proposer selects ballot b and sends Prepare(b) to all acceptors.
 * To complete Phase 1, it must receive Promise messages from 2f+1
 * distinct acceptors (a quorum certificate).
 *
 * This is stricter than crash-fault Paxos (which only needs f+1).
 */
int byzantine_paxos_prepare_send(byzantine_proposer_t *bp,
                                  ballot_t ballot,
                                  int num_acceptors,
                                  paxos_message_t *out_msgs) {
    bp->ballot = ballot;
    bp->prepare_cert_count = 0;

    for (int i = 0; i < num_acceptors; i++) {
        out_msgs[i].type = PAXOS_MSG_PREPARE;
        out_msgs[i].from = bp->id;
        out_msgs[i].to   = (node_id_t)i;
        out_msgs[i].ballot = ballot;
    }
    return num_acceptors;
}

/**
 * Byzantine Paxos: Process a Promise message.
 *
 * The proposer adds the acceptor to its quorum certificate.
 * Phase 1 completes when |cert| ≥ 2f+1.
 */
bool byzantine_paxos_prepare_receive(byzantine_proposer_t *bp,
                                      const paxos_message_t *msg,
                                      int f) {
    if (msg->type != PAXOS_MSG_PROMISE) return false;
    if (msg->ballot != bp->ballot) return false;

    /* Check for duplicate acceptor */
    for (int i = 0; i < bp->prepare_cert_count; i++) {
        if (bp->prepare_cert[i] == msg->from) return false;
    }

    bp->prepare_cert[bp->prepare_cert_count++] = msg->from;
    int quorum_size = 2 * f + 1;
    return bp->prepare_cert_count >= quorum_size;
}

/**
 * Byzantine Paxos Phase 2 (Accept):
 *
 * After collecting a Phase 1 quorum certificate (2f+1 Promises),
 * the proposer sends Accept(b, v) where v is determined by the
 * highest-numbered accepted value among the promises (as in
 * crash-fault Paxos).
 *
 * Phase 2 completes when 2f+1 acceptors respond with Accepted.
 */
int byzantine_paxos_accept_send(byzantine_proposer_t *bp,
                                 int num_acceptors,
                                 const node_id_t *acceptor_ids,
                                 paxos_message_t *out_msgs) {
    bp->accept_cert_count = 0;

    for (int i = 0; i < num_acceptors; i++) {
        out_msgs[i].type   = PAXOS_MSG_ACCEPT;
        out_msgs[i].from   = bp->id;
        out_msgs[i].to     = acceptor_ids[i];
        out_msgs[i].ballot = bp->ballot;
        out_msgs[i].value  = bp->proposed_value;
    }
    return num_acceptors;
}

/**
 * Byzantine Paxos: Process an Accepted message.
 * Completes Phase 2 when 2f+1 acceptors have accepted.
 */
bool byzantine_paxos_accept_receive(byzantine_proposer_t *bp,
                                     const paxos_message_t *msg,
                                     int f) {
    if (msg->type != PAXOS_MSG_ACCEPTED) return false;
    if (msg->ballot != bp->ballot) return false;

    for (int i = 0; i < bp->accept_cert_count; i++) {
        if (bp->accept_cert[i] == msg->from) return false;
    }

    bp->accept_cert[bp->accept_cert_count++] = msg->from;
    int quorum_size = 2 * f + 1;
    return bp->accept_cert_count >= quorum_size;
}

/* ─── Byzantine Consensus Bounds ───────────────────────────────────── */

/**
 * Verify the necessary conditions for Byzantine consensus.
 *
 * Dolev-Strong (1983): In a synchronous system with Byzantine faults,
 * consensus is possible iff n > 3f.
 *
 * This function verifies the bound and outputs an explanation.
 */
void byzantine_verify_bounds(int n, int f) {
    printf("=== Byzantine Fault Tolerance Bounds ===\n");
    printf("Nodes (n): %d, Faults (f): %d\n", n, f);

    if (n >= 3 * f + 1) {
        printf("✓ n ≥ 3f+1 : Byzantine consensus is POSSIBLE\n");
        printf("  Quorum size: %d (out of %d)\n", 2*f + 1, n);
        printf("  Intersection: ≥ %d nodes (guaranteeing ≥1 correct)\n", f + 1);
    } else {
        printf("✗ n < 3f+1 : Byzantine consensus is IMPOSSIBLE\n");
        printf("  Minimum n needed: %d\n", 3*f + 1);
    }

    printf("\nComparison with crash-fault Paxos:\n");
    if (n >= 2 * f + 1) {
        printf("  Crash-fault Paxos: possible with n ≥ 2f+1 = %d\n", 2*f + 1);
        printf("  Quorum size: %d, Intersection: ≥ %d\n", f + 1, 1);
    }

    printf("\nByzantine overhead: +%d extra nodes (vs crash-fault)\n",
           n >= 3*f+1 ? f : (3*f+1 - n));
}

/**
 * Compute the minimum number of messages for Byzantine Paxos.
 *
 * Phase 1: n PREPARE + n PROMISE = 2n messages
 * Phase 2: n ACCEPT + n ACCEPTED = 2n messages
 * Total: 4n messages per consensus instance
 *
 * (In practice, with a stable leader and Multi-Paxos optimization,
 *  this reduces to 2n per instance after the leader is established.)
 */
int byzantine_paxos_message_cost(int n) {
    return 4 * n;
}

/* ─── Byzantine Node Types ──────────────────────────────────────────── */

/**
 * Byzantine node behavior model.
 *
 * A Byzantine node can exhibit any of these behaviors:
 *   - Equivocation: sending different values to different nodes
 *   - Omission: not sending messages
 *   - Lying: sending false information (e.g., claiming higher ballot)
 *   - Collusion: coordinating with other Byzantine nodes
 *
 * This enumeration captures the types of attacks Byzantine fault
 * tolerance must defend against.
 */
typedef enum {
    BYZ_OK,              /* Correct behavior */
    BYZ_EQUIVOCATION,    /* Sends conflicting values */
    BYZ_OMISSION,        /* Drops messages selectively */
    BYZ_LYING_BALLOT,    /* Lies about ballot numbers */
    BYZ_LYING_VALUE,     /* Lies about accepted values */
    BYZ_DELAY,           /* Delays messages arbitrarily */
} byzantine_behavior_t;

/**
 * Simulate a Byzantine acceptor's response to a Prepare message.
 *
 * A Byzantine acceptor might:
 *   - Respond with a higher ballot (causing the proposer to abort)
 *   - Claim a false accepted value
 *   - Not respond at all
 *
 * @param behavior  which Byzantine behavior to simulate
 * @param msg       the incoming PREPARE message
 * @param out_msg   the (possibly Byzantine) response
 */
void byzantine_acceptor_respond(byzantine_behavior_t behavior,
                                 const paxos_message_t *msg,
                                 paxos_message_t *out_msg) {
    memset(out_msg, 0, sizeof(*out_msg));
    out_msg->type = PAXOS_MSG_PROMISE;
    out_msg->from = msg->to;
    out_msg->to   = msg->from;
    out_msg->ballot = msg->ballot;

    switch (behavior) {
    case BYZ_OK:
        /* Correct response */
        break;

    case BYZ_EQUIVOCATION:
        /* Send different value to different proposer —
         * cannot be simulated in a single response. */
        break;

    case BYZ_LYING_BALLOT:
        /* Claim a higher ballot to block the proposer */
        out_msg->ballot = msg->ballot + 100;
        out_msg->accepted_ballot = msg->ballot + 100;
        break;

    case BYZ_LYING_VALUE:
        /* Claim a fake accepted value */
        out_msg->accepted_ballot = msg->ballot - 1;
        out_msg->accepted_value.data[0] = 'F'; /* Fake value */
        out_msg->accepted_value.length = 1;
        break;

    case BYZ_DELAY:
        /* Don't respond at all (simulated by caller not delivering) */
        out_msg->type = PAXOS_MSG_PREPARE; /* Invalid — caller should handle */
        break;

    case BYZ_OMISSION:
        /* Don't respond */
        out_msg->type = PAXOS_MSG_PREPARE; /* Invalid — drop indicator */
        break;
    }
}

/* ─── Cross-Fault Tolerance ───────────────────────────────────────── */

/**
 * L8 Advanced: Cross-fault tolerance analysis.
 *
 * Real systems may face both crash and Byzantine faults simultaneously.
 * The combined fault tolerance bound is more complex.
 *
 * If we assume up to b Byzantine faults and c crash faults, with
 * total faults f = b + c, the bound becomes approximately:
 *   n ≥ 3b + 2c + 1
 *
 * This is because Byzantine faults require 3-to-1 overhead, while
 * crash faults require 2-to-1 overhead.
 */
int cross_fault_min_nodes(int byzantine_faults, int crash_faults) {
    return 3 * byzantine_faults + 2 * crash_faults + 1;
}

/**
 * Compute the quorum size for mixed fault model.
 *
 * q = b + c + 1 + b = 2b + c + 1
 *
 * The first (b + c + 1) ensures a majority over total failures,
 * the extra b ensures intersection in the presence of equivocation.
 */
int cross_fault_quorum_size(int byzantine_faults, int crash_faults) {
    return 2 * byzantine_faults + crash_faults + 1;
}

/* ─── PBFT Comparison ─────────────────────────────────────────────── */

/**
 * Compare Byzantine Paxos with PBFT (Practical Byzantine Fault Tolerance).
 *
 * PBFT (Castro & Liskov, 1999) uses a three-phase protocol:
 *   1. Pre-Prepare (leader broadcasts request)
 *   2. Prepare (nodes broadcast their agreement)
 *   3. Commit (nodes broadcast commitment)
 *
 * Byzantine Paxos uses the standard two-phase Paxos with larger quorums.
 * Both require n ≥ 3f+1 in the partially synchronous model.
 *
 * Key differences:
 *   - PBFT uses all-to-all communication (O(n²) messages per request)
 *   - Byzantine Paxos uses leader-based communication (O(n) messages)
 *   - PBFT provides liveness under weak synchrony
 *   - Byzantine Paxos inherits Paxos' liveness properties
 */
void byzantine_compare_with_pbft(int n, int f) {
    printf("╔══════════════════════════════════════════╗\n");
    printf("║  Byzantine Paxos vs PBFT Comparison      ║\n");
    printf("╠══════════════════════════════════════════╣\n");
    printf("║  Nodes: %d, Byzantine faults: %d         ║\n", n, f);
    printf("╠══════════════════════════════════════════╣\n");

    int bpaxos_msgs = byzantine_paxos_message_cost(n);
    int pbft_msgs   = 2 * n * n; /* Approximate: all-to-all in 2 phases */

    printf("║  Byzantine Paxos messages: O(n) = %d     ║\n", bpaxos_msgs);
    printf("║  PBFT messages:           O(n²) = %d    ║\n", pbft_msgs);
    printf("╠══════════════════════════════════════════╣\n");
    printf("║  Both require n ≥ 3f+1 = %d             ║\n", 3*f+1);
    printf("║  Byzantine Paxos: simpler, leader-based  ║\n");
    printf("║  PBFT: lower latency, all-to-all         ║\n");
    printf("╚══════════════════════════════════════════╝\n");
}
