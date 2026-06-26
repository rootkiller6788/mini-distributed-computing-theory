#include "graph.h"
#include "lower_bounds.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("=== Lower Bound Examples ===\n\n");
    printf("--- log* values ---\n");
    int vals[] = {1,2,3,5,10,16,100,65536};
    for (int i=0; i<8; i++)
        printf("  log*(%d) = %d\n", vals[i], log_star(vals[i]));
    printf("\n--- Linial Lower Bound ---\n");
    printf("3-coloring an n-cycle requires Omega(log* n) rounds.\n");
    for (int t=1; t<=5; t++) {
        int max_n = 1 << (2*(t+2));
        printf("  T=%d: n up to %d can be 3-colored\n", t, max_n);
    }
    printf("\n--- KMW Lower Bound for MIS ---\n");
    printf("MIS: Omega(min{log Delta, sqrt(log n)}) rounds.\n");
    Graph* ring10 = graph_create_ring(10);
    printf("C10: n=%d, Delta=%d, diam=%d\n",
           ring10->n, ring10->max_degree, graph_diameter(ring10));
    int d = count_distinct_k_views(ring10, 2);
    printf("  Distinct 2-views: %d\n", d);
    graph_free(ring10);
    Graph* grid = graph_create_grid(3,3);
    printf("3x3 Grid: n=%d, m=%d, Delta=%d\n",
           grid->n, grid->m, grid->max_degree);
    graph_free(grid);
    printf("\n=== Complete ===\n");
    return 0;
}
