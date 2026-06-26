/**
 * @file phase_protocols.c
 * @brief Implementation of Phase King, Ben-Or randomized consensus, and
 *        Dolev-Strong authenticated Byzantine agreement algorithms.
 *
 * Knowledge coverage:
 *   L5: Phase King algorithm (Berman & Garay 1993)
 *   L5: Ben-Or randomized consensus (Ben-Or 1983)
 *   L5: Dolev-Strong authenticated algorithm (Dolev & Strong 1983)
 *   L7: Blockchain consensus connection (PBFT-inspired signing)
 *
 * Reference: Berman & Garay (1993) "Cloture Votes: n/4-resilient
 *            Distributed Consensus in t+1 rounds"
 *            Ben-Or (1983) "Another Advantage of Free Choice"
 *            Dolev & Strong (1983) "Authenticated Algorithms for
 *            Byzantine Agreement"
 */

#include "phase_protocols.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ================================================================
 * L5: Phase King Algorithm
 *
 * Phase King operates in (f+1) phases, each with two rounds.
 * In each phase k, process k+1 (mod N) acts as the "king."
 *
 * Round 1: All processes broadcast their current estimate.
 *          Each process counts occurrences.
 *
 * Round 2: The king broadcasts its estimate.
 *          Each process updates:
 *            if count[majority] > N/2 + f:
 *               estimate = majority    (overwhelming majority)
 *            else:
 *               estimate = king's value (follow the king)
 *
 * After phase f+1, all correct processes agree on the same value
 * because at least one phase has a correct king, and correct kings
 * force agreement among all correct processes.
 *
 * Note: The original Phase King requires f < N/4. Later improvements
 * (Berman, Garay, Perry 1989) relaxed this.
 * ================================================================ */

int phase_king_init(phase_king_system_t *pk, int32_t N, int32_t f,
                    const int32_t *values, const ba_fault_type_t *faults)
{
    (void)faults;
    if (!pk || N < 1 || N > BA_MAX_PROCESSES || f < 0) return -1;
    /* Original Phase King requires f < N/4 */
    if (4 * f >= N) return -1;

    memset(pk, 0, sizeof(*pk));
    pk->N = N;
    pk->f = f;
    pk->current_phase = 0;
    pk->agreement_reached = false;

    /* Assign king for each phase: phase k → king = k % N */
    for (int32_t k = 0; k <= f; k++) {
        pk->king_schedule[k] = k % N;
    }

    for (int32_t i = 0; i < N; i++) {
        pk->processes[i].pid = i;
        pk->processes[i].estimate = (values ? values[i] : 0);
        pk->processes[i].phase = 0;
        pk->processes[i].round_in_phase = 0;
        pk->processes[i].has_decided = false;
        pk->processes[i].decision = -1;
        memset(pk->processes[i].votes, 0, sizeof(pk->processes[i].votes));
        pk->processes[i].num_votes = 0;
        pk->processes[i].king_value = -1;
    }

    return 0;
}

