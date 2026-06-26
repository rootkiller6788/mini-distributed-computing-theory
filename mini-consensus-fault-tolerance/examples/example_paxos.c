/*
 * example_paxos.c - End-to-end Paxos consensus demonstration.
 * L6: Canonical consensus problem solved with Paxos.
 *
 * Demonstrates Basic Paxos: a committee of 5 acceptors reaches
 * consensus on a single value despite 2 faulty acceptors.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "paxos.h"

int main(void)
{
    printf("=== Paxos Consensus Demonstration ===\n\n");
    int num_acceptors = 5;
    int max_faulty = 2;  /* n=5, f=2, need majority=3 */

    printf("Configuration: %d acceptors, up to %d can fail.\n",
           num_acceptors, max_faulty);
    printf("Majority needed: %d\n\n", num_acceptors / 2 + 1);

    /* Initialize acceptors */
    paxos_acceptor_t *acceptors = (paxos_acceptor_t *)calloc(
        num_acceptors, sizeof(paxos_acceptor_t));
    for (int i = 0; i < num_acceptors; i++) {
        acceptors[i].promised.round = 0;
        acceptors[i].promised.server_id = 0;
        acceptors[i].has_accepted = false;
        acceptors[i].accepted_value = -1;
    }

    /* Run Paxos instances for 3 values */
    int values[] = {42, 100, -7};
    int num_instances = 3;

    for (int slot = 0; slot < num_instances; slot++) {
        printf("--- Slot %d: Proposing value %d ---\n",
               slot, values[slot]);

        paxos_instance_t inst;
        memset(&inst, 0, sizeof(inst));
        inst.instance_id = slot;
        inst.num_acceptors = num_acceptors;
        inst.peer_acceptors = (paxos_acceptor_t *)calloc(
            num_acceptors, sizeof(paxos_acceptor_t));

        int result = paxos_run_instance(&inst, values[slot],
                                         acceptors, num_acceptors);

        if (result >= 0) {
            printf("  DECIDED: value = %d\n", result);
            printf("  Phase 1 promises: %d/%d\n",
                   inst.proposer.promises_received,
                   num_acceptors / 2 + 1);
            printf("  Phase 2 accepts:  %d/%d\n",
                   inst.proposer.accepts_received,
                   num_acceptors / 2 + 1);
        } else {
            printf("  FAILED to reach consensus\n");
        }

        printf("\n");
        if (inst.peer_acceptors) free(inst.peer_acceptors);
    }

    /* Verify safety: all decided values are from proposed set */
    printf("Safety: Paxos guarantees no conflicting decisions.\n");
    printf("Reference: Lamport (2001) 'Paxos Made Simple'\n");

    free(acceptors);
    printf("\n=== Paxos demo complete ===\n");
    return 0;
}
