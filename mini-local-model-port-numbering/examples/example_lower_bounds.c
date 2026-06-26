/**
 * @example example_lower_bounds.c
 * @brief End-to-end example: Exploring LOCAL model lower bounds.
 *
 * Demonstrates:
 *  - Linial's Ω(log* n) lower bound for 3-coloring a ring
 *  - KMW lower bound for MIS
 *  - Ramsey-theoretic thresholds
 *  - Deterministic vs randomized separations
 *  - Symmetry class analysis
 *  - The lower bound summary table
 *
 * Knowledge: L4 (Fundamental Laws / Lower Bounds)
 *
 * Theorem: Lower bounds in the LOCAL model are proved via
 * neighborhood indistinguishability, round elimination, and
 * Ramsey-theoretic arguments.
 */

#include "local_model.h"
#include "port_numbering.h"
#include "algorithms.h"
#include "lower_bounds.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int main(void) {
    printf("=== LOCAL Model Lower Bounds Explorer ===\n\n");

    /* Print the summary table */
    lb_print_summary();
    printf("\n");

    /* ---- 1. Linial's Lower Bound ---- */
    printf("=== 1. Linial's Ω(log* n) Lower Bound for 3-Coloring a Ring ===\n\n");

    uint32_t test_sizes[] = {5, 10, 50, 100, 1000, 10000, 100000};
    printf("%10s %12s %12s %s\n", "n", "log* n", "Min Rounds", "3-Col Possible?");
    printf("-----------------------------------------------------------\n");
    for (int k = 0; k < 7; k++) {
        uint32_t n = test_sizes[k];
        uint32_t logstar = iterated_log2(n);
        uint32_t min_rounds = lb_linial_ring_min_rounds(n, 3);
        bool impossible_0round = lb_linial_ring_impossible(0, n);
        bool impossible_logstar = lb_linial_ring_impossible(logstar - 1, n);

        printf("%10u %12u %12u ", n, logstar, min_rounds);
        if (impossible_0round) {
            printf("%s\n", "0-round: NO");
        } else {
            printf("%s\n", (impossible_logstar ? "T<log* n: NO" : "Possible"));
        }
    }

    /* ---- 2. KMW MIS Lower Bound ---- */
    printf("\n=== 2. KMW MIS Lower Bound = Ω(min{log Δ, √(log n)}) ===\n\n");
    printf("%8s %8s %12s %12s\n", "n", "Δ", "log Δ", "min{logΔ,√(logn)}");
    printf("------------------------------------------------\n");
    for (uint32_t delta = 2; delta <= 64; delta *= 2) {
        uint32_t n_val = delta * delta * delta;
        if (n_val > 1000000) n_val = 1000000;
        uint32_t bound = lb_kmw_mis_lower_bound(delta, n_val);
        uint32_t log_delta = (uint32_t)(log((double)delta) / log(2.0));
        uint32_t sqrt_logn = (uint32_t)(sqrt(log((double)n_val) / log(2.0)));
        printf("%8u %8u %12u %12u\n", n_val, delta, log_delta, bound);
    }

    /* ---- 3. Ramsey Thresholds ---- */
    printf("\n=== 3. Ramsey-Theoretic Thresholds ===\n\n");
    printf("Output colors | Rounds | Min graph size\n");
    printf("---------------------------------------\n");
    for (uint32_t k = 2; k <= 5; k++) {
        for (uint32_t r = 0; r <= 3; r++) {
            uint32_t threshold = lb_ramsey_threshold(k, r);
            printf("     %2u       |   %2u   | %u\n", k, r, threshold);
        }
    }

    /* ---- 4. Communication Complexity Reduction ---- */
    printf("\n=== 4. Communication Complexity Lower Bounds ===\n\n");
    printf("For a 2-party problem requiring C bits across k cut edges:\n");
    printf("  T ≥ C / (k · log n)\n\n");
    for (uint32_t cut = 1; cut <= 10; cut++) {
        uint32_t lb = lb_from_communication_complexity(100, cut, 1000);
        printf("  cut=%u edges → T ≥ %u rounds\n", cut, lb);
    }

    /* ---- 5. Deterministic vs Randomized Gap ---- */
    printf("\n=== 5. Deterministic vs Randomized Gap ===\n\n");
    printf("MIS on general graphs:\n");
    printf("  T_det = 2^O(√(log n)), T_rand = O(log n)\n");
    printf("  Gap = T_det / T_rand ≈ infinite (exponential separation)\n");

    printf("\nMIS on a path:\n");
    printf("  T_det = Ω(log* n), T_rand = O(1)\n");
    printf("  Gap = T_det / T_rand ≈ infinite\n");

    /* ---- 6. Symmetry Analysis on Ring ---- */
    printf("\n=== 6. Symmetry Class Analysis ===\n\n");
    {
        local_context_t ctx;
        local_make_cycle(&ctx, 6);

        port_numbering_t pns[6];
        port_assign_ring_orientation(pns, 6, ctx.adjacency);

        uint32_t classes = port_count_symmetry_classes(pns, 6, ctx.adjacency);
        uint32_t sb_num = port_symmetry_breaking_number(pns, 6, ctx.adjacency);

        printf("C6 ring with orientation-based port numbering:\n");
        printf("  Symmetry classes: %u\n", classes);
        printf("  Symmetry-breaking number: %u\n", sb_num);
        printf("  Interpretation: ");
        if (classes == 1) {
            printf("All nodes are structurally identical (no symmetry breaking from ports alone)\n");
            printf("  → UIDs are necessary to break symmetry for deterministic algorithms.\n");
        } else {
            printf("The port numbering breaks some symmetry.\n");
        }

        local_context_destroy(&ctx);
    }

    /* ---- 7. Problem-Specific Lower Bounds ---- */
    printf("\n=== 7. Problem-Specific Lower Bounds ===\n\n");
    printf("Problems queried for n=100, Δ=10:\n");
    printf("  3-coloring ring:  T ≥ %u rounds\n", lb_for_problem("3-coloring", 100, 10));
    printf("  MIS:              T ≥ %u rounds\n", lb_for_problem("MIS", 100, 10));
    printf("  Sinkless orient:  T ≥ %u rounds\n", lb_for_problem("sinkless", 100, 10));
    printf("  (Δ+1)-coloring:   T ≥ %u rounds\n", lb_for_problem("coloring", 100, 10));

    printf("\n=== Lower Bounds Explorer Complete ===\n");
    return 0;
}
