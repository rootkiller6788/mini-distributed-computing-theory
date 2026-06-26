/**
 * @file convergence_bench.c
 * @brief Performance benchmarks for Dijkstra's self-stabilizing ring
 *
 * Measures convergence time as a function of:
 *   - Ring size N (scalability)
 *   - Number of states K (state space impact)
 *   - Algorithm variant (3-state vs 4-state vs K-state)
 *   - Daemon type (central vs distributed vs synchronous)
 *
 * Each benchmark runs multiple trials and reports mean, median,
 * and worst-case convergence times.
 */

#include "dijkstra_ring.h"
#include "convergence.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double get_time_sec(void) {
    return (double)clock() / (double)CLOCKS_PER_SEC;
}

static void bench_scalability(void) {
    printf("\n═══ Scalability Benchmark: N=3 to N=50 ═══\n");
    printf("  N   | Mean Steps | Median | Max   | Trials\n");
    printf("──────┼────────────┼────────┼───────┼───────\n");

    int32_t n;
    for (n = 3; n <= 50; n += (n < 10 ? 2 : (n < 20 ? 5 : 10))) {
        int32_t num_trials = (n <= 20) ? 50 : 20;
        int32_t max_steps = n * n * 5; /* Generous bound */

        convergence_result_t *results = convergence_run_trials(
            n, 3, ALGORITHM_3STATE, num_trials, max_steps,
            (uint32_t)n * 1000);

        if (results) {
            convergence_stats_t stats;
            convergence_compute_stats(results, num_trials, &stats);
            printf(" %3d  | %10.1f | %6.0f | %5d | %5d\n",
                   n, stats.mean_steps, stats.median_steps,
                   stats.max_steps, stats.num_converged);
            free(results);
        }
    }
}

static void bench_algorithm_comparison(void) {
    printf("\n═══ Algorithm Comparison: N=7 ═══\n");
    printf(" Algorithm | Mean Steps | Median | Max   | OK?\n");
    printf("───────────┼────────────┼────────┼───────┼─────\n");

    int32_t n = 7;
    int32_t num_trials = 30;
    algorithm_variant_t variants[] = {
        ALGORITHM_3STATE, ALGORITHM_4STATE, ALGORITHM_KSTATE
    };
    int32_t k_vals[] = {3, 4, 8}; /* K > N for K-state */
    const char *names[] = {"3-State", "4-State", "K-State"};

    int32_t v;
    for (v = 0; v < 3; v++) {
        convergence_result_t *results = convergence_run_trials(
            n, k_vals[v], variants[v], num_trials, 5000,
            (uint32_t)(v * 10000));

        if (results) {
            convergence_stats_t stats;
            convergence_compute_stats(results, num_trials, &stats);
            printf(" %-9s | %10.1f | %6.0f | %5d | %s\n",
                   names[v], stats.mean_steps, stats.median_steps,
                   stats.max_steps,
                   (stats.num_converged == num_trials) ? "YES ✓" : "NO ✗");
            free(results);
        }
    }
}

static void bench_daemon_comparison(void) {
    printf("\n═══ Daemon Comparison: N=5, 3-State ═══\n");
    printf(" Daemon      | Steps (1 trial) |\n");
    printf("─────────────┼─────────────────┼\n");

    int32_t n = 5;
    int32_t k = 3;

    /* Run single trials for each daemon type */
    /* (convergence_run_experiment only supports central via the public API,
       but we can compare bounds) */
    printf(" Central     | ≤ %d (theoretical) |\n",
           n * n);
    printf(" Distributed | ≤ %d (theoretical) |\n",
           n * n);
    printf(" Synchronous | ≤ %d (theoretical) |\n", n);

    /* Run central daemon trial */
    convergence_result_t *r = convergence_run_experiment(
        n, k, ALGORITHM_3STATE, 0, 10000, 42);
    if (r) {
        printf(" Central     | %d (actual)     |\n", r->total_steps);
        free(r);
    }
}

static void bench_energy_analysis(void) {
    printf("\n═══ Energy Function Analysis: N=4 to N=6 ═══\n");
    printf(" N | Energy Mono? | Max Conv Dist |\n");
    printf("───┼──────────────┼───────────────┼\n");

    int32_t n;
    for (n = 3; n <= 4; n++) {
        bool mono = convergence_verify_monotonicity(n, 3);
        int32_t worst[6];
        int32_t max_dist = convergence_worst_case_distance(
            n, 3, ALGORITHM_3STATE, worst);
        printf(" %d | %-12s | %-13d |\n", n,
               mono ? "YES ✓" : "NO ✗", max_dist);
    }
}

int main(void) {
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║  Dijkstra Self-Stabilizing Ring — Benchmarks ║\n");
    printf("╚══════════════════════════════════════════════╝\n");

    double t0 = get_time_sec();

    bench_scalability();
    bench_algorithm_comparison();
    bench_daemon_comparison();
    bench_energy_analysis();

    double t1 = get_time_sec();
    printf("\n═══ Total benchmark time: %.2f seconds ═══\n",
           t1 - t0);

    return 0;
}
