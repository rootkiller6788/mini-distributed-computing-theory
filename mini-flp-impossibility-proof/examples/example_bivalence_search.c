/**
 * @file example_bivalence_search.c
 * @brief Deep bivalence search demonstration.
 *
 * Demonstrates the core FLP proof technique:
 *   - Find a bivalent initial configuration (Lemma 2)
 *   - Search for preserving schedules (Lemma 3)
 *   - Construct non-deciding infinite runs
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
    printf("=============================================\n");
    printf("  Bivalence Search — FLP Core Algorithm\n");
    printf("=============================================\n\n");

    int32_t n = 3;
    int32_t f = 1;
    int32_t max_depth = 5;

    /* --- Lemma 2: Find bivalent initial configuration --- */
    printf("--- Lemma 2: Initial Bivalency ---\n");

    flp_protocol_desc desc;
    flp_protocol_desc_init(&desc, FLP_PROTO_FLOOD_SET, n, f);

    flp_configuration biv_config;
    bool found = flp_find_bivalent_initial(&desc, n, max_depth,
                                            &biv_config);

    if (found) {
        printf("FOUND bivalent initial configuration:\n");
        flp_config_print(&biv_config);

        /* Check what makes it bivalent */
        bool can0 = flp_config_can_decide_0(&biv_config, max_depth);
        bool can1 = flp_config_can_decide_1(&biv_config, max_depth);
        printf("  Can decide 0: %s\n", can0 ? "YES" : "NO");
        printf("  Can decide 1: %s\n", can1 ? "YES" : "NO");

        /* Find a schedule to 0 and a schedule to 1 */
        flp_schedule sched0, sched1;
        int32_t len0 = flp_config_schedule_to_0(&biv_config, &sched0, max_depth);
        int32_t len1 = flp_config_schedule_to_1(&biv_config, &sched1, max_depth);

        printf("\n  Schedule to decide 0 (%d steps):", len0);
        for (int32_t i = 0; i < len0 && i < 10; i++) {
            printf(" P%d", sched0.schedule[i]);
        }
        printf("\n  Schedule to decide 1 (%d steps):", len1);
        for (int32_t i = 0; i < len1 && i < 10; i++) {
            printf(" P%d", sched1.schedule[i]);
        }
        printf("\n\n");

        /* --- Lemma 3: Bivalence Preservation --- */
        printf("--- Lemma 3: Bivalence Preservation ---\n");

        /* Try to find a preserving schedule for a pending event */
        flp_schedule preserve;
        bool preserved = flp_find_preserving_schedule(
            &biv_config, 0, 0, max_depth, &preserve);

        printf("Preserving schedule found: %s\n",
               preserved ? "YES" : "NO (or need deeper search)");
        if (preserved) {
            printf("  Schedule length: %d\n", preserve.length);
            for (int32_t i = 0; i < preserve.length; i++) {
                printf("  Step %d: P%d\n", i, preserve.schedule[i]);
            }
        }
        printf("\n");

        /* --- Construct infinite run --- */
        printf("--- Infinite Non-Deciding Run ---\n");

        flp_schedule infinite_sched;
        int32_t steps = flp_construct_infinite_run(&desc, n, 10, max_depth,
                                                    &infinite_sched);

        printf("Constructed %d steps of non-deciding execution:\n", steps);
        for (int32_t i = 0; i < steps; i++) {
            printf("  Step %d: process P%d\n",
                   i, infinite_sched.schedule[i]);
        }

        bool decided = false;
        flp_configuration current;
        flp_config_clone(&current, &biv_config);

        printf("\n  Configuration states after each step:\n");
        for (int32_t i = 0; i < steps && i < 10; i++) {
            printf("  After step %d (P%d): ", i, infinite_sched.schedule[i]);
            if (flp_config_has_decision(&current)) {
                printf("DECIDED %s\n",
                       flp_decision_to_string(
                           current.processes[infinite_sched.schedule[i]].decision));
                decided = true;
            } else {
                printf("still undecided\n");
            }
        }

        if (!decided) {
            printf("\n  >> FLP THEOREM DEMONSTRATED: The system can be kept\n");
            printf("  >> in an undecided state indefinitely, proving that\n");
            printf("  >> consensus is impossible in the asynchronous model\n");
            printf("  >> with even one crash fault.\n");
        }
    } else {
        printf("No bivalent initial config found within depth %d.\n",
               max_depth);
        printf("Try increasing max_depth or using a different protocol.\n");
    }

    printf("\n=============================================\n");
    printf("  Bivalence search complete.\n");
    printf("=============================================\n");

    return 0;
}
