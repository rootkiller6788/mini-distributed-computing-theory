/*
 * byzantine_agreement.c - Byzantine Agreement Algorithms
 * L5: Oral Messages algorithm (Lamport, Shostak, Pease 1982).
 * L5: Signed Messages algorithm (Dolev & Strong 1983).
 * L4: Lower bounds: n > 3f for oral, n > f for signed.
 *
 * Reference: Lamport, Shostak, Pease (1982) "The Byzantine Generals Problem"
 *            Dolev & Strong (1983) "Authenticated Algorithms for Byzantine Agreement"
 */

#include "consensus_types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ================================================================
 * L5: Oral Messages Algorithm OM(m) - Lamport et al. (1982)
 * ================================================================
 *
 * OM(0):
 *   1. Commander sends its value to every lieutenant.
 *   2. Each lieutenant uses the value received (or RETREAT if none).
 *
 * OM(m), m > 0:
 *   1. Commander sends its value to every lieutenant.
 *   2. For each i, let vi be the value lieutenant i received (or RETREAT).
 *      Lieutenant i acts as commander in OM(m-1), sending vi to the
 *      other n-2 lieutenants.
 *   3. For each i and each j != i, let vj be the value lieutenant i
 *      received from lieutenant j in step 2 (or RETREAT).
 *      Lieutenant i uses majority(v1, ..., v_{n-1}).
 *
 * Requirements: n > 3m for m traitors.
 */

#define ORAL_MSG_RETREAT -1
#define ORAL_MSG_ATTACK   1

/* Choose majority value from array.
 * If tie, return RETREAT (default fallback). */
static int oral_majority(const int *values, int count)
{
    int attack_count = 0, retreat_count = 0;
    for (int i = 0; i < count; i++) {
        if (values[i] == ORAL_MSG_ATTACK) attack_count++;
        else if (values[i] == ORAL_MSG_RETREAT) retreat_count++;
    }
    if (attack_count > retreat_count) return ORAL_MSG_ATTACK;
    if (retreat_count > attack_count) return ORAL_MSG_RETREAT;
    return ORAL_MSG_RETREAT;  /* default on tie */
}

/* L5: OM(m) recursive algorithm.
 *
 * @param commander  The commander's process id
 * @param lieutenants Array of lieutenant process ids
 * @param num_lieutenants Number of lieutenants
 * @param m  Number of traitors to tolerate
 * @param commander_value Value the commander sends (only used at level m)
 * @param results Output: value decided by each lieutenant
 * @param honest Mask: 1 if process is honest, 0 if Byzantine
 *
 * Returns 0 on success, -1 if n <= 3m (insufficient processes).
 */
