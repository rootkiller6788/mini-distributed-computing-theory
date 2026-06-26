#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "flp_common.h"
#include "flp_config.h"
#include "flp_protocol.h"
#include "flp_message.h"
#include "flp_consensus.h"
#include "flp_search.h"

int main(void) {
    printf("FLP Impossibility Proof - Test Suite\n");
    printf("====================================\n\n");

    /* Test 1: Common utilities */
    printf("Test 1: Common utilities...\n");
    flp_fault_model fm = flp_fault_model_init(1);
    assert(fm.max_crashes == 1);
    assert(!fm.byzantine);
    assert(strcmp(flp_valence_to_string(FLP_VALENCE_BIVALENT), "bivalent") == 0);
    assert(strcmp(flp_decision_to_string(FLP_UNDECIDED), "UNDECIDED") == 0);
    printf("  PASSED\n");

    /* Test 2: Configuration init */
    printf("Test 2: Configuration initialization...\n");
    flp_configuration cfg;
    int32_t inputs[] = {0, 1, 0};
    flp_config_init_with_inputs(&cfg, 3, inputs, 1);
    assert(cfg.num_processes == 3);
    assert(cfg.processes[0].input_value == 0);
    assert(cfg.processes[1].input_value == 1);
    assert(flp_config_is_initial(&cfg));
    assert(!flp_config_is_decided(&cfg));
    printf("  PASSED\n");

    /* Test 3: Configuration clone and equality */
    printf("Test 3: Configuration clone and equality...\n");
    flp_configuration cfg2;
    flp_config_clone(&cfg2, &cfg);
    assert(flp_config_equal(&cfg, &cfg2));
    cfg2.processes[0].input_value = 99;
    assert(!flp_config_equal(&cfg, &cfg2));
    printf("  PASSED\n");

    /* Test 4: Message buffer operations */
    printf("Test 4: Message buffer operations...\n");
    assert(flp_msg_buffer_is_empty(&cfg));
    flp_msg_send_to(&cfg, 0, 1, 1, 42, 0);
    assert(!flp_msg_buffer_is_empty(&cfg));
    assert(flp_msg_buffer_pending_count(&cfg, 1) == 1);
    assert(flp_msg_buffer_pending_count(&cfg, 0) == 0);
    flp_message msg;
    bool delivered = flp_msg_buffer_deliver(&cfg, 1, &msg);
    assert(delivered);
    assert(msg.value == 42);
    assert(flp_msg_buffer_is_empty(&cfg));
    printf("  PASSED\n");

    /* Test 5: Process correctness */
    printf("Test 5: Process correctness...\n");
    assert(flp_process_is_correct(&cfg.processes[0]));
    cfg.processes[0].status = FLP_PROCESS_CRASHED;
    assert(!flp_process_is_correct(&cfg.processes[0]));
    assert(flp_config_correct_count(&cfg) == 2);
    printf("  PASSED\n");

    /* Test 6: Protocol descriptor */
    printf("Test 6: Protocol descriptor...\n");
    flp_protocol_desc desc;
    flp_protocol_desc_init(&desc, FLP_PROTO_FLOOD_SET, 3, 1);
    assert(desc.type == FLP_PROTO_FLOOD_SET);
    assert(desc.num_processes == 3);
    assert(desc.fault_tolerance == 1);
    printf("  PASSED\n");

    /* Test 7: Schedule operations */
    printf("Test 7: Schedule operations...\n");
    flp_schedule sched;
    flp_schedule_init(&sched);
    flp_schedule_append(&sched, 0);
    flp_schedule_append(&sched, 1);
    flp_schedule_append(&sched, 2);
    assert(sched.length == 3);
    assert(flp_schedule_contains_process(&sched, 0));
    assert(!flp_schedule_contains_process(&sched, 3));
    printf("  PASSED\n");

    /* Test 8: Consensus agreement */
    printf("Test 8: Consensus agreement check...\n");
    assert(flp_consensus_agreement_holds(FLP_DECIDE_0, FLP_DECIDE_0));
    assert(flp_consensus_agreement_holds(FLP_DECIDE_0, FLP_UNDECIDED));
    assert(!flp_consensus_agreement_holds(FLP_DECIDE_0, FLP_DECIDE_1));
    printf("  PASSED\n");

    /* Test 9: Event disjointness (Lemma 1) */
    printf("Test 9: Lemma 1 - Commutativity...\n");
    flp_event e1, e2;
    flp_event_init(&e1, 0, 0, &msg);
    flp_event_init(&e2, 1, 1, &msg);
    assert(flp_events_are_disjoint(&e1, &e2));
    flp_event_init(&e1, 0, 0, &msg);
    flp_event_init(&e2, 0, 1, &msg);
    assert(!flp_events_are_disjoint(&e1, &e2));
    printf("  PASSED\n");

    /* Test 10: Configuration hashing */
    printf("Test 10: Configuration hashing...\n");
    flp_configuration cfg_a, cfg_b;
    int32_t in_a[] = {0, 0, 0};
    int32_t in_b[] = {1, 1, 1};
    flp_config_init_with_inputs(&cfg_a, 3, in_a, 10);
    flp_config_init_with_inputs(&cfg_b, 3, in_b, 11);
    assert(flp_config_hash(&cfg_a) != flp_config_hash(&cfg_b));
    printf("  PASSED\n");

    /* Test 11: Canonical configs */
    printf("Test 11: Canonical consensus configurations...\n");
    flp_configuration all0, all1, split;
    flp_consensus_all_zero_config(&all0, 3);
    flp_consensus_all_one_config(&all1, 3);
    flp_consensus_split_config(&split, 3);
    assert(all0.processes[0].input_value == 0);
    assert(all1.processes[0].input_value == 1);
    assert(split.processes[0].input_value == 0);
    assert(split.processes[1].input_value == 1);
    printf("  PASSED\n");

    /* Test 12: Message strategy */
    printf("Test 12: Message delivery strategy...\n");
    flp_msg_set_strategy(FLP_MSG_FIFO);
    assert(flp_msg_get_strategy() == FLP_MSG_FIFO);
    flp_msg_set_strategy(FLP_MSG_ADVERSARIAL);
    assert(flp_msg_get_strategy() == FLP_MSG_ADVERSARIAL);
    printf("  PASSED\n");

    /* Test 13: Fault model */
    printf("Test 13: Fault model variants...\n");
    flp_fault_model fm2 = flp_fault_model_init(2);
    assert(fm2.max_crashes == 2);
    printf("  PASSED\n");

    /* Test 14: Process state initialization */
    printf("Test 14: Process state init...\n");
    flp_process_state ps;
    flp_process_state_init(&ps, 5, 1);
    assert(ps.pid == 5);
    assert(ps.input_value == 1);
    assert(ps.status == FLP_PROCESS_CORRECT);
    assert(!ps.has_decided);
    printf("  PASSED\n");

    /* Test 15: Message buffer statistics */
    printf("Test 15: Message buffer statistics...\n");
    flp_configuration cfg_stat;
    flp_config_init(&cfg_stat, 3, 100);
    flp_msg_send_to(&cfg_stat, 0, 1, 1, 10, 0);
    flp_msg_send_to(&cfg_stat, 0, 2, 2, 20, 0);
    flp_msg_send_to(&cfg_stat, 1, 2, 1, 30, 0);
    assert(flp_msg_buffer_total_count(&cfg_stat) == 3);
    assert(flp_msg_buffer_count_by_type(&cfg_stat, 1) == 2);
    assert(flp_msg_buffer_channel_count(&cfg_stat) == 3);
    printf("  PASSED\n");

    printf("\n====================================\n");
    printf("All 15 tests PASSED.\n");
    printf("FLP module compiles and runs correctly.\n");
    return 0;
}
