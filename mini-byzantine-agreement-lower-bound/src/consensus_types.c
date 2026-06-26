/**
 * @file consensus_types.c
 * @brief Implementation of consensus type taxonomy, thresholds, reductions,
 *        Paxos, PBFT, failure detectors, and CAP theorem analysis.
 *
 * Knowledge coverage:
 *   L2: Consensus families and their fault tolerance thresholds
 *   L2: Protocol reductions between consensus types
 *   L5: Paxos (crash-stop consensus) initialization and phases
 *   L5: PBFT (Practical Byzantine Fault Tolerance) phases
 *   L5: Failure detector abstractions
 *   L7: CAP theorem connection to Byzantine agreement
 *
 * Reference: Lamport (1998) "The Part-Time Parliament" (Paxos)
 *            Castro & Liskov (1999) "Practical Byzantine Fault Tolerance"
 *            Chandra & Toueg (1996) "Unreliable Failure Detectors"
 *            Brewer (2000) "Towards Robust Distributed Systems" (CAP)
 */

#include "consensus_types.h"
#include "impossibility_proof.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ================================================================
 * L2: Consensus Threshold Computation
 *
 * Each consensus family has a characteristic fault tolerance bound.
 * These bounds are fundamental results from the distributed computing
 * literature.
 * ================================================================ */

int consensus_get_threshold(consensus_family_t family, int32_t N,
                             consensus_threshold_t *threshold)
{
    if (!threshold || N < 1) return -1;

    memset(threshold, 0, sizeof(*threshold));
    threshold->family = family;
    threshold->N = N;

    switch (family) {
    case CONSENSUS_CRASH_STOP:
        /* Paxos: requires f < N/2, i.e., majority quorum available */
        threshold->max_faulty = (N - 1) / 2;
        threshold->fault_ratio = 0.5;
        threshold->bound_theorem = "Lamport (1998): Paxos requires f < N/2 acceptors";
        threshold->synchronous = false;
        threshold->min_rounds = 2;
        break;

    case CONSENSUS_CRASH_RECOVERY:
        /* Raft: same as Paxos for safety, liveness needs majority */
        threshold->max_faulty = (N - 1) / 2;
        threshold->fault_ratio = 0.5;
        threshold->bound_theorem = "Ongaro & Ousterhout (2014): Raft, f < N/2";
        threshold->synchronous = false;
        threshold->min_rounds = 2;
        break;

    case CONSENSUS_BYZANTINE_ORAL:
        /* LSP82: f < N/3 necessary and sufficient */
        threshold->max_faulty = (N - 1) / 3;
        threshold->fault_ratio = 1.0 / 3.0;
        threshold->bound_theorem = "Lamport-Shostak-Pease (1982): f < N/3 for oral messages";
        threshold->synchronous = true;
        threshold->min_rounds = threshold->max_faulty + 1;
        break;

    case CONSENSUS_BYZANTINE_SIGNED:
        /* Dolev-Strong: f < N/2 with strong validity, f < N for agreement */
        threshold->max_faulty = (N - 1) / 2;
        threshold->fault_ratio = 0.5;
        threshold->bound_theorem = "Dolev-Strong (1983): f < N/2 with authenticated channels";
        threshold->synchronous = true;
        threshold->min_rounds = threshold->max_faulty + 1;
        break;

    case CONSENSUS_RANDOMIZED:
        /* Ben-Or: f < N/5 Byzantine, f < N/2 crash */
        threshold->max_faulty = (N - 1) / 5;
        threshold->fault_ratio = 0.2;
        threshold->bound_theorem = "Ben-Or (1983): Randomized consensus tolerates f < N/5 Byzantine";
        threshold->synchronous = false;
        threshold->min_rounds = 1;  /* Expected O(1) rounds */
        break;

    case CONSENSUS_QUANTUM:
        /* Quantum Byzantine agreement can tolerate f < N/4 with
         * perfect security, or f < N/3 with some relaxations. */
        threshold->max_faulty = (N - 1) / 4;
        threshold->fault_ratio = 0.25;
        threshold->bound_theorem = "Fitzi et al. (2001): Quantum BA tolerates f < N/4";
        threshold->synchronous = true;
        threshold->min_rounds = threshold->max_faulty + 1;
        break;

    default:
        return -1;
    }

    return 0;
}

/* ================================================================
 * L2: Consensus Reductions
 *
 * Reductions show how one consensus primitive can implement another.
 * These are fundamental to understanding the relative power of
 * different system models.
 * ================================================================ */