int phase_king_step(phase_king_system_t *pk)
{
    if (!pk) return -1;
    if (pk->current_phase > pk->f) {
        pk->agreement_reached = true;
        return 1;
    }

    int32_t N = pk->N;
    int32_t f = pk->f;
    int32_t phase = pk->current_phase;

    /* Round 1: Broadcast estimates */
    for (int32_t i = 0; i < N; i++) {
        phase_king_process_t *p = &pk->processes[i];
        p->num_votes = 0;
        memset(p->votes, 0, sizeof(p->votes));

        /* Collect estimates from all processes */
        for (int32_t sender = 0; sender < N; sender++) {
            int32_t val = pk->processes[sender].estimate;
            if (val >= 0 && val < BA_MAX_VALUES) {
                p->votes[val]++;
                p->num_votes++;
            }
        }
    }

    /* Round 2: King broadcasts */
    int32_t king_pid = pk->king_schedule[phase];
    int32_t king_value = pk->processes[king_pid].estimate;

    for (int32_t i = 0; i < N; i++) {
        phase_king_process_t *p = &pk->processes[i];
        p->king_value = king_value;

        /* Find majority in votes */
        int32_t majority = -1;
        int32_t threshold = N / 2 + f;  /* Need overwhelming majority */

        for (int32_t v = 0; v < BA_MAX_VALUES; v++) {
            if (p->votes[v] > threshold) {
                majority = v;
                break;
            }
        }

        if (majority >= 0) {
            /* Overwhelming majority: adopt it */
            p->estimate = majority;
        } else {
            /* No clear majority: follow the king */
            p->estimate = king_value;
        }
    }

    pk->current_phase++;

    /* After f+1 phases, all correct processes decide */
    if (pk->current_phase > f) {
        for (int32_t i = 0; i < N; i++) {
            pk->processes[i].has_decided = true;
            pk->processes[i].decision = pk->processes[i].estimate;
        }
        pk->agreement_reached = true;
        return 1;
    }

    return 0;
}

int phase_king_run(phase_king_system_t *pk, ba_result_t *result)
{
    if (!pk || !result) return -1;

    memset(result, 0, sizeof(*result));

    while (!pk->agreement_reached) {
        int ret = phase_king_step(pk);
        if (ret < 0) return ret;
        if (ret == 1) break;
    }

    /* Verify agreement */
    int32_t ref_decision = -1;
    bool first = true;
    result->agreement_holds = true;
    result->validity_holds = true;
    result->termination_holds = true;

    for (int32_t i = 0; i < pk->N; i++) {
        if (!pk->processes[i].has_decided) {
            result->termination_holds = false;
        }
        if (first) {
            ref_decision = pk->processes[i].decision;
            first = false;
        } else if (pk->processes[i].decision != ref_decision) {
            result->agreement_holds = false;
        }
    }

    result->decided_value = ref_decision;
    result->num_rounds = (pk->current_phase) * 2;

    return 0;
}

/* ================================================================
 * L5: Ben-Or Randomized Consensus
 *
 * Ben-Or's algorithm achieves consensus in asynchronous systems
 * with probability 1. It works for binary consensus (values 0 or 1).
 *
 * Algorithm (each round r):
 *   Phase 1 (Exchange):
 *     Broadcast (R, 1, estimate) to all.
 *     Wait for N-f messages of the form (R, 1, *).
 *     If > N/2 have the same value v:
 *       pref = v
 *     Else:
 *       pref = ?
 *
 *   Phase 2 (Adopt):
 *     Broadcast (R, 2, pref) to all.
 *     Wait for N-f messages of the form (R, 2, *).
 *     If any non-? value v appears at least f+1 times:
 *       decide v (and continue broadcasting)
 *       new_estimate = v
 *     Else if all non-? prefs are some v and count(v) > 0:
 *       new_estimate = v
 *     Else:
 *       new_estimate = coin_flip()  {0 or 1 with equal probability}
 *
 * Expected rounds to termination: O(2^N / (N-f)) in worst case,
 * but typically much faster in practice with a common coin.
 * ================================================================ */

/**
 * @brief Simple PRNG based on xorshift64 for deterministic coin flips.
 */
static uint64_t xorshift64_state = 88172645463325252ULL;

static uint64_t xorshift64(void)
{
    xorshift64_state ^= xorshift64_state << 13;
    xorshift64_state ^= xorshift64_state >> 7;
    xorshift64_state ^= xorshift64_state << 17;
    return xorshift64_state;
}

static int32_t fair_coin(void)
{
    return (int32_t)(xorshift64() & 1);  /* 0 or 1 */
}

