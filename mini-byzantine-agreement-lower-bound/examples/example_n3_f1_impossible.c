/**
 * @file example_n3_f1_impossible.c
 * @brief End-to-end demonstration: N=3, f=1, oral messages — IMPOSSIBLE.
 *
 * This example walks through the classical LSP82 proof showing why
 * Byzantine agreement with N=3 and f=1 (oral messages) is impossible.
 *
 * The three scenarios A, B, C are constructed and the contradiction
 * is displayed interactively.
 *
 * Reference: Lamport, Shostak, Pease (1982) Theorem 1
 */

#include "byzantine_agreement.h"
#include "impossibility_proof.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  Byzantine Agreement — N=3, f=1 Impossibility Proof\n");
    printf("  Lamport, Shostak, Pease (1982) Theorem 1\n");
    printf("═══════════════════════════════════════════════════════════\n\n");

    /* 1. Initialize a 3-process system */
    ba_system_t sys;
    if (ba_system_init(&sys, 3, 1, BA_MSG_ORAL, BA_SYNC) != 0) {
        printf("ERROR: System init failed\n");
        return 1;
    }

    printf("System: N=%d, f=%d, oral messages, synchronous\n", 3, 1);
    printf("Is Byzantine agreement achievable? %s\n\n",
           ba_is_achievable(&sys) ? "YES" : "NO (by LSP82 Theorem 1)");

    /* 2. Construct the proof */
    lsp82_proof_t proof;
    if (lsp82_construct_proof(&proof) != 0) {
        printf("ERROR: Proof construction failed\n");
        return 1;
    }

    /* 3. Display the three scenarios */
    for (int s = 0; s < NUM_LSP82_SCENARIOS; s++) {
        lsp82_transcript_t *sc = &proof.scenarios[s];
        printf("───────────────────────────────────────────────────────────\n");
        printf("Scenario %c:\n", 'A' + s);
        printf("  General (P0): %s, %s\n",
               sc->g_correct ? "correct" : "FAULTY",
               sc->g_correct ? (sc->g_value == 0 ? "proposes 0" : "proposes 1")
                             : "equivocates (sends 0 to L1, 1 to L2)");
        printf("  Lieutenant 1 (P1): %s\n",
               sc->l1_correct ? "correct" : "FAULTY");
        printf("  Lieutenant 2 (P2): %s\n",
               sc->l2_correct ? "correct" : "FAULTY");

        printf("  Messages:\n");
        for (int32_t m = 0; m < sc->num_messages; m++) {
            printf("    Round %d: P%d → P%d: ",
                   sc->messages[m].round,
                   sc->messages[m].sender,
                   sc->messages[m].receiver);

            if (sc->messages[m].round == 1) {
                printf("%d\n", sc->messages[m].reported_value);
            } else {
                /* Round 2: relay */
                printf("P%d said %d\n",
                       sc->messages[m].reported_sender,
                       sc->messages[m].reported_value);
            }
        }
        printf("\n");
    }

    /* 4. Display indistinguishability relations */
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  Indistinguishability Analysis\n");
    printf("═══════════════════════════════════════════════════════════\n\n");

    for (int32_t i = 0; i < proof.num_indist_pairs; i++) {
        printf("  Scenario %c is INDISTINGUISHABLE from Scenario %c\n",
               'A' + proof.indist_pairs[i].a,
               'A' + proof.indist_pairs[i].b);
        printf("    to Process %d (Lieutenant %d)\n\n",
               proof.indist_pairs[i].pid,
               proof.indist_pairs[i].pid);
    }

    printf("Why?\n");
    printf("  A ~_L1 C: In both A and C, L1 hears G→0 directly.\n");
    printf("            L1 cannot tell whether G is faulty (C) or\n");
    printf("            L1 itself is faulty (A).\n\n");

    printf("  B ~_L2 C: In both B and C, L2 hears G→1 directly.\n");
    printf("            L2 cannot tell whether G is faulty (C) or\n");
    printf("            L2 itself is faulty (B).\n\n");

    /* 5. The contradiction */
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  THE CONTRADICTION\n");
    printf("═══════════════════════════════════════════════════════════\n\n");

    printf("  %s\n\n", proof.contradiction_chain);

    printf("Therefore: NO deterministic protocol can achieve Byzantine\n");
    printf("Agreement with N=3 processes and f=1 faulty process using\n");
    printf("oral (unsigned) messages.\n\n");

    /* 6. Verify the proof */
    printf("Proof verification: %s\n\n",
           lsp82_verify_proof(&proof) ? "VALID ✓" : "INVALID ✗");

    /* 7. Extension to general N */
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  Extension to General N, f ≥ N/3 (Simulation Argument)\n");
    printf("═══════════════════════════════════════════════════════════\n\n");

    printf("  For any N, f with f ≥ N/3:\n");
    printf("  Partition N into 3 groups G₀, G₁, G₂ of size ≤ f each.\n");
    printf("  Each group simulates one of the 3 processes.\n");
    printf("  A protocol for (N, f) → a protocol for (3, 1).\n");
    printf("  Contradiction! Therefore, f < N/3 is NECESSARY.\n\n");

    printf("  Examples:\n");
    printf("    N=3,  f=1:  3*1 ≥ 3  → IMPOSSIBLE  ✓\n");
    printf("    N=4,  f=1:  3*1 < 4  → POSSIBLE    (EIG algorithm)\n");
    printf("    N=7,  f=2:  3*2 < 7  → POSSIBLE    (EIG algorithm)\n");
    printf("    N=10, f=3:  3*3 < 10 → POSSIBLE    (EIG algorithm)\n");
    printf("    N=10, f=4:  3*4 ≥ 10 → IMPOSSIBLE\n\n");

    lsp82_proof_destroy(&proof);
    ba_system_destroy(&sys);

    return 0;
}