typedef struct {
    consensus_family_t from;
    consensus_family_t to;
    int32_t overhead_rounds;
    int32_t overhead_msgs;
    const char *name;
    bool is_tight;
} reduction_entry_t;

static const reduction_entry_t reduction_table[] = {
    /* Signed BFT can simulate oral BFT by ignoring signatures */
    {CONSENSUS_BYZANTINE_SIGNED, CONSENSUS_BYZANTINE_ORAL, 0, 0,
     "Signed-to-Oral (trivial inclusion)", true},

    /* BFT can simulate crash faults (Byzantine is stronger) */
    {CONSENSUS_BYZANTINE_ORAL, CONSENSUS_CRASH_STOP, 0, 0,
     "BFT-to-CFT (fault model inclusion)", true},

    /* Randomized consensus can solve async BFT where deterministic cannot */
    {CONSENSUS_RANDOMIZED, CONSENSUS_BYZANTINE_ORAL, 0, 0,
     "Randomized-async-to-BFT-sync (with common coin)", false},

    /* Crash-stop to crash-recovery (trivial) */
    {CONSENSUS_CRASH_STOP, CONSENSUS_CRASH_RECOVERY, 0, 0,
     "CrashStop-to-CrashRecovery (recovery adds complexity)", false},

    /* Terminator entry */
    {0, 0, 0, 0, NULL, false}
};

bool consensus_has_reduction(consensus_family_t from, consensus_family_t to,
                              consensus_reduction_t *reduction)
{
    if (!reduction) return false;

    for (const reduction_entry_t *r = reduction_table; r->name != NULL; r++) {
        if (r->from == from && r->to == to) {
            memset(reduction, 0, sizeof(*reduction));
            reduction->from_family = r->from;
            reduction->to_family = r->to;
            reduction->overhead_rounds = r->overhead_rounds;
            reduction->overhead_msgs = r->overhead_msgs;
            reduction->reduction_name = r->name;
            reduction->is_tight = r->is_tight;
            return true;
        }
    }

    return false;
}

/* ================================================================
 * L5: Paxos Protocol
 *
 * Paxos solves consensus in the crash-stop model. It works in phases:
 *
 * Phase 1 (Prepare):
 *   Proposer chooses proposal number n and sends Prepare(n) to all acceptors.
 *   Acceptor: if n > highest prepare seen, promise not to accept any
 *   proposal < n, and reply with the highest-numbered proposal accepted.
 *
 * Phase 2 (Accept):
 *   If proposer receives promises from a quorum:
 *     Choose value v = value of highest-numbered proposal from responses,
 *                     or proposer's own value if no responses have a value.
 *     Send Accept(n, v) to all acceptors.
 *   Acceptor: if n >= highest prepare seen, accept (n, v).
 *
 * Decision: A value is decided when a quorum accepts it.
 * ================================================================ */

int paxos_init(paxos_state_t *paxos, int32_t N, int32_t f)
{
    if (!paxos || N < 1 || N > BA_MAX_PROCESSES) return -1;
    if (2 * f >= N) return -1;  /* Need quorum > N/2 */

    memset(paxos, 0, sizeof(*paxos));
    paxos->N = N;
    paxos->f = f;
    paxos->quorum_size = (N / 2) + 1;
    paxos->leader_id = 0;
    paxos->proposal_number = 0;
    paxos->proposed_value = -1;
    paxos->has_decided = false;
    paxos->decided_value = -1;

    for (int32_t i = 0; i < N; i++) {
        paxos->accepted_value[i] = -1;
        paxos->accepted_proposal[i] = -1;
    }

    return 0;
}

int paxos_phase1_prepare(paxos_state_t *paxos, int64_t proposal)
{
    if (!paxos) return -1;
    if (proposal <= paxos->proposal_number) return -1;

    paxos->proposal_number = proposal;

    /* In a real implementation, we'd send Prepare(proposal) to all
     * acceptors and collect promises. Here we simulate a quorum
     * always granting the promise for deterministic testing. */

    /* Check if any acceptor has a previously accepted value */
    int64_t highest_accepted_proposal = -1;
    int32_t highest_accepted_value = -1;

    for (int32_t i = 0; i < paxos->N; i++) {
        if (paxos->accepted_proposal[i] > highest_accepted_proposal) {
            highest_accepted_proposal = paxos->accepted_proposal[i];
            highest_accepted_value = paxos->accepted_value[i];
        }
    }

    if (highest_accepted_value >= 0) {
        paxos->proposed_value = highest_accepted_value;
    }

    return 0;
}