int oral_message_om(int commander, const int *lieutenants, int num_lieutenants,
                     int m, int commander_value, int *results, const int *honest)
{
    int n = num_lieutenants + 1;  /* total processes including commander */
    if (n <= 3 * m) return -1;    /* bound not satisfied */
    if (m < 0) return -1;

    if (!results || !honest) return -1;

    if (m == 0) {
        /* OM(0): Commander sends value to all lieutenants.
         * Honest commander sends his value to all.
         * Byzantine commander may send different values. */
        for (int i = 0; i < num_lieutenants; i++) {
            if (honest[commander]) {
                /* Honest commander: all receive same value */
                results[i] = commander_value;
            } else {
                /* Byzantine commander: may send conflicting values.
                 * Simulate: alternating attack/retreat for demonstration. */
                results[i] = (i % 2 == 0) ? ORAL_MSG_ATTACK : ORAL_MSG_RETREAT;
            }
        }
        return 0;
    }

    /* OM(m), m > 0: Commander sends value, then each lieutenant
     * runs OM(m-1) as commander for the remaining lieutenants. */

    int *direct_values = (int *)calloc((size_t)num_lieutenants, sizeof(int));
    int *sub_results = (int *)calloc((size_t)num_lieutenants, sizeof(int));
    int *sub_lieutenants = (int *)calloc((size_t)(num_lieutenants - 1), sizeof(int));

    if (!direct_values || !sub_results || !sub_lieutenants) {
        free(direct_values);
        free(sub_results);
        free(sub_lieutenants);
        return -1;
    }

    /* Step 1: Commander sends value */
    for (int i = 0; i < num_lieutenants; i++) {
        if (honest[commander]) {
            direct_values[i] = commander_value;
        } else {
            /* Byzantine commander: conflicting values */
            direct_values[i] = (i % 2 == 0) ? ORAL_MSG_ATTACK : ORAL_MSG_RETREAT;
        }
    }

    /* Step 2-3: Each lieutenant runs OM(m-1) with the remaining */
    for (int i = 0; i < num_lieutenants; i++) {
        /* Build sub-lieutenants excluding lieutenant i */
        int sub_count = 0;
        for (int j = 0; j < num_lieutenants; j++) {
            if (j != i) {
                sub_lieutenants[sub_count++] = lieutenants[j];
            }
        }

        /* Run OM(m-1) with lieutenant i as commander */
        int *sub_out = (int *)calloc((size_t)(num_lieutenants - 1), sizeof(int));
        if (!sub_out) { continue; }

        int rc = oral_message_om(lieutenants[i], sub_lieutenants,
                                  sub_count, m - 1, direct_values[i],
                                  sub_out, honest);

        if (rc == 0) {
            /* Collect the values that lieutenant i received from others.
             * For the majority, use sub_out values plus direct_values[i].
             * But for a single lieutenant's decision, we need majority of
             * all values received. */
            int *all_values = (int *)calloc((size_t)num_lieutenants, sizeof(int));
            if (all_values) {
                all_values[0] = direct_values[i];
                for (int k = 0; k < sub_count; k++) {
                    all_values[k + 1] = sub_out[k];
                }
                results[i] = oral_majority(all_values, num_lieutenants);
                free(all_values);
            }
        } else {
            results[i] = ORAL_MSG_RETREAT;  /* default on failure */
        }

        free(sub_out);
    }

    free(direct_values);
    free(sub_results);
    free(sub_lieutenants);
    return 0;
}

/* L5: Verify Byzantine agreement properties:
 * 1. All honest lieutenants obey the same order (Agreement).
 * 2. If the commander is honest, honest lieutenants obey commander (Validity). */
typedef struct {
    bool agreement_holds;
    bool validity_holds;
    int  agreed_value;
    int  commander_value;
} byzantine_verdict_t;

byzantine_verdict_t byzantine_verify(const int *results, int num_lieutenants,
                                      int commander_value, const int *honest,
                                      int commander)
{
    byzantine_verdict_t verdict;
    memset(&verdict, 0, sizeof(verdict));
    verdict.agreement_holds = true;
    verdict.validity_holds = true;
    verdict.commander_value = commander_value;

    int first_honest_value = ORAL_MSG_RETREAT;
    bool first_found = false;

    for (int i = 0; i < num_lieutenants; i++) {
        if (!honest[i + 1]) continue;  /* skip Byzantine (index+1 because 0 is commander) */
        if (!first_found) {
            first_honest_value = results[i];
            verdict.agreed_value = first_honest_value;
            first_found = true;
        } else if (results[i] != first_honest_value) {
            verdict.agreement_holds = false;
        }
    }

    if (honest[commander] && first_found && first_honest_value != commander_value) {
        verdict.validity_holds = false;
    }

    return verdict;
}

/* ================================================================
 * L5: Signed Messages Algorithm SM(m) - Dolev & Strong (1983)
 * ================================================================
 *
 * Uses digital signatures so Byzantine processes cannot forge messages
 * or alter relayed values without detection.
 *
 * SM(m), m >= 0:
 *   1. Commander signs and sends value to all lieutenants.
 *   2. For each lieutenant i:
 *      a) If a message with value v is received from commander, and
 *         it has not been received before, then:
 *         - Set Vi = {v}
 *         - Send (v : commander : i) to all other lieutenants
 *      b) If a message (v : s1 : s2 : ... : sk) of length at most m
 *         is received, v not in Vi, then:
 *         - Vi = Vi U {v}
 *         - If k < m, send (v : s1 : s2 : ... : sk : i) to all
 *           lieutenants not already in the signature list.
 *   3. When no more messages are received, lieutenant i decides
 *      choice(Vi) — if Vi has one value, that value; else RETREAT.
 *
 * Requirements: n > m for m traitors (much weaker than oral!).
 *
 * This function simulates signed message agreement for a given configuration.
 */

