/**
 * @file example_flp_demo.c
 * @brief End-to-end FLP impossibility demonstration.
 *
 * This example shows:
 *   1. Creation of a consensus protocol instance.
 *   2. Exploration of initial bivalent configurations.
 *   3. Construction of a non-deciding execution.
 *   4. Comparison with synchronous consensus.
 *
 * Build: make examples && ./build/example_example_flp_demo
 */

#include <stdio.h>
#include <string.h>
#include "flp_common.h"
#include "flp_config.h"
#include "flp_protocol.h"
#include "flp_message.h"
#include "flp_consensus.h"
#include "flp_search.h"

int main(void)
{
    printf("============================================\n");
    printf("  FLP Impossibility Proof — Live Demo\n");
    printf("============================================\n\n");

    /* --- Part 1: System Model --- */
    printf("--- Part 1: Asynchronous System Model ---\n");
    int32_t n = 3;  /* 3 processes */
    int32_t f = 1;  /* tolerate 1 crash */

    printf("System: N=%d processes, f=%d crash fault(s)\n", n, f);
    printf("Communication: reliable asynchronous message passing\n");
    printf("Fault model: fail-stop (crash)\n\n");

    /* --- Part 2: Create an initial configuration --- */
    printf("--- Part 2: Initial Configuration ---\n");
    flp_configuration cfg;
    int32_t inputs[] = {0, 1, 0};  /* Split vote: not unanimous */
    flp_config_init_with_inputs(&cfg, n, inputs, 0);

    printf("Inputs: P0=%d, P1=%d, P2=%d\n",
           cfg.processes[0].input_value,
           cfg.processes[1].input_value,
           cfg.processes[2].input_value);
    printf("Configuration is initial: %s\n",
           flp_config_is_initial(&cfg) ? "yes" : "no");
    printf("Message buffer empty: %s\n\n",
           flp_msg_buffer_is_empty(&cfg) ? "yes" : "no");

    /* --- Part 3: Test multiple protocols --- */
    printf("--- Part 3: Protocol Analysis ---\n");

    flp_protocol_type protocols[] = {
        FLP_PROTO_FLOOD_SET,
        FLP_PROTO_PHASE_KING,
        FLP_PROTO_ECHO,
        FLP_PROTO_ROTATING_LEADER,
        FLP_PROTO_MAJORITY_VOTE,
        FLP_PROTO_TWO_PHASE
    };

    for (int p = 0; p < 6; p++) {
        flp_protocol_desc desc;
        flp_protocol_desc_init(&desc, protocols[p], n, f);

        flp_theorem_result result;
        flp_analyze_protocol(&desc, n, 4, &result);

        printf("  %-20s: bivalent_init=%s  impossible=%s\n",
               flp_protocol_type_name(protocols[p]),
               result.has_bivalent_init ? "YES" : "NO",
               result.impossible ? "YES" : "?");
    }
    printf("\n");

    /* --- Part 4: Build configuration graph --- */
    printf("--- Part 4: Configuration Graph ---\n");

    flp_protocol_desc desc;
    flp_protocol_desc_init(&desc, FLP_PROTO_FLOOD_SET, n, f);

    flp_config_graph graph;
    flp_graph_init(&graph, 10000);

    /* Initialize the protocol by having each process take a first step */
    flp_configuration started;
    flp_config_clone(&started, &cfg);

    /* Simulate first round: each process sends initial messages */
    for (int32_t pid = 0; pid < n; pid++) {
        flp_message nullmsg;
        memset(&nullmsg, 0, sizeof(nullmsg));
        flp_step_result step_res;
        flp_protocol_step(&desc, &started.processes[pid],
                          &nullmsg, &step_res);
        started.processes[pid] = step_res.new_state;
        for (int32_t oi = 0; oi < step_res.num_outgoing; oi++) {
            flp_msg_buffer_send(&started, &step_res.outgoing[oi]);
        }
    }

    int32_t nodes = flp_graph_build(&graph, &desc, &started, 3);
    printf("Configurations explored: %d\n", nodes);
    flp_graph_print_summary(&graph);

    /* --- Part 5: Bivalence search --- */
    printf("\n--- Part 5: Bivalence Search ---\n");

    bool found_biv = flp_find_bivalent_initial(&desc, n, 4, NULL);
    printf("Bivalent initial config found: %s\n",
           found_biv ? "YES (Lemma 2 holds)" : "NO (search depth may be insufficient)");

    /* --- Part 6: Consensus consistency --- */
    printf("\n--- Part 6: Consensus Properties ---\n");

    flp_configuration all0, all1, split;
    flp_consensus_all_zero_config(&all0, n);
    flp_consensus_all_one_config(&all1, n);
    flp_consensus_split_config(&split, n);

    printf("All-0 config: %s (hash=%u)\n",
           flp_config_is_initial(&all0) ? "initial" : "non-initial",
           flp_config_hash(&all0));
    printf("All-1 config: %s (hash=%u)\n",
           flp_config_is_initial(&all1) ? "initial" : "non-initial",
           flp_config_hash(&all1));
    printf("Split config: %s (hash=%u)\n",
           flp_config_is_initial(&split) ? "initial" : "non-initial",
           flp_config_hash(&split));

    printf("\nConsensus agreement check:\n");
    printf("  agree(0,0) = %s (expect true)\n",
           flp_consensus_agreement_holds(FLP_DECIDE_0, FLP_DECIDE_0) ?
           "true" : "false");
    printf("  agree(0,1) = %s (expect false)\n",
           flp_consensus_agreement_holds(FLP_DECIDE_0, FLP_DECIDE_1) ?
           "true" : "false");
    printf("  agree(0,UNDECIDED) = %s (expect true)\n",
           flp_consensus_agreement_holds(FLP_DECIDE_0, FLP_UNDECIDED) ?
           "true" : "false");

    flp_graph_destroy(&graph);
    printf("\n============================================\n");
    printf("  Demo complete.\n");
    printf("  FLP Theorem: No deterministic protocol\n");
    printf("  can solve consensus in an asynchronous\n");
    printf("  system with even one crash fault.\n");
    printf("============================================\n");

    return 0;
}
