/**
 * example_sensor_network.c — Sensor Network Aggregation using Synchronizers
 *
 * Demonstrates a realistic L7 application: wireless sensor network
 * (e.g., IoT environmental monitoring) where nodes collect temperature
 * readings and use a synchronizer-based convergecast to find the
 * maximum reading across the network.
 *
 * Real-world references:
 *   - Environmental monitoring: climate stations (IPCC WG1 2020)
 *   - Smart grid: distributed voltage monitoring
 *   - Industrial IoT: factory floor temperature mapping
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "synchro.h"
#include "distributed_algorithms.h"

int main(void) {
    printf("============================================================\n");
    printf("  Example: Sensor Network Aggregation\n");
    printf("  Application: Wireless Sensor Network (IoT)\n");
    printf("============================================================\n\n");

    /* Create a 4×4 grid representing a field with 16 sensor motes */
    network_t *net = network_gen_grid(4, 4, SYNCHRO_BETA);
    synchro_beta_init(net);

    /* Initialize randomized sensor readings (e.g., temperature in °C) */
    sensor_init_random(net, 42);

    /* Print all sensor readings */
    printf("Sensor Readings (16 motes on 4×4 grid):\n");
    printf("  Grid layout:\n");
    for (int r = 0; r < 4; r++) {
        printf("    ");
        for (int c = 0; c < 4; c++) {
            int id = r * 4 + c;
            printf("%5.1f°C ", net->nodes[id].local_sensor);
        }
        printf("\n");
    }

    /* Find expected max by local computation */
    double expected_max = net->nodes[0].local_sensor;
    for (int i = 1; i < 16; i++) {
        if (net->nodes[i].local_sensor > expected_max)
            expected_max = net->nodes[i].local_sensor;
    }
    printf("\n  Expected max: %.1f°C (local computation)\n", expected_max);

    /* Run distributed aggregation via Beta synchronizer */
    synchro_reset_counters(net);
    int rounds = synchro_run_protocol(net, sensor_aggregate_max_round, 20);

    int result_raw = net->nodes[0].local_data;
    double result_c = (double)result_raw / 100.0;

    printf("  Distributed result (at sink node 0): %.1f°C\n", result_c);
    printf("  Rounds required: %d\n", rounds);
    printf("  Messages exchanged: %zu\n", synchro_get_message_complexity(net));
    printf("\n  Correctness: %s (expected %.1f°C, got %.1f°C)\n",
           (fabs(result_c - expected_max) < 0.01) ? "PASS" : "FAIL",
           expected_max, result_c);

    /* Compare with Alpha synchronizer */
    network_t *net_a = network_gen_grid(4, 4, SYNCHRO_ALPHA);
    synchro_alpha_init(net_a);
    sensor_init_random(net_a, 42);
    synchro_reset_counters(net_a);

    printf("\n--- Comparison: Alpha vs Beta for Sensor Aggregation ---\n");
    synchro_run_protocol(net_a, sensor_aggregate_max_round, 20);
    printf("  Alpha: %zu messages on grid (|E|=%d)\n",
           synchro_get_message_complexity(net_a), net_a->num_edges);
    printf("  Beta:  %zu messages on grid (|V|=%d)\n",
           synchro_get_message_complexity(net), net->num_nodes);

    printf("\n============================================================\n");
    printf("  Real-World Mapping:\n");
    printf("  - IPCC Climate Monitoring (global temperature aggregation)\n");
    printf("  - Smart Grid PMU (Phasor Measurement Unit) networks\n");
    printf("  - Industrial IoT (factory floor sensor mesh)\n");
    printf("============================================================\n");

    network_destroy(net);
    network_destroy(net_a);
    return 0;
}
