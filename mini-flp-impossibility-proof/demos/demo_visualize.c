/**
 * @file demo_visualize.c
 * @brief Demo: Print a DOT-format visualization of a configuration graph.
 *
 * Generates output that can be piped to Graphviz:
 *   make demos && ./build/demo_demo_visualize > graph.dot
 *   dot -Tpng graph.dot -o graph.png
 */

#include <stdio.h>
#include <string.h>
#include "flp_common.h"
#include "flp_config.h"
#include "flp_protocol.h"
#include "flp_message.h"
#include "flp_search.h"

int main(void)
{
    printf("/* FLP Configuration Graph — DOT format for Graphviz */\n\n");

    flp_protocol_desc desc;
    flp_protocol_desc_init(&desc, FLP_PROTO_FLOOD_SET, 3, 1);

    flp_configuration cfg;
    int32_t inputs[3] = {0, 1, 0};
    flp_config_init_with_inputs(&cfg, 3, inputs, 0);

    /* Initialize: first round of messages */
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
    flp_graph_init(&graph, 10000);
    int32_t nodes = flp_graph_build(&graph, &desc, &cfg, 2);

    printf("digraph FLPConfigGraph {\n");
    printf("  rankdir=LR;\n");
    printf("  node [shape=box, style=filled, fontname=\"Courier\"];\n");
    printf("  edge [fontname=\"Courier\", fontsize=10];\n\n");

    for (int32_t i = 0; i < nodes; i++) {
        const flp_graph_node *node = &graph.nodes[i];
        const char *color;
        switch (node->config.valence) {
            case FLP_VALENCE_0: color = "lightblue"; break;
            case FLP_VALENCE_1: color = "lightcoral"; break;
            case FLP_VALENCE_BIVALENT: color = "lightyellow"; break;
            default: color = "lightgray"; break;
        }

        /* Build a concise label */
        char label[256];
        int32_t off = 0;
        off += snprintf(label + off, sizeof(label) - (size_t)off,
                        "C%d\nd=%d\n",
                        node->config.config_id, node->config.depth);
        for (int32_t p = 0; p < node->config.num_processes && p < 4; p++) {
            off += snprintf(label + off, sizeof(label) - (size_t)off,
                            "P%d=%s ", p,
                            flp_decision_to_string(
                                node->config.processes[p].decision));
        }

        printf("  node%d [label=\"%s\", fillcolor=%s];\n",
               i, label, color);
    }

    /* Edges */
    printf("\n");
    for (int32_t i = 0; i < nodes; i++) {
        if (graph.adj_count[i] > 0) {
            int32_t adj_start = graph.adj_start[i];
            for (int32_t j = 0; j < graph.adj_count[i]; j++) {
                int32_t to = graph.adjacency[adj_start + j];
                printf("  node%d -> node%d [label=\"P%d\"];\n",
                       i, to, graph.nodes[to].incoming_event.process);
            }
        }
    }

    printf("}\n");

    flp_graph_destroy(&graph);
    return 0;
}
