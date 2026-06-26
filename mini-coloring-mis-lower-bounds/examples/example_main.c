#include "graph.h"
#include "coloring.h"
#include "mis.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("=== Distributed Coloring & MIS Examples ===\n\n");

    printf("--- Example 1: Ring 3-Coloring (Cole-Vishkin) ---\n");
    int n = 16;
    int ids[] = {10,25,3,17,42,8,55,31,19,7,63,12,28,5,47,33};
    Coloring* c = ring_3coloring_cole_vishkin(n, ids);
    if (c) {
        printf("Ring of %d nodes:\n", n);
        for (int i = 0; i < n; i++)
            printf("  Node %2d: color %d\n", i, c->colors[i]);
        Graph* ring = graph_create_ring(n);
        printf("  Proper: %s\n", coloring_is_proper(ring, c) ? "YES" : "NO");
        graph_free(ring);
        printf("  Colors used: %d\n", coloring_num_colors_used(c));
        coloring_free(c);
    }

    printf("\n--- Example 2: Greedy K5 ---\n");
    Graph* k5 = graph_create_complete(5);
    Coloring* c2 = coloring_greedy_sequential(k5, NULL);
    if (c2) {
        for (int i = 0; i < 5; i++)
            printf("  V%d: color %d\n", i, c2->colors[i]);
        printf("  Proper: %s\n", coloring_is_proper(k5, c2) ? "YES" : "NO");
        coloring_free(c2);
    }
    graph_free(k5);

    printf("\n--- Example 3: MIS on C8 ---\n");
    Graph* ring8 = graph_create_ring(8);
    MIS* mis = mis_greedy_sequential(ring8, NULL);
    if (mis) {
        printf("  MIS: ");
        for (int i = 0; i < 8; i++)
            if (mis->in_mis[i]) printf("%d ", i);
        printf("\n  Size: %d\n", mis->mis_size);
        printf("  Valid: %s\n", mis_verify(ring8, mis) ? "YES" : "NO");
        mis_free(mis);
    }
    graph_free(ring8);

    printf("\n--- Example 4: Luby MIS on C10 ---\n");
    Graph* ring10 = graph_create_ring(10);
    MIS* mis2 = mis_luby_randomized(ring10, 42);
    if (mis2) {
        printf("  MIS: ");
        for (int i = 0; i < 10; i++)
            if (mis2->in_mis[i]) printf("%d ", i);
        printf("\n  Valid: %s\n", mis_verify(ring10, mis2) ? "YES" : "NO");
        mis_free(mis2);
    }
    graph_free(ring10);

    printf("\n--- Example 5: Randomized Coloring ---\n");
    Graph* rg = graph_create_random_regular(12, 4, 12345);
    graph_print_stats(rg);
    Coloring* rc = randomized_delta_plus_one_coloring(rg, 999);
    if (rc) {
        printf("  Colors used: %d\n", coloring_num_colors_used(rc));
        printf("  Proper: %s\n", coloring_is_proper(rg, rc) ? "YES" : "NO");
        coloring_free(rc);
    }
    graph_free(rg);

    printf("\n--- Example 6: Graph Analysis ---\n");
    Graph* grid = graph_create_grid(4, 4);
    graph_print_stats(grid);
    printf("  Diameter: %d\n", graph_diameter(grid));
    printf("  Bipartite: %s\n", graph_is_bipartite(grid) ? "yes" : "no");
    printf("  Girth: %d\n", graph_girth(grid));
    printf("  Avg clustering: %.3f\n", graph_avg_clustering(grid));
    printf("  log*(%d) = %d\n", grid->n, log_star(grid->n));
    graph_free(grid);

    printf("\n--- Example 7: Round Complexity ---\n");
    int cnt;
    RoundComplexity* tbl = round_complexity_table(&cnt);
    for (int i = 0; i < cnt; i++)
        printf("  %s (tight=%s)\n", tbl[i].problem_name,
               tbl[i].tight ? "yes" : "no");

    printf("\n=== All examples complete ===\n");
    return 0;
}