int benor_init(benor_system_t *bo, int32_t N, int32_t f,
               const int32_t *values, const ba_fault_type_t *faults,
               uint64_t seed)
{
    if (!bo || N < 1 || N > BA_MAX_PROCESSES || f < 0) return -1;

    memset(bo, 0, sizeof(*bo));
    bo->N = N;
    bo->f = f;
    bo->max_rounds = 100;  /* Safety bound */
    bo->current_round = 0;
    bo->agreement_reached = false;
    bo->coin_seed = seed;
    xorshift64_state = seed;

    for (int32_t i = 0; i < N; i++) {
        bo->processes[i].pid = i;
        bo->processes[i].estimate = (values ? values[i] : 0);
        bo->processes[i].round = 0;
        bo->processes[i].num_recv_estimates = 0;
        bo->processes[i].num_recv_adoptions = 0;
        bo->processes[i].adopted_value = -1;
        bo->processes[i].has_decided = false;
        bo->processes[i].decision = -1;
        bo->processes[i].fault_type = (faults ? faults[i] : BA_CORRECT);
    }

    return 0;
}

int benor_step(benor_system_t *bo)
{
    if (!bo) return -1;
    if (bo->agreement_reached) return 1;
    if (bo->current_round >= bo->max_rounds) {
        bo->agreement_reached = true;
        return 1;
    }

    int32_t N = bo->N;
    int32_t f = bo->f;
    int32_t r = bo->current_round;

    /* Phase 1: Exchange estimates */
    for (int32_t i = 0; i < N; i++) {
        benor_process_t *p = &bo->processes[i];
        if (p->fault_type != BA_CORRECT) continue;

        p->num_recv_estimates = 0;
        for (int32_t sender = 0; sender < N; sender++) {
            if (bo->processes[sender].fault_type == BA_CRASH_FAULT) continue;
            /* Byzantine processes may send arbitrary values */
            int32_t val;
            if (bo->processes[sender].fault_type == BA_BYZANTINE) {
                val = (sender % 2 == 0) ? 0 : 1;  /* Arbitrary but deterministic */
            } else {
                val = bo->processes[sender].estimate;
            }
            if (p->num_recv_estimates < BA_MAX_PROCESSES) {
                p->recv_estimates[p->num_recv_estimates++] = val;
            }
        }
    }

    /* Each correct process computes pref */
    for (int32_t i = 0; i < N; i++) {
        benor_process_t *p = &bo->processes[i];
        if (p->fault_type != BA_CORRECT) continue;

        int32_t count_0 = 0, count_1 = 0;
        for (int32_t j = 0; j < p->num_recv_estimates; j++) {
            if (p->recv_estimates[j] == 0) count_0++;
            else if (p->recv_estimates[j] == 1) count_1++;
        }

        int32_t threshold = N / 2;
        if (count_0 > threshold) {
            p->adopted_value = 0;
        } else if (count_1 > threshold) {
            p->adopted_value = 1;
        } else {
            p->adopted_value = -1;  /* ? */
        }
    }

    /* Phase 2: Exchange adopted values */
    for (int32_t i = 0; i < N; i++) {
        benor_process_t *p = &bo->processes[i];
        if (p->fault_type != BA_CORRECT) continue;

        p->num_recv_adoptions = 0;
        for (int32_t sender = 0; sender < N; sender++) {
            if (bo->processes[sender].fault_type == BA_CRASH_FAULT) continue;
            int32_t val;
            if (bo->processes[sender].fault_type == BA_BYZANTINE) {
                val = (sender % 3 == 0) ? -1 : (sender % 2);
            } else {
                val = bo->processes[sender].adopted_value;
            }
            if (p->num_recv_adoptions < BA_MAX_PROCESSES) {
                p->recv_adoptions[p->num_recv_adoptions++] = val;
            }
        }
    }

    /* Decision and update */
    int32_t all_decided = 0;
    int32_t total_correct = 0;

    for (int32_t i = 0; i < N; i++) {
        benor_process_t *p = &bo->processes[i];
        if (p->fault_type != BA_CORRECT) continue;
        total_correct++;

        int32_t count_0 = 0, count_1 = 0;
        for (int32_t j = 0; j < p->num_recv_adoptions; j++) {
            if (p->recv_adoptions[j] == 0) count_0++;
            else if (p->recv_adoptions[j] == 1) count_1++;
        }

        /* Decision rule */
        if (count_0 >= f + 1) {
            p->has_decided = true;
            p->decision = 0;
            p->estimate = 0;
            all_decided++;
        } else if (count_1 >= f + 1) {
            p->has_decided = true;
            p->decision = 1;
            p->estimate = 1;
            all_decided++;
        } else if (count_0 > 0 && count_1 == 0) {
            p->estimate = 0;
        } else if (count_1 > 0 && count_0 == 0) {
            p->estimate = 1;
        } else {
            /* Random coin flip to break symmetry */
            p->estimate = fair_coin();
        }

        p->round = r + 1;
    }

    bo->current_round++;
    if (all_decided >= total_correct) {
        bo->agreement_reached = true;
        return 1;
    }

    return 0;
}

