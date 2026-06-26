/**
 * @file bench_config_graph.c
 * @brief Performance benchmark: configuration graph construction.
 *
 * Measures the time to build the configuration graph for
 * increasing numbers of processes and depth limits.
 *
 * Build: make benches && ./build/bench_bench_config_graph
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "flp_common.h"
#include "flp_config.h"
#include "flp_protocol.h"
#include "flp_message.h"
#include "flp_search.h"

static double get_time_ms(void)
{
    return (double)clock() / (double)(CLOCKS_PER_SEC) * 1000.0;
}

int main(void)
{
    printf("FLP Configuration Graph — Performance Benchmark\n");
    printf("===============================================\n\n");

    flp_protocol_desc desc;
    flp_protocol_desc_init(&desc, FLP_PROTO_FLOOD_SET, 3, 1);

    printf("%-10s %-10s %-15s %-15s\n",
           "N", "Depth", "Configs", "Time(ms)");
    printf("%-10s %-10s %-15s %-15s\n",
           "----------", "----------", "---------------", "---------------");

    for (int32_t n = 2; n <= 4; n++) {
        for (int32_t depth = 1; depth <= 3; depth++) {
            flp_configuration cfg;
            int32_t inputs[4] = {0, 1, 0, 1};
            flp_config_init_with_inputs(&cfg, n, inputs, 0);

            /* Initialize: send first messages */
            for (int32_t pid = 0; pid < n; pid++) {
                flp_message nullmsg;
                memset(&nullmsg, 0, sizeof(nullmsg));
                flp_step_result step_res;
                flp_protocol_step(&desc, &cfg.processes[pid],
                                  &nullmsg, &step_res);
                cfg.processes[pid] = step_res.new_state;
                for (int32_t oi = 0; oi < step_res.num_outgoing; oi++) {
                    flp_msg_buffer_send(&cfg, &step_res.outgoing[oi]);
                }
            }

            flp_config_graph graph;
            flp_graph_init(&graph, 10000);

            double start = get_time_ms();
            int32_t count = flp_graph_build(&graph, &desc, &cfg, depth);
            double elapsed = get_time_ms() - start;

            printf("%-10d %-10d %-15d %-15.2f\n",
                   n, depth, count, elapsed);

            flp_graph_destroy(&graph);
        }
    }

    /* Larger benchmark: depth 4 with N=3 */
    printf("\n--- Larger Benchmark ---\n");
    flp_configuration cfg;
    int32_t inputs[3] = {0, 1, 0};
    flp_config_init_with_inputs(&cfg, 3, inputs, 0);

    for (int32_t pid = 0; pid < 3; pid++) {
        flp_message nullmsg;
        memset(&nullmsg, 0, sizeof(nullmsg));
        flp_step_result step_res;
        flp_protocol_step(&desc, &cfg.processes[pid],
                          &nullmsg, &step_res);
        cfg.processes[pid] = step_res.new_state;
        for (int32_t oi = 0; oi < step_res.num_outgoing; oi++) {
            flp_msg_buffer_send(&cfg, &step_res.outgoing[oi]);
        }
    }

    flp_config_graph graph;
    flp_graph_init(&graph, 50000);

    double start = get_time_ms();
    int32_t count = flp_graph_build(&graph, &desc, &cfg, 4);
    double elapsed = get_time_ms() - start;

    printf("N=3, depth=4: %d configs in %.2f ms\n", count, elapsed);
    printf("Nodes/sec: %.0f\n", (double)count / (elapsed / 1000.0));

    flp_graph_destroy(&graph);
    printf("\n===============================================\n");
    printf("Benchmark complete.\n");
    return 0;
}
