/**
 * @file example_blockchain_consensus.c
 * @brief End-to-end demonstration: Blockchain consensus through the lens
 *        of Byzantine agreement lower bounds.
 *
 * This example connects the classical theory (f < N/3 for BFT,
 * FLP impossibility) to practical blockchain systems:
 *   - Nakamoto consensus (Bitcoin): probabilistic finality, f < N/2
 *   - Tendermint (Cosmos): BFT finality, f < N/3
 *   - HTLC atomic swaps: 2-party case, no consensus needed
 *
 * Reference: Nakamoto (2008), Castro & Liskov (1999), Buchman (2016)
 */

#include "byzantine_agreement.h"
#include "consensus_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* Forward declarations */
extern double nakamoto_attack_success_probability(double q, int32_t z);
extern int32_t nakamoto_min_confirmations(double q, double epsilon);
extern int consensus_feasibility_analysis(int32_t f, int32_t N,
    double network_delay_s, double block_time_s, void *result);
extern int finality_analyze(int32_t f, int32_t N, int32_t block_confirmations,
    double attacker_hash_frac, void *result);

int main(void)
{
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  Blockchain Consensus & Byzantine Agreement Lower Bounds\n");
    printf("═══════════════════════════════════════════════════════════\n\n");

    /* Part 1: Classical BFT vs Nakamoto Consensus */
    printf("───────────────────────────────────────────────────────────\n");
    printf("  Part 1: Consensus Model Comparison\n");
    printf("───────────────────────────────────────────────────────────\n\n");

    printf("  %-20s %10s %10s %15s\n", "Model", "N", "Max f", "f/N Bound");
    printf("  -------------------- ---------- ---------- ---------------\n");

    for (int32_t N = 10; N <= 100; N *= 10) {
        consensus_threshold_t th_bft, th_signed, th_crash;

        consensus_get_threshold(CONSENSUS_BYZANTINE_ORAL, N, &th_bft);
        consensus_get_threshold(CONSENSUS_BYZANTINE_SIGNED, N, &th_signed);
        consensus_get_threshold(CONSENSUS_CRASH_STOP, N, &th_crash);

        printf("  BFT (oral)         %10d %10d %14.2f\n",
               N, th_bft.max_faulty, th_bft.fault_ratio);
        printf("  BFT (signed)       %10d %10d %14.2f\n",
               N, th_signed.max_faulty, th_signed.fault_ratio);
        printf("  Crash (Paxos)      %10d %10d %14.2f\n",
               N, th_crash.max_faulty, th_crash.fault_ratio);
        printf("\n");
    }

    /* Part 2: Nakamoto Attack Probability */
    printf("───────────────────────────────────────────────────────────\n");
    printf("  Part 2: Nakamoto Consensus — Double-Spend Probability\n");
    printf("───────────────────────────────────────────────────────────\n\n");

    printf("  %12s %15s %20s %20s\n",
           "Adversary %", "Confirmations", "P[attack success]", "Security (bits)");
    printf("  ------------ --------------- -------------------- --------------------\n");

    double adv_fracs[] = {0.10, 0.20, 0.30, 0.40, 0.45};
    for (int i = 0; i < 5; i++) {
        double q = adv_fracs[i];
        for (int32_t z = 1; z <= 6; z++) {
            double prob = nakamoto_attack_success_probability(q, z);
            double bits = (prob > 0) ? -log2(prob) : INFINITY;
            printf("  %11.0f%% %15d %20.6e %19.1f\n",
                   q * 100, z, prob, bits);
        }
        printf("\n");
    }

    /* Part 3: Minimum Confirmations for Security */
    printf("───────────────────────────────────────────────────────────\n");
    printf("  Part 3: Confirmations Needed for ε = 10⁻⁶\n");
    printf("───────────────────────────────────────────────────────────\n\n");

    double epsilons[] = {1e-3, 1e-6, 1e-9};
    for (int ei = 0; ei < 3; ei++) {
        printf("  ε = 10⁻%d:\n", (ei == 0) ? 3 : (ei == 1) ? 6 : 9);
        for (double q = 0.05; q < 0.45; q += 0.10) {
            int32_t k = nakamoto_min_confirmations(q, epsilons[ei]);
            printf("    q=%.0f%%  →  k=%d confirmations\n", q * 100, k);
        }
        printf("    q=50%%   →  IMPOSSIBLE (attacker has majority)\n\n");
    }

    /* Part 4: Consensus Feasibility Analysis */
    printf("───────────────────────────────────────────────────────────\n");
    printf("  Part 4: Feasibility Under Network Conditions\n");
    printf("───────────────────────────────────────────────────────────\n\n");

    printf("  Network delay = 1s, Block time = 10s:\n\n");

    struct {
        int32_t N;
        int32_t f;
    } configs[] = {{7, 2}, {10, 3}, {10, 4}, {100, 33}};

    for (int i = 0; i < 4; i++) {
        int32_t N = configs[i].N;
        int32_t f = configs[i].f;

        bool bft_possible = (3 * f < N);
        double prob = nakamoto_attack_success_probability((double)f / N, 6);

        printf("  N=%d, f=%d (f/N=%.2f):\n", N, f, (double)f / N);
        printf("    Classical BFT:    %s\n",
               bft_possible ? "POSSIBLE" : "IMPOSSIBLE");
        printf("    Nakamoto safety:  %s (6-conf P=%.2e)\n",
               prob < 1e-3 ? "SAFE" : "UNSAFE", prob);
        printf("\n");
    }

    /* Part 5: The Connection to Byzantine Agreement Lower Bound */
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  THE DEEP CONNECTION\n");
    printf("═══════════════════════════════════════════════════════════\n\n");

    printf("The f < N/3 lower bound for Byzantine agreement is not just\n");
    printf("a theoretical curiosity — it directly determines the security\n");
    printf("parameters of modern blockchain systems:\n\n");

    printf("1. PBFT/Tendermint: Requires f < N/3 validators to be faulty.\n");
    printf("   With 100 validators: can tolerate up to 33 Byzantine.\n");
    printf("   This is the DIRECT application of LSP82 Theorem 1.\n\n");

    printf("2. Nakamoto Consensus: Uses PoW to convert the f < N/2 bound\n");
    printf("   (hash power) into probabilistic security. The FLP\n");
    printf("   impossibility is circumvented by allowing non-zero\n");
    printf("   reversal probability.\n\n");

    printf("3. Ethereum 2.0 (Gasper): Hybrid approach — LMD-GHOST for\n");
    printf("   liveness (like Nakamoto) + Casper FFG for finality\n");
    printf("   (like PBFT). The BFT finality requires f < N/3 of the\n");
    printf("   staked validators.\n\n");

    printf("4. The Dolev-Strong bound (f < N/2 with signatures) explains\n");
    printf("   why signed messages in blockchain (transaction signatures)\n");
    printf("   improve over the oral message model: they prevent\n");
    printf("   equivocation by honest nodes.\n\n");

    printf("═══════════════════════════════════════════════════════════\n");
    printf("  CONCLUSION\n");
    printf("═══════════════════════════════════════════════════════════\n\n");

    printf("The Byzantine agreement lower bound (f < N/3 for oral messages)\n");
    printf("is a FUNDAMENTAL limit of distributed computing. Adding\n");
    printf("cryptography (signatures → f < N/2), randomization\n");
    printf("(Ben-Or → f < N/5 async), or economic incentives (Nakamoto →\n");
    printf("probabilistic security) can relax but never eliminate the\n");
    printf("underlying impossibility.\n\n");

    return 0;
}