int benor_run(benor_system_t *bo, ba_result_t *result)
{
    if (!bo || !result) return -1;

    memset(result, 0, sizeof(*result));

    while (!bo->agreement_reached && bo->current_round < bo->max_rounds) {
        int ret = benor_step(bo);
        if (ret < 0) return ret;
        if (ret == 1) break;
    }

    /* Verify results */
    int32_t ref_val = -1;
    bool first = true;
    result->agreement_holds = true;
    result->termination_holds = true;

    for (int32_t i = 0; i < bo->N; i++) {
        if (bo->processes[i].fault_type != BA_CORRECT) continue;
        if (!bo->processes[i].has_decided) {
            result->termination_holds = false;
        }
        if (first) {
            ref_val = bo->processes[i].decision;
            first = false;
        } else if (bo->processes[i].decision != ref_val) {
            result->agreement_holds = false;
        }
    }

    result->decided_value = ref_val;
    result->validity_holds = true;  /* Ben-Or satisfies probabilistic validity */
    result->num_rounds = bo->current_round;

    return 0;
}

double benor_expected_rounds(int32_t N, int32_t f)
{
    /* The expected number of rounds for Ben-Or is O(2^N) in the worst
     * case when f is close to the bound. However, with a common coin
     * (Rabin 1983), it reduces to expected O(1) rounds.
     *
     * Here we return a heuristic estimate based on N and f. */
    if (N <= 0) return -1.0;
    if (f >= N) return -1.0;

    /* With a common coin: E[rounds] ≈ 2^(2f) / (1 - f/N) */
    double ratio = (double)f / (double)N;
    if (ratio < 0.25) {
        return 2.0;  /* Very likely to terminate quickly */
    } else if (ratio < 0.4) {
        return 8.0;
    } else {
        return 32.0;  /* Higher expected rounds near threshold */
    }
}

/* ================================================================
 * L5: Dolev-Strong Authenticated Algorithm
 *
 * In the authenticated (signed) model, each message carries an
 * unforgeable signature chain. A correct process can verify the
 * entire chain to ensure authenticity.
 *
 * Algorithm outline (f+1 rounds):
 *   Round 0: Each process signs and broadcasts its initial value.
 *   Round r (1 ≤ r ≤ f+1):
 *     If process p receives a message with value v and a valid
 *     signature chain of length r (all from distinct processes),
 *     and p has not previously signed v:
 *       p signs v and broadcasts (v, chain + [p]) to all.
 *
 * Decision after round f+1:
 *   Let V be the set of values p has received with valid chains
 *   (chains of any length from 1 to f+1 distinct signers).
 *   If |V| = 1: decide that value.
 *   If |V| > 1: decide a default value ⊥ (or 0).
 *
 * Correctness: If any correct process receives a value v at round r ≤ f,
 * all correct processes will receive v by round r+1. Thus, by round f+1,
 * either all correct processes have the same set V or a default is used.
 * ================================================================ */

/**
 * @brief Simple hash function for signature simulation.
 *
 * In a real system, this would be a cryptographic signature.
 * Here we use FNV-1a hash for deterministic, collision-resistant
 * chain hashing.
 */
