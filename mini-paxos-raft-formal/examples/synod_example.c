/**
 * synod_example.c — Basic Paxos (Synod) End-to-End Example
 *
 * Demonstrates the complete Basic Paxos protocol:
 *   - A proposer initiates consensus on a value
 *   - Acceptors participate in the two-phase protocol
 *   - Learners detect when a value is chosen
 *
 * This example shows how Paxos achieves safety (agreement) even
 * with crash faults, as long as a majority of acceptors survive.
 *
 * L6 Canonical Problem: Consensus in crash-fault model
 *
 * References:
 *   - Lamport, "Paxos Made Simple" (2001)
 */

#include <stdio.h>
#include <string.h>
#include "../include/paxos_raft_types.h"
#include "../include/paxos_core.h"

int main(void) {
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║   Basic Paxos (Synod) Protocol Demo          ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");

    const int NUM_ACCEPTORS = 5;
    int majority = NUM_ACCEPTORS / 2 + 1;

    printf("Configuration: 1 proposer, %d acceptors, majority = %d\n",
           NUM_ACCEPTORS, majority);
    printf("Fault tolerance: up to %d crash failures\n\n",
           (NUM_ACCEPTORS - 1) / 2);

    /* ─── Proposer ─── */
    paxos_proposer_t proposer;
    paxos_proposer_init(&proposer, 0);

    /* ─── Acceptors ─── */
    paxos_acceptor_t acceptors[NUM_ACCEPTORS];
    node_id_t acceptor_ids[NUM_ACCEPTORS];
    for (int i = 0; i < NUM_ACCEPTORS; i++) {
        acceptors[i].node_id = (node_id_t)(i + 1);
        acceptors[i].promise_ballot = 0;
        acceptors[i].accepted_ballot = 0;
        acceptors[i].has_accepted = false;
        acceptor_ids[i] = (node_id_t)(i + 1);
    }

    /* ─── Learners ─── */
    paxos_learner_t learners[2];
    node_id_t learner_ids[2] = {10, 11};
    for (int i = 0; i < 2; i++) {
        learners[i].node_id = learner_ids[i];
        learners[i].has_learned = false;
    }

    /* ─── Proposed Value ─── */
    consensus_value_t proposed_value;
    memcpy(proposed_value.data, "update x=42", 12);
    proposed_value.length = 12;

    printf("Step 1: Proposer initiates ballot 100\n");
    ballot_t ballot = 100;
    paxos_message_t phase1_msgs[NUM_ACCEPTORS];

    int n = paxos_prepare(&proposer, ballot, NUM_ACCEPTORS,
                           acceptor_ids, phase1_msgs);
    printf("  Sent %d Prepare(100) messages\n", n);

    /* ─── Phase 1b: Acceptors respond ─── */
    printf("\nStep 2: Acceptors process Prepare(100)\n");
    paxos_message_t promises[NUM_ACCEPTORS];
    int num_promises = 0;

    for (int i = 0; i < n; i++) {
        paxos_message_t response;
        if (paxos_receive_prepare(&acceptors[i], &phase1_msgs[i], &response)) {
            promises[num_promises++] = response;
        }
    }
    printf("  Received %d Promise responses (need %d for majority)\n",
           num_promises, majority);

    if (num_promises < majority) {
        printf("  FAILED: Not enough promises for majority.\n");
        return 1;
    }

    /* ─── Phase 2a: Proposer sends Accept ─── */
    printf("\nStep 3: Proposer selects value and sends Accept(100, v)\n");

    n = paxos_accept(&proposer, num_promises, promises,
                      &proposed_value, NUM_ACCEPTORS,
                      acceptor_ids, phase1_msgs);
    printf("  Sent %d Accept(100, \"%s\") messages\n", n, proposed_value.data);

    /* ─── Phase 2b: Acceptors accept ─── */
    printf("\nStep 4: Acceptors process Accept(100, v)\n");
    int total_accepted = 0;

    for (int i = 0; i < n; i++) {
        paxos_message_t accepted_msgs[NUM_ACCEPTORS];
        int m = paxos_receive_accept(&acceptors[i], &phase1_msgs[i],
                                      2, learner_ids, accepted_msgs);
        if (m > 0) {
            total_accepted++;
            printf("  Acceptor %d accepted and notified %d learners\n",
                   i, m);
        }
    }
    printf("  %d acceptors accepted (need %d for chosen)\n",
           total_accepted, majority);

    if (total_accepted < majority) {
        printf("  FAILED: Not enough acceptances for value to be chosen.\n");
        return 1;
    }

    /* ─── Learners learn ─── */
    printf("\nStep 5: Learners detect chosen value\n");
    for (int i = 0; i < 2; i++) {
        printf("  Learner %d: ", i);
        for (int a = 0; a < NUM_ACCEPTORS; a++) {
            if (acceptors[a].has_accepted) {
                paxos_message_t ack;
                ack.value = acceptors[a].accepted_value;
                ack.ballot = acceptors[a].accepted_ballot;
                paxos_learner_receive(&learners[i], &ack, NUM_ACCEPTORS);
            }
        }
        if (learners[i].has_learned) {
            printf("learned value = \"%.*s\"\n",
                   learners[i].learned_value.length,
                   learners[i].learned_value.data);
        } else {
            printf("has not yet learned\n");
        }
    }

    /* ─── Safety Check ─── */
    printf("\nStep 6: Safety verification\n");
    if (paxos_verify_safety_invariant(acceptors, NUM_ACCEPTORS)) {
        printf("  ✓ Paxos safety invariant holds\n");
    } else {
        printf("  ✗ SAFETY VIOLATION DETECTED\n");
        return 1;
    }

    printf("\n╔══════════════════════════════════════════════╗\n");
    printf("║   Paxos consensus achieved successfully!     ║\n");
    printf("║   Value: \"%.*s\"                              ║\n",
           learners[0].learned_value.length,
           learners[0].learned_value.data);
    printf("╚══════════════════════════════════════════════╝\n");

    return 0;
}
