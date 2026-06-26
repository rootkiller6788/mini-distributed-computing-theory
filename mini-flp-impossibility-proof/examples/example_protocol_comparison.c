/**
 * @file example_protocol_comparison.c
 * @brief Compare all 6 protocols under the FLP impossibility analysis.
 *
 * Runs the full FLP theorem analysis on every implemented protocol
 * and prints a comparison table showing which protocols have
 * bivalent initial configurations and are therefore subject to
 * the FLP impossibility result.
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
    printf("==================================================\n");
    printf("  FLP Protocol Comparison — All Protocols\n");
    printf("==================================================\n\n");

    int32_t n = 3;
    int32_t f = 1;
    int32_t max_depth = 5;

    printf("System: N=%d processes, f=%d crash fault\n", n, f);
    printf("Search depth: %d\n\n", max_depth);

    printf("%-20s %-10s %-10s %-10s %s\n",
           "Protocol", "BivalInit", "Impossible", "NonDecRun",
           "Why FLP Applies");
    printf("%-20s %-10s %-10s %-10s %s\n",
           "--------------------", "----------", "----------",
           "----------", "-----------------------------------");

    for (int32_t p = 0; p < FLP_PROTO_COUNT; p++) {
        flp_protocol_desc desc;
        flp_protocol_desc_init(&desc, (flp_protocol_type)p, n, f);

        flp_theorem_result result;
        flp_analyze_protocol(&desc, n, max_depth, &result);

        const char *reason;
        if (result.has_bivalent_init) {
            reason = "Cannot determine if process is slow or crashed";
        } else {
            reason = "Search insufficient or protocol may solve consensus";
        }

        printf("%-20s %-10s %-10s %-10d %s\n",
               flp_protocol_type_name((flp_protocol_type)p),
               result.has_bivalent_init ? "YES" : "no",
               result.impossible ? "YES" : "?",
               result.max_nondeciding,
               reason);
    }

    printf("\n--- Detailed analysis for Flood-Set ---\n\n");

    flp_protocol_desc desc;
    flp_protocol_desc_init(&desc, FLP_PROTO_FLOOD_SET, n, f);

    int32_t c0, c1, cb;
    flp_consensus_classify_initial_configs(&desc, n, max_depth,
                                            &c0, &c1, &cb);
    printf("Initial configuration classification (Flood-Set, N=3):\n");
    printf("  0-valent initial configs: %d\n", c0);
    printf("  1-valent initial configs: %d\n", c1);
    printf("  Bivalent initial configs: %d\n", cb);
    printf("  Total initial configs:    %d (2^3 = 8)\n\n", c0 + c1 + cb);

    if (cb > 0) {
        printf("  FLP Lemma 2 VERIFIED: At least one bivalent initial\n");
        printf("  configuration exists. The protocol cannot be both\n");
        printf("  correct and always-terminating.\n");
    }

    /* Test validity property */
    printf("\n--- Validity Check ---\n");
    bool valid = flp_protocol_check_validity(&desc, n, max_depth);
    printf("Validity holds: %s\n", valid ? "YES (all-0 -> 0, all-1 -> 1)" : "NO");

    printf("\n==================================================\n");
    printf("  Comparison complete.\n");
    printf("==================================================\n");

    return 0;
}