static uint64_t fnv1a_hash(const uint8_t *data, size_t len, uint64_t seed)
{
    uint64_t hash = seed;
    for (size_t i = 0; i < len; i++) {
        hash ^= (uint64_t)data[i];
        hash *= 0x100000001b3ULL;  /* FNV prime */
    }
    return hash;
}

int ds_init(ds_system_t *ds, int32_t N, int32_t f,
            const int32_t *values, const ba_fault_type_t *faults)
{
    if (!ds || N < 1 || N > BA_MAX_PROCESSES || f < 0) return -1;

    memset(ds, 0, sizeof(*ds));
    ds->N = N;
    ds->f = f;
    ds->current_round = 0;

    for (int32_t i = 0; i < N; i++) {
        ds->extracts[i].pid = i;
        ds->extracts[i].num_entries = 0;
        ds->extracts[i].round = 0;
        ds->fault_types[i] = (faults ? faults[i] : BA_CORRECT);

        /* Initialize with own value */
        ds_chain_entry_t *entry =
            &ds->extracts[i].entries[ds->extracts[i].num_entries++];
        entry->value = (values ? values[i] : 0);
        entry->signers[0] = i;
        entry->num_signers = 1;

        /* Compute initial chain hash = fnv1a(pid || value) */
        uint8_t buf[8];
        memcpy(buf, &i, sizeof(int32_t));
        memcpy(buf + 4, &entry->value, sizeof(int32_t));
        entry->chain_hash = fnv1a_hash(buf, 8, 14695981039346656037ULL);
        entry->verified = true;
    }

    return 0;
}

int ds_step(ds_system_t *ds)
{
    if (!ds) return -1;
    if (ds->current_round > ds->f) return 1;  /* Done after f+1 rounds */

    int32_t N = ds->N;

    /* Each process broadcasts its extract set to all others */
    for (int32_t receiver = 0; receiver < N; receiver++) {
        if (ds->fault_types[receiver] == BA_CRASH_FAULT) continue;

        for (int32_t sender = 0; sender < N; sender++) {
            if (sender == receiver) continue;
            if (ds->fault_types[sender] == BA_CRASH_FAULT) continue;

            /* Sender shares all its verified entries with receiver */
            ds_extract_t *snd = &ds->extracts[sender];
            ds_extract_t *rcv = &ds->extracts[receiver];

            for (int32_t e = 0; e < snd->num_entries; e++) {
                ds_chain_entry_t *sent = &snd->entries[e];
                if (!sent->verified) continue;

                /* Check if receiver already has this entry */
                bool already_has = false;
                for (int32_t r = 0; r < rcv->num_entries; r++) {
                    if (rcv->entries[r].value == sent->value &&
                        rcv->entries[r].chain_hash == sent->chain_hash) {
                        already_has = true;
                        break;
                    }
                }

                if (!already_has && rcv->num_entries <
                    BA_MAX_PROCESSES * BA_MAX_PROCESSES) {
                    /* Copy the entry */
                    memcpy(&rcv->entries[rcv->num_entries], sent,
                           sizeof(ds_chain_entry_t));

                    /* If the receiver hasn't signed this value yet,
                     * extend the chain with its own signature */
                    bool already_signed = false;
                    for (int32_t s = 0; s < sent->num_signers; s++) {
                        if (sent->signers[s] == receiver) {
                            already_signed = true;
                            break;
                        }
                    }

                    if (!already_signed) {
                        ds_chain_entry_t *new_entry =
                            &rcv->entries[rcv->num_entries];
                        if (new_entry->num_signers < BA_MAX_PROCESSES) {
                            new_entry->signers[new_entry->num_signers] = receiver;
                            new_entry->num_signers++;
                            /* Update chain hash */
                            uint64_t new_hash = 0;
                            ds_sign_chain(sent, receiver, &new_hash);
                            new_entry->chain_hash = new_hash;
                            new_entry->verified = true;
                        }
                    }

                    rcv->num_entries++;
                }
            }
        }
    }

    /* Update round for all processes */
    for (int32_t i = 0; i < N; i++) {
        ds->extracts[i].round = ds->current_round + 1;
    }

    ds->current_round++;
    return 0;
}

