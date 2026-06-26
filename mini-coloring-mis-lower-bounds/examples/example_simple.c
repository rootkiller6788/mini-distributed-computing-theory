#include "graph.h"
#include "coloring.h"
#include "mis.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("=== Simple Demo ===\n\n");
    Graph* ring = graph_create_ring(5);
    graph_print_stats(ring);
    Coloring* col = coloring_greedy_sequential(ring, NULL);
    printf("Colors: ");
    for (int i=0; i<5; i++) printf("%d ", col->colors[i]);
    printf("\nProper: %s\n", coloring_is_proper(ring,col)?"yes":"no");
    coloring_free(col);
    MIS* m = mis_greedy_sequential(ring, NULL);
    printf("MIS: ");
    for (int i=0; i<5; i++) if(m->in_mis[i]) printf("%d ", i);
    printf("\nValid: %s\n", mis_verify(ring,m)?"yes":"no");
    mis_free(m);
    printf("log*(%d) = %d\n", ring->n, log_star(ring->n));
    graph_free(ring);
    printf("\n=== Demo complete ===\n");
    return 0;
}