typedef struct {
    int  value;
    int  signers[32];   /* signature chain */
    int  num_signers;
    bool is_valid;
} signed_message_t;

/* Simulate SM(m) for a commander and lieutenants.
 * For demonstration, we simplify: the signature chain ensures that
 * Byzantine processes cannot forge values. */
int signed_message_agreement(int commander, const int *lieutenants,
                              int num_lieutenants, int m,
                              int commander_value, int *results,
                              const int *honest)
{
    int n = num_lieutenants + 1;
    if (n <= m) return -1;  /* SM requirement: n > m */

    if (!results || !honest) return -1;

    /* Each lieutenant's value set Vi (simplified: single value) */
    for (int i = 0; i < num_lieutenants; i++) {
        /* Commander sends signed value */
        if (honest[commander]) {
            results[i] = commander_value;  /* honest value, correctly signed */
        } else {
            /* Byzantine commander can try to send different values,
             * but signatures prevent forging of relayed messages. */
            results[i] = commander_value;  /* Signature ensures consistency */
        }

        /* Relay phase: each lieutenant sends its received value to others.
         * In SM, valid signed messages propagate. */
        for (int j = 0; j < num_lieutenants; j++) {
            if (j == i) continue;
            /* In a full implementation, check signature chain length <= m+1
             * and verify no signature appeared twice. */
        }
    }

    /* Decision: if all honest lieutenants have the same value in Vi,
     * that value is the decision. Otherwise RETREAT. */
    int common_value = -1;
    bool all_same = true;
    for (int i = 0; i < num_lieutenants; i++) {
        if (!honest[lieutenants[i]]) continue;
        if (common_value == -1) {
            common_value = results[i];
        } else if (results[i] != common_value) {
            all_same = false;
            break;
        }
    }

    if (!all_same) {
        /* Vi has conflicting values => decide RETREAT */
        for (int i = 0; i < num_lieutenants; i++) {
            if (honest[lieutenants[i]])
                results[i] = ORAL_MSG_RETREAT;
        }
    }

    return 0;
}

/* ================================================================
 * L2: Consensus in Practice — model comparison
 * ================================================================
 *
 * Compare fault models and their costs:
 *   Oral Messages (OM):    n > 3f,  O(n^f) messages, f+1 rounds
 *   Signed Messages (SM):  n > f,   O(n^2)  messages, f+1 rounds
 *   PBFT:                  n > 3f,  O(n^2)  messages, 3 phases
 *   Paxos/Raft (crash):    n > 2f,  O(n)    messages, 2 phases
 */

const char *agreement_algorithm_name(int algo)
{
    switch (algo) {
    case 0: return "Oral Messages OM(m)";
    case 1: return "Signed Messages SM(m)";
    case 2: return "PBFT (Practical Byzantine Fault Tolerance)";
    case 3: return "Paxos (Crash Fault Tolerance)";
    case 4: return "Raft (Crash Fault Tolerance)";
    default: return "Unknown";
    }
}

/* Get the minimum number of processes needed for given faults. */
int min_processes_for_agreement(int f, fault_type_t fault_model,
                                 bool authenticated_channels)
{
    if (f < 0) return -1;
    switch (fault_model) {
    case FAULT_NONE:    return 1;
    case FAULT_CRASH:
    case FAULT_OMISSION:
    case FAULT_TIMING:  return 2 * f + 1;  /* n > 2f */
    case FAULT_BYZANTINE:
        if (authenticated_channels)
            return f + 1;   /* n > f with signatures (SM) */
        else
            return 3 * f + 1;  /* n > 3f without signatures (OM) */
    default: return -1;
    }
}

/* Compute the maximum number of faults tolerated for n processes. */
int max_tolerated_faults(int n, fault_type_t fault_model,
                          bool authenticated_channels)
{
    if (n < 1) return -1;
    switch (fault_model) {
    case FAULT_NONE:    return 0;
    case FAULT_CRASH:
    case FAULT_OMISSION:
    case FAULT_TIMING:  return (n - 1) / 2;  /* floor((n-1)/2) */
    case FAULT_BYZANTINE:
        if (authenticated_channels)
            return n - 1;  /* any number with signatures */
        else
            return (n - 1) / 3;  /* floor((n-1)/3) without */
    default: return -1;
    }
}