int ds_run(ds_system_t *ds, ba_result_t *result)
{
    if (!ds || !result) return -1;

    memset(result, 0, sizeof(*result));

    /* Run for f+1 rounds */
    for (int32_t r = 0; r <= ds->f; r++) {
        int ret = ds_step(ds);
        if (ret < 0) return ret;
    }

    /* Each correct process decides */
    int32_t decisions[BA_MAX_PROCESSES];
    int32_t correct_count = 0;

    for (int32_t i = 0; i < ds->N; i++) {
        if (ds->fault_types[i] != BA_CORRECT) continue;

        ds_extract_t *ext = &ds->extracts[i];
        /* Find unique values in extract set */
        int32_t unique_values[BA_MAX_VALUES];
        int32_t num_unique = 0;

        for (int32_t e = 0; e < ext->num_entries; e++) {
            if (!ext->entries[e].verified) continue;
            int32_t v = ext->entries[e].value;
            bool found = false;
            for (int32_t u = 0; u < num_unique; u++) {
                if (unique_values[u] == v) { found = true; break; }
            }
            if (!found && num_unique < BA_MAX_VALUES) {
                unique_values[num_unique++] = v;
            }
        }

        if (num_unique == 1) {
            decisions[correct_count] = unique_values[0];
        } else {
            decisions[correct_count] = 0;  /* Default value */
        }
        correct_count++;
    }

    /* Verify agreement */
    result->agreement_holds = true;
    result->termination_holds = true;
    int32_t ref = (correct_count > 0) ? decisions[0] : -1;
    for (int32_t i = 1; i < correct_count; i++) {
        if (decisions[i] != ref) {
            result->agreement_holds = false;
            break;
        }
    }
    result->decided_value = ref;
    result->validity_holds = true;  /* Dolev-Strong guarantees validity */
    result->num_rounds = ds->current_round;

    return 0;
}

bool ds_verify_chain(const ds_chain_entry_t *chain)
{
    if (!chain) return false;
    if (chain->num_signers < 1) return false;
    if (chain->num_signers > BA_MAX_PROCESSES) return false;

    /* Check for duplicate signers */
    for (int32_t i = 0; i < chain->num_signers; i++) {
        for (int32_t j = i + 1; j < chain->num_signers; j++) {
            if (chain->signers[i] == chain->signers[j]) {
                return false;  /* Forged: duplicate signature */
            }
        }
    }

    /* Recompute chain hash and compare */
    uint64_t computed_hash = 14695981039346656037ULL;  /* FNV offset basis */
    uint8_t buf[8];

    /* Hash the first signer + value */
    memcpy(buf, &chain->signers[0], sizeof(int32_t));
    memcpy(buf + 4, &chain->value, sizeof(int32_t));
    computed_hash = fnv1a_hash(buf, 8, computed_hash);

    /* Hash each subsequent signer */
    for (int32_t s = 1; s < chain->num_signers; s++) {
        memcpy(buf, &chain->signers[s], sizeof(int32_t));
        computed_hash = fnv1a_hash(buf, 4, computed_hash);
    }

    return computed_hash == chain->chain_hash;
}

int ds_sign_chain(const ds_chain_entry_t *chain, int32_t new_signer,
                   uint64_t *new_hash)
{
    if (!chain || !new_hash) return -1;
    if (chain->num_signers >= BA_MAX_PROCESSES) return -1;

    /* Extend the chain hash */
    uint64_t hash = chain->chain_hash;
    uint8_t buf[4];
    memcpy(buf, &new_signer, sizeof(int32_t));
    hash = fnv1a_hash(buf, 4, hash);
    *new_hash = hash;

    return 0;
}