int paxos_phase2_accept(paxos_state_t *paxos, int32_t value)
{
    if (!paxos) return -1;

    /* Leader proposes value v */
    paxos->proposed_value = value;

    /* Simulate quorum acceptance: count how many accept */
    int32_t accept_count = 0;
    for (int32_t i = 0; i < paxos->N; i++) {
        /* Acceptors that haven't crashed accept */
        if (i < paxos->N - paxos->f) {
            paxos->accepted_value[i] = value;
            paxos->accepted_proposal[i] = paxos->proposal_number;
            accept_count++;
        }
    }

    if (accept_count >= paxos->quorum_size) {
        paxos->has_decided = true;
        paxos->decided_value = value;
        return 0;
    }

    return 1;  /* Not enough acceptors */
}

/* ================================================================
 * L5: PBFT Protocol
 *
 * PBFT (Castro & Liskov 1999) is the practical Byzantine fault
 * tolerance protocol. It works in three phases:
 *
 * Pre-prepare: Primary assigns sequence number n to request and
 *              multicasts <<PRE-PREPARE, v, n, d>_σp, m> to all.
 * Prepare:     Upon receiving valid pre-prepare, replica multicasts
 *              <PREPARE, v, n, d, i>_σi to all.
 * Commit:      Upon receiving 2f+1 matching prepare messages,
 *              replica multicasts <COMMIT, v, n, d, i>_σi.
 * Decision:    Upon receiving 2f+1 matching commit messages,
 *              replica executes the request.
 *
 * Total: f < N/3, quorum size = 2f+1.
 * ================================================================ */

int pbft_init(pbft_state_t *pbft, int32_t N, int32_t f)
{
    if (!pbft || N < 1 || N > BA_MAX_PROCESSES) return -1;
    if (3 * f >= N) return -1;

    memset(pbft, 0, sizeof(*pbft));
    pbft->N = N;
    pbft->f = f;
    pbft->quorum_size = 2 * f + 1;
    pbft->view = 0;
    pbft->primary_id = 0;
    pbft->sequence_number = 0;
    pbft->prepare_count = 0;
    pbft->commit_count = 0;
    pbft->prepared = false;
    pbft->committed = false;

    for (int32_t i = 0; i < N; i++) {
        pbft->pre_prepare_values[i] = -1;
    }

    return 0;
}

int pbft_pre_prepare(pbft_state_t *pbft, int32_t value)
{
    if (!pbft) return -1;
    pbft->pre_prepare_values[pbft->primary_id] = value;

    /* In a real PBFT, replicas verify the pre-prepare message.
     * Here we simulate correct replicas accepting it. */
    int32_t valid_count = 0;
    for (int32_t i = 0; i < pbft->N - pbft->f; i++) {
        /* Correct replicas accept the pre-prepare if it's from the primary */
        pbft->pre_prepare_values[i] = value;
        valid_count++;
    }

    return (valid_count >= pbft->quorum_size) ? 0 : 1;
}

int pbft_prepare(pbft_state_t *pbft)
{
    if (!pbft) return -1;

    /* Count replicas that have matching pre-prepare values */
    int32_t match_count = 0;
    int32_t ref_value = pbft->pre_prepare_values[pbft->primary_id];

    for (int32_t i = 0; i < pbft->N - pbft->f; i++) {
        if (pbft->pre_prepare_values[i] == ref_value) {
            match_count++;
        }
    }

    pbft->prepare_count = match_count;
    if (match_count >= pbft->quorum_size) {
        pbft->prepared = true;
        return 0;
    }
    return 1;
}

int pbft_commit(pbft_state_t *pbft)
{
    if (!pbft) return -1;
    if (!pbft->prepared) return 1;

    /* In real PBFT, replicas send commit messages after receiving
     * 2f+1 matching prepare messages. We simulate the commit phase. */
    pbft->commit_count = pbft->prepare_count;  /* Simplify: same set commits */

    if (pbft->commit_count >= pbft->quorum_size) {
        pbft->committed = true;
        return 0;
    }
    return 1;
}

