#include "graph.h"
#include "coloring.h"
#include "mis.h"
#include "local_model.h"
#include "lower_bounds.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

int main(void) {
    printf("Running core tests...\n");

    Graph* g = graph_create(5);
    assert(g != NULL);
    assert(g->n == 5);
    graph_free(g);
    printf("graph_create: PASSED\n");

    Graph* r = graph_create_ring(5);
    assert(r != NULL);
    assert(r->m == 5);
    assert(r->adjacency[0].degree == 2);
    graph_free(r);
    printf("graph_create_ring: PASSED\n");

    Graph* k4 = graph_create_complete(4);
    assert(k4->m == 6);
    graph_free(k4);
    printf("graph_create_complete: PASSED\n");

    Graph* bp = graph_create_bipartite(3, 4);
    assert(bp->m == 12);
    graph_free(bp);
    printf("graph_create_bipartite: PASSED\n");

    Graph* gr = graph_create_grid(3, 3);
    assert(gr->n == 9);
    graph_free(gr);
    printf("graph_create_grid: PASSED\n");

    Graph* ring = graph_create_ring(6);
    Coloring* c = coloring_greedy_sequential(ring, NULL);
    assert(c != NULL);
    assert(coloring_is_proper(ring, c));
    assert(coloring_num_colors_used(c) <= 3);
    coloring_free(c);
    graph_free(ring);
    printf("coloring_greedy: PASSED\n");

    Graph* ring2 = graph_create_ring(8);
    MIS* mis = mis_greedy_sequential(ring2, NULL);
    assert(mis != NULL);
    assert(mis_verify(ring2, mis));
    mis_free(mis);
    graph_free(ring2);
    printf("mis_greedy: PASSED\n");

    Graph* c6 = graph_create_ring(6);
    assert(graph_diameter(c6) == 3);
    graph_free(c6);
    printf("graph_diameter: PASSED\n");

    Graph* c5 = graph_create_ring(5);
    assert(!graph_is_bipartite(c5));
    graph_free(c5);
    printf("graph_is_bipartite(C5): PASSED\n");

    assert(log_star(3) == 1);
    assert(log_star(5) == 2);
    assert(log_star(16) == 3);
    assert(log_star(65536) == 4);
    printf("log_star: PASSED\n");

    Graph* rng = graph_create_ring(6);
    DistanceKView* v = neighborhood_view(rng, 0, 1);
    assert(v != NULL);
    assert(v->size >= 3);
    neighborhood_view_free(v);
    graph_free(rng);
    printf("neighborhood_view: PASSED\n");

    assert(symmetry_break_by_id(3, 5, true));
    assert(!symmetry_break_by_id(5, 3, true));
    printf("symmetry_break: PASSED\n");

    Graph* rr = graph_create_random_regular(20, 4, 42);
    assert(rr != NULL);
    assert(rr->max_degree <= 4);
    graph_free(rr);
    printf("random_regular: PASSED\n");

    Graph* rng2 = graph_create_ring(10);
    Coloring* rc = randomized_delta_plus_one_coloring(rng2, 123);
    assert(rc != NULL);
    assert(coloring_is_proper(rng2, rc));
    coloring_free(rc);
    graph_free(rng2);
    printf("randomized_coloring: PASSED\n");

    Graph* grid = graph_create_grid(3, 3);
    Coloring* pc = planar_6coloring(grid);
    assert(pc != NULL);
    assert(coloring_is_proper(grid, pc));
    assert(coloring_num_colors_used(pc) <= 6);
    coloring_free(pc);
    graph_free(grid);
    printf("planar_6coloring: PASSED\n");

    printf("\nAll tests passed!\n");
    return 0;
}