/* ================================================================
 * L5: Failure Detectors
 *
 * Chandra & Toueg (1996) introduced the concept of unreliable failure
 * detectors. They characterized the minimal failure detector needed
 * for consensus in asynchronous systems: ◇S (eventually strong).
 *
 * Properties:
 *   Strong completeness: Eventually every crashed process is permanently
 *                        suspected by every correct process.
 *   Eventual weak accuracy: There is a time after which some correct
 *                        process is never suspected by any correct process.
 * ================================================================ */

int failure_detector_init(failure_detector_t *fd,
                           failure_detector_class_t class, int32_t N)
{
    if (!fd || N < 1 || N > BA_MAX_PROCESSES) return -1;

    memset(fd, 0, sizeof(*fd));
    fd->class = class;
    fd->N = N;
    fd->false_positives = 0;
    fd->false_negatives = 0;

    return 0;
}

void failure_detector_heartbeat(failure_detector_t *fd, int32_t pid, bool alive)
{
    if (!fd || pid < 0 || pid >= fd->N) return;

    if (alive) {
        /* Process responded: clear suspicion */
        if (fd->suspected[pid]) {
            /* Was incorrectly suspected → false positive */
            fd->false_positives++;
        }
        fd->suspected[pid] = false;
    } else {
        /* Process did not respond: suspect it */
        if (!fd->suspected[pid]) {
            fd->suspected[pid] = true;
            /* If pid is actually correct → false positive (temporary) */
            /* In a real system we don't know, so just track the suspicion */
        }
    }
}

bool failure_detector_is_suspected(const failure_detector_t *fd, int32_t pid)
{
    if (!fd || pid < 0 || pid >= fd->N) return false;
    return fd->suspected[pid];
}

/* ================================================================
 * L7: CAP Theorem Analysis
 *
 * The CAP theorem (Brewer 2000, Gilbert & Lynch 2002) states that a
 * distributed data store can provide at most two of:
 *   Consistency (C): All nodes see the same data at the same time.
 *   Availability (A): Every request receives a (non-error) response.
 *   Partition Tolerance (P): System continues despite network partitions.
 *
 * In practice, P is non-negotiable (partitions happen), so the choice
 * is between CP (consistent under partition) and AP (available
 * under partition).
 *
 * Connection to Byzantine agreement:
 *   - Consensus is a CP system: during a partition, it may not be
 *     available (cannot form a quorum).
 *   - If we relax consistency, we can achieve availability (AP).
 *   - Byzantine faults are "adversarial partitions."
 * ================================================================ */

int cap_evaluate(int32_t N, int32_t f, ba_message_model_t msg_model,
                  ba_sync_model_t sync_model, cap_system_t *cap_result)
{
    if (!cap_result || N < 1) return -1;

    memset(cap_result, 0, sizeof(*cap_result));

    /* Partition tolerance is always achievable by definition:
     * we assume network partitions can occur. */
    cap_result->provides_partition_tolerance = true;

    /* Can we achieve consistency (all nodes agree) under faults? */
    bool consensus_possible = false;
    const char *theorem_ref = NULL;
    impossibility_check(N, f, msg_model, sync_model,
                        &consensus_possible, &theorem_ref);

    cap_result->provides_consistency = consensus_possible;

    /* Availability: can we respond to every request?
     * Under Byzantine faults with consensus, we may need to wait
     * for a quorum, sacrificing availability during partitions. */
    if (sync_model == BA_SYNC) {
        /* Synchronous: we can always respond within bounded time */
        cap_result->provides_availability = true;
        cap_result->achievable_pair = consensus_possible ? CAP_CA : CAP_AP;
    } else if (sync_model == BA_ASYNC) {
        /* Asynchronous + consensus = impossible (FLP)
         * Must choose: consistency (CP, but unavailable) or
         *              availability (AP, eventually consistent) */
        if (msg_model == BA_MSG_SIGNED || f == 0) {
            /* With signatures or no faults, CP is possible */
            cap_result->provides_availability = false;
            cap_result->achievable_pair = CAP_CP;
        } else {
            cap_result->provides_availability = true;
            cap_result->provides_consistency = false;
            cap_result->achievable_pair = CAP_AP;
        }
    } else {
        /* Partial synchrony: during synchronous periods, CA possible */
        cap_result->provides_availability = consensus_possible;
        cap_result->achievable_pair = consensus_possible ? CAP_CA : CAP_CP;
    }

    /* Note: A CA system (no partition tolerance) is only possible
     * in a perfectly reliable network, which is unrealistic.
     * The CAP theorem's practical implication: choose CP or AP. */

    return 0;
}
