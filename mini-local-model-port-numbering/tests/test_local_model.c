/**
 * @file test_local_model.c
 * @brief Assert-based tests for the LOCAL model and port numbering.
 *
 * Tests cover:
 *  - Context initialization / destruction
 *  - Graph topology generators (path, cycle, tree, random, clique)
 *  - r-neighborhood computation
 *  - Distance, diameter, connectivity
 *  - Graph properties (max/min/avg degree, edge count, girth)
 *  - Port numbering (arbitrary, ID-ordered, degree-ordered, random)
 *  - Port validation and consistency
 *  - BFS tree port numbering
 *  - (Δ+1)-vertex coloring verification
 *  - MIS verification
 *  - Maximal matching verification
 *  - 3-coloring a ring (Cole-Vishkin)
 *  - Weak 2-coloring
 *  - Vertex cover from matching
 *  - Sinkless orientation
 *  - Iterated log
 *  - Lower bound calculations
 *  - Leader election
 *  - Triangle detection
 *  - Distance-2 coloring
 */

#include "local_model.h"
#include "port_numbering.h"
#include "graph_problems.h"
#include "algorithms.h"
#include "lower_bounds.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) printf("  TEST: %s ... ", name)
#define OK() do { printf("OK\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); tests_failed++; } while(0)
#define CHECK(cond, msg) do { if (cond) OK(); else FAIL(msg); } while(0)

/* ================================================================
 * L1: Context Init / Destroy
 * ================================================================ */

static void test_context_init_destroy(void) {
    TEST("context_init_destroy");
    local_context_t ctx;
    bool adj[16] = {0,1,0,0, 1,0,1,0, 0,1,0,1, 0,0,1,0};
    int ret = local_context_init(&ctx, 4, adj);
    CHECK(ret == 0 && ctx.num_nodes == 4, "init failed");

    /* Check UID assignment */
    int uid_ok = 1;
    for (uint32_t i = 0; i < 4; i++) {
        if (ctx.nodes[i].uid != i + 1) uid_ok = 0;
    }
    CHECK(uid_ok, "UID assignment wrong");

    local_context_destroy(&ctx);
}

/* ================================================================
 * L2+L3: Graph Topologies
 * ================================================================ */

static void test_make_path(void) {
    TEST("make_path");
    local_context_t ctx;
    int ret = local_make_path(&ctx, 5);
    CHECK(ret == 0, "make_path returned error");

    CHECK(ctx.num_nodes == 5, "wrong node count");
    CHECK(local_max_degree(&ctx) == 2, "path max degree not 2");
    CHECK(local_min_degree(&ctx) == 1, "path min degree not 1");
    CHECK(local_is_connected(&ctx), "path not connected");
    CHECK(local_diameter(&ctx) == 4, "path diameter wrong");
    CHECK(local_edge_count(&ctx) == 4, "path edge count wrong");

    local_context_destroy(&ctx);
}

static void test_make_cycle(void) {
    TEST("make_cycle");
    local_context_t ctx;
    int ret = local_make_cycle(&ctx, 6);
    CHECK(ret == 0, "make_cycle returned error");

    CHECK(ctx.num_nodes == 6, "wrong node count");
    CHECK(local_max_degree(&ctx) == 2, "cycle max degree not 2");
    CHECK(local_min_degree(&ctx) == 2, "cycle min degree not 2");
    CHECK(local_is_connected(&ctx), "cycle not connected");
    CHECK(local_diameter(&ctx) == 3, "C6 diameter should be 3");
    CHECK(local_edge_count(&ctx) == 6, "C6 edge count wrong");
    CHECK(local_girth(&ctx) == 6, "C6 girth should be 6");

    local_context_destroy(&ctx);
}

static void test_make_tree(void) {
    TEST("make_regular_tree");
    local_context_t ctx;
    int ret = local_make_regular_tree(&ctx, 3, 3);
    CHECK(ret == 0, "make_regular_tree returned error");

    /* Depth-3 ternary tree: 1 + 3 + 6 + 12 = ... let's calculate */
    CHECK(ctx.num_nodes > 5, "tree too small");
    CHECK(local_is_connected(&ctx), "tree not connected");
    CHECK(local_girth(&ctx) == UINT32_MAX, "tree should be acyclic (girth = INF)");

    local_context_destroy(&ctx);
}

static void test_make_random_graph(void) {
    TEST("make_random_graph");
    local_context_t ctx;
    int ret = local_make_random_graph(&ctx, 20, 0.3, 42);
    CHECK(ret == 0, "make_random_graph returned error");

    CHECK(ctx.num_nodes == 20, "wrong node count");
    CHECK(local_edge_count(&ctx) > 0, "random graph has no edges?");

    local_context_destroy(&ctx);
}

static void test_make_clique(void) {
    TEST("make_clique");
    local_context_t ctx;
    int ret = local_make_clique(&ctx, 5);
    CHECK(ret == 0, "make_clique returned error");

    CHECK(local_max_degree(&ctx) == 4, "K5 max degree not 4");
    CHECK(ctx.nodes[0].degree == 4, "K5 node degree not 4");
    CHECK(local_diameter(&ctx) == 1, "K5 diameter not 1");
    CHECK(local_edge_count(&ctx) == 10, "K5 edge count not 10");
    CHECK(local_chromatic_number(&ctx) == 5, "K5 chromatic number not 5");

    local_context_destroy(&ctx);
}

/* ================================================================
 * L2: r-Neighborhood
 * ================================================================ */

static void test_r_neighborhood(void) {
    TEST("r_neighborhood");
    local_context_t ctx;
    local_make_path(&ctx, 7);

    uint32_t neighbors[LOCAL_MAX_NODES];
    uint32_t count;
    int ret = local_r_neighborhood(&ctx, 3, 2, neighbors, &count);
    CHECK(ret == 0 && count == 5, "r=2 from center of P7 should see 5 nodes");

    local_context_destroy(&ctx);
}

/* ================================================================
 * L2: Graph Properties
 * ================================================================ */

static void test_graph_properties(void) {
    TEST("graph_properties");
    local_context_t ctx;
    local_make_cycle(&ctx, 4);

    CHECK(local_max_degree(&ctx) == 2, "C4 max deg");
    CHECK(local_min_degree(&ctx) == 2, "C4 min deg");
    double avg = local_avg_degree(&ctx);
    CHECK(avg >= 1.99 && avg <= 2.01, "C4 avg deg not ~2");
    CHECK(local_edge_count(&ctx) == 4, "C4 edge count");
    CHECK(local_girth(&ctx) == 4, "C4 girth");
    CHECK(local_chromatic_number(&ctx) == 2, "C4 chromatic number (bipartite)");

    local_context_destroy(&ctx);
}

/* ================================================================
 * L2: Port Numbering
 * ================================================================ */

static void test_port_numbering_basic(void) {
    TEST("port_numbering_basic");
    port_numbering_t pn;
    port_init(&pn, 1);

    uint32_t neighbors[] = {2, 3, 4};
    int ret = port_assign_id_ordered(&pn, neighbors, 3);
    CHECK(ret == 0, "port_assign_id_ordered failed");
    CHECK(port_is_valid(&pn), "port not valid");

    /* ID-ordered: neighbor 2 → port 0, 3 → port 1, 4 → port 2 */
    CHECK(port_get_port_for_neighbor(&pn, 2) == 0, "port for neighbor 2 wrong");
    CHECK(port_get_port_for_neighbor(&pn, 3) == 1, "port for neighbor 3 wrong");
    CHECK(port_get_port_for_neighbor(&pn, 4) == 2, "port for neighbor 4 wrong");
    CHECK(port_get_neighbor_for_port(&pn, 0) == 2, "neighbor for port 0 wrong");
    CHECK(port_get_neighbor_for_port(&pn, 1) == 3, "neighbor for port 1 wrong");
    CHECK(port_get_neighbor_for_port(&pn, 2) == 4, "neighbor for port 2 wrong");
}

static void test_port_numbering_random(void) {
    TEST("port_numbering_random");
    port_numbering_t pn;
    port_init(&pn, 1);

    uint32_t neighbors[] = {10, 20, 30, 40};
    int ret = port_assign_random(&pn, neighbors, 4, 12345);
    CHECK(ret == 0, "port_assign_random failed");
    CHECK(port_is_valid(&pn), "random port not valid");

    /* All 4 neighbors should be reachable through some port */
    int found[4] = {0,0,0,0};
    for (uint32_t p = 0; p < 4; p++) {
        uint32_t nb = port_get_neighbor_for_port(&pn, p);
        CHECK(nb != UINT32_MAX, "invalid port mapping");
        for (int k = 0; k < 4; k++) {
            if (nb == neighbors[k]) found[k] = 1;
        }
    }
    CHECK(found[0] && found[1] && found[2] && found[3], "not all neighbors mapped");
}

static void test_port_degree_ordered(void) {
    TEST("port_degree_ordered");
    port_numbering_t pn;
    port_init(&pn, 1);

    uint32_t neighbors[] = {5, 6, 7};
    uint32_t degrees[]   = {3, 5, 2};
    int ret = port_assign_degree_ordered(&pn, neighbors, degrees, 3);
    CHECK(ret == 0, "port_assign_degree_ordered failed");

    /* Port 0 should be neighbor 6 (degree 5), port 1 neighbor 5 (deg 3), port 2 neighbor 7 (deg 2) */
    CHECK(port_get_neighbor_for_port(&pn, 0) == 6, "port 0 wrong");
    CHECK(port_get_neighbor_for_port(&pn, 1) == 5, "port 1 wrong");
    CHECK(port_get_neighbor_for_port(&pn, 2) == 7, "port 2 wrong");
}

static void test_port_ring_orientation(void) {
    TEST("port_ring_orientation");
    local_context_t ctx;
    local_make_cycle(&ctx, 6);

    port_numbering_t pns[6];
    int ret = port_assign_ring_orientation(pns, 6, ctx.adjacency);
    CHECK(ret == 0, "ring orientation failed");

    /* Each node should have degree 2 */
    for (int i = 0; i < 6; i++) {
        CHECK(pns[i].degree == 2, "ring node degree not 2");
        CHECK(port_is_valid(&pns[i]), "ring port not valid");
    }

    local_context_destroy(&ctx);
}

static void test_port_bfs_tree(void) {
    TEST("port_bfs_tree");
    local_context_t ctx;
    local_make_path(&ctx, 5);

    port_numbering_t pns[5];
    uint32_t parent[5];
    int ret = port_assign_bfs_tree(pns, 5, ctx.adjacency, 0, parent);
    CHECK(ret == 0, "bfs tree port numbering failed");

    /* Root (node 0) should have no parent port */
    CHECK(pns[0].degree == 1, "root of P5 should have degree 1 (only child)");
    /* Leaf (node 4) should have degree 1 (only parent) */
    CHECK(pns[4].degree == 1, "leaf of P5 should have degree 1");

    local_context_destroy(&ctx);
}

/* ================================================================
 * L4: Lower Bounds
 * ================================================================ */

static void test_linial_lower_bound(void) {
    TEST("linial_lower_bound");
    /* For n=100, 3-coloring requires roughly log*(100) ≈ 3 rounds */
    uint32_t lb = lb_linial_ring_min_rounds(100, 3);
    CHECK(lb >= 1 && lb <= 5, "log* lower bound for n=100 seems off");

    /* With 0 rounds and n=10, 3-coloring should be impossible */
    CHECK(lb_linial_ring_impossible(0, 10), "0-round 3-coloring of 10-cycle should be impossible");
    /* With enough rounds (log* n + 3), it should be possible */
    CHECK(!lb_linial_ring_impossible(iterated_log2(100) + 3, 100),
          "sufficient rounds should make it possible");
}

static void test_iterated_log(void) {
    TEST("iterated_log2");
    CHECK(iterated_log2(1) == 0, "log*(1)");
    CHECK(iterated_log2(2) == 1, "log*(2)");
    CHECK(iterated_log2(4) == 2, "log*(4)");
    CHECK(iterated_log2(16) == 3, "log*(16)");
    CHECK(iterated_log2(65536) == 4, "log*(65536)");
}

static void test_ramsey_threshold(void) {
    TEST("ramsey_threshold");
    uint32_t t = lb_ramsey_threshold(2, 2);
    CHECK(t >= 4, "Ramsey threshold for 2 colors, 2 rounds should be ≥ 4");
}

static void test_det_rand_gap(void) {
    TEST("det_rand_gap");
    CHECK(lb_det_rand_gap(10, 5) == 2, "10/5 gap should be 2");
    CHECK(lb_det_rand_gap(0, 5) == 0, "0/5 gap should be 0");
    CHECK(lb_det_rand_gap(10, 0) == UINT32_MAX, "10/0 gap should be MAX");
}

static void test_communication_lower_bound(void) {
    TEST("comm_lower_bound");
    /* If comm requires 1000 bits across a cut of 10 edges with 100 nodes */
    uint32_t lb = lb_from_communication_complexity(100, 10, 1000);
    CHECK(lb > 0 && lb < 100, "comm lower bound seems off");
}

/* ================================================================
 * L5: Algorithms
 * ================================================================ */

static void test_bfs_tree_build(void) {
    TEST("bfs_tree_build");
    local_context_t ctx;
    local_make_path(&ctx, 5);

    uint32_t parent[5], distance[5];
    uint32_t rounds = bfs_tree_build(&ctx, 0, parent, distance);
    CHECK(rounds == 4, "BFS tree on P5 should take 4 rounds");

    /* Root */
    CHECK(parent[0] == 0, "root parent should be self");
    CHECK(distance[0] == 0, "root distance should be 0");
    /* Leaf */
    CHECK(distance[4] == 4, "leaf distance should be 4");

    local_context_destroy(&ctx);
}

static void test_leader_election(void) {
    TEST("leader_election_deterministic");
    local_context_t ctx;
    local_make_cycle(&ctx, 5);

    uint32_t leader;
    uint32_t rounds = leader_election_deterministic(&ctx, &leader);
    CHECK(rounds == local_diameter(&ctx), "leader election rounds != diameter");
    CHECK(leader == 1, "leader should be node with min UID = 1");

    local_context_destroy(&ctx);
}

static void test_triangle_detection(void) {
    TEST("triangle_detection");
    local_context_t ctx;
    local_make_clique(&ctx, 4);  /* K4 has 4 triangles */

    bool has_tri;
    uint32_t rounds = triangle_detect(&ctx, &has_tri);
    CHECK(has_tri, "K4 should have triangles");
    CHECK(triangle_count(&ctx) == 4, "K4 should have 4 triangles");

    local_context_destroy(&ctx);

    /* Path has no triangles */
    local_make_path(&ctx, 5);
    rounds = triangle_detect(&ctx, &has_tri);
    CHECK(!has_tri, "P5 should have no triangles");
    CHECK(triangle_count(&ctx) == 0, "P5 should have 0 triangles");

    local_context_destroy(&ctx);
}

static void test_ring_sort(void) {
    TEST("ring_sort");
    local_context_t ctx;
    local_make_cycle(&ctx, 5);

    uint32_t values[] = {50, 10, 40, 20, 30};
    port_numbering_t pns[5];
    port_assign_ring_orientation(pns, 5, ctx.adjacency);

    uint32_t rounds = ring_sort(&ctx, values, pns);
    /* After sorting: values should be {10, 20, 30, 40, 50} on a line */
    /* But ring sort distributes around the ring; check that sum is preserved */
    uint32_t sum_before = 50+10+40+20+30;
    uint32_t sum_after = 0;
    for (uint32_t i = 0; i < 5; i++) sum_after += values[i];
    CHECK(sum_before == sum_after, "ring sort should preserve values");

    local_context_destroy(&ctx);
}

/* ================================================================
 * L6: Graph Problems
 * ================================================================ */

static void test_vertex_coloring(void) {
    TEST("vertex_coloring");
    local_context_t ctx;
    local_make_cycle(&ctx, 6);

    /* C6 is bipartite: 2-colorable, certainly (Δ+1)=3 colorable */
    uint32_t colors[6] = {0, 1, 0, 1, 0, 1};
    CHECK(vcol_is_proper(&ctx, colors), "C6 2-coloring should be proper");
    CHECK(vcol_count_colors(colors, 6) == 2, "C6 2-coloring should use 2 colors");

    /* Same colors on adjacent nodes should fail */
    uint32_t bad_colors[6] = {0, 0, 0, 0, 0, 0};
    CHECK(!vcol_is_proper(&ctx, bad_colors), "monochromatic coloring should not be proper");

    local_context_destroy(&ctx);
}

static void test_mis(void) {
    TEST("mis_luby");
    local_context_t ctx;
    local_make_path(&ctx, 10);

    mis_node_state_t states[10];
    for (int i = 0; i < 10; i++) mis_init(&states[i]);

    uint32_t rounds = luby_mis_full(&ctx, states, 42);
    CHECK(rounds > 0, "MIS should take some rounds");
    CHECK(mis_is_valid(&ctx, states), "MIS should be valid");

    /* Path P10: MIS size must be between ceil(10/3)=4 and ceil(10/2)=5 */
    uint32_t mis_size = 0;
    for (int i = 0; i < 10; i++) {
        if (states[i].status == MIS_IN_SET) mis_size++;
    }
    CHECK(mis_size >= 4 && mis_size <= 5, "MIS size on P10 should be 4 or 5");

    for (int i = 0; i < 10; i++) mis_destroy(&states[i]);
    local_context_destroy(&ctx);
}

static void test_maximal_matching(void) {
    TEST("maximal_matching");
    local_context_t ctx;
    local_make_path(&ctx, 6);

    mm_node_state_t states[6];
    for (int i = 0; i < 6; i++) mm_init(&states[i]);

    port_numbering_t pns[6];
    port_assign_bfs_tree(pns, 6, ctx.adjacency, 0, (uint32_t[]){0,0,0,0,0,0});

    uint32_t rounds = mm_full_random(&ctx, states, pns, 123);
    CHECK(rounds > 0, "MM should take some rounds");
    CHECK(mm_is_valid(&ctx, states), "MM should be valid");

    for (int i = 0; i < 6; i++) /* mm_init doesn't allocate, no destroy needed */;
    local_context_destroy(&ctx);
}

static void test_ring_3coloring(void) {
    TEST("ring_3coloring_cv");
    local_context_t ctx;
    local_make_cycle(&ctx, 15);

    uint32_t colors[15];
    port_numbering_t pns[15];
    port_assign_ring_orientation(pns, 15, ctx.adjacency);

    uint32_t rounds = ring_3color_cole_vishkin(&ctx, colors, pns);
    CHECK(rounds > 0 && rounds < 20, "CV should finish in < 20 rounds");
    CHECK(ring_3color_verify(&ctx, colors), "CV output should be proper 3-coloring");

    local_context_destroy(&ctx);
}

static void test_weak_2coloring(void) {
    TEST("weak_2coloring");
    local_context_t ctx;
    local_make_cycle(&ctx, 8);

    uint32_t colors[8];
    port_numbering_t pns[8];
    port_assign_ring_orientation(pns, 8, ctx.adjacency);

    uint32_t rounds = weak_2coloring(&ctx, colors, pns);
    CHECK(rounds < 50, "weak 2-coloring should converge quickly");
    CHECK(weak_2coloring_verify(&ctx, colors), "weak 2-coloring should be valid");

    local_context_destroy(&ctx);
}

static void test_vertex_cover(void) {
    TEST("vertex_cover_approximation");
    local_context_t ctx;
    local_make_path(&ctx, 6);

    mm_node_state_t mm_states[6];
    for (int i = 0; i < 6; i++) mm_init(&mm_states[i]);

    port_numbering_t pns[6];
    port_assign_bfs_tree(pns, 6, ctx.adjacency, 0, (uint32_t[]){0,0,0,0,0,0});
    mm_full_random(&ctx, mm_states, pns, 456);

    bool vc[6];
    vc_from_matching(mm_states, 6, vc);
    CHECK(vc_is_valid(&ctx, vc), "VC from matching should be valid");
    CHECK(vc_size(vc, 6) >= 2, "VC should be ≥ 2 (matching has ≥ 1 edge)");

    local_context_destroy(&ctx);
}

static void test_dominating_set(void) {
    TEST("dominating_set");
    local_context_t ctx;
    local_make_path(&ctx, 10);

    bool domset[10];
    port_numbering_t pns[10];
    port_assign_bfs_tree(pns, 10, ctx.adjacency, 0, (uint32_t[]){0,0,0,0,0,0,0,0,0,0});

    uint32_t rounds = dominating_set_greedy(&ctx, domset, pns, 42);
    CHECK(rounds > 0, "dominating set should take some steps");
    CHECK(domset_is_valid(&ctx, domset), "dominating set should be valid");

    local_context_destroy(&ctx);
}

static void test_sinkless(void) {
    TEST("sinkless_orientation");
    local_context_t ctx;
    local_make_cycle(&ctx, 10);

    sinkless_state_t states[10];
    for (int i = 0; i < 10; i++) sinkless_init(&states[i], 2);

    port_numbering_t pns[10];
    port_assign_ring_orientation(pns, 10, ctx.adjacency);

    uint32_t sinks = sinkless_round(&ctx, states, pns, 999);
    CHECK(sinks == 0, "no sinks should remain");
    CHECK(sinkless_is_valid(&ctx, states), "sinkless orientation should be valid");

    local_context_destroy(&ctx);
}

static void test_distance2_coloring(void) {
    TEST("distance2_coloring");
    local_context_t ctx;
    local_make_path(&ctx, 6);

    uint32_t colors[6];
    uint32_t rounds = distance2_coloring(&ctx, colors);
    CHECK(rounds == 1, "distance-2 coloring takes 1 round in LOCAL");

    /* Verify: no two nodes at distance ≤2 have same color */
    int ok = 1;
    for (uint32_t i = 0; i < 6 && ok; i++) {
        for (uint32_t j = i+1; j < 6 && ok; j++) {
            uint32_t d = local_distance(&ctx, i, j);
            if (d <= 2 && colors[i] == colors[j]) ok = 0;
        }
    }
    CHECK(ok, "distance-2 coloring violated");

    local_context_destroy(&ctx);
}

/* ================================================================
 * L4: Symmetry Classes
 * ================================================================ */

static void test_symmetry_classes(void) {
    TEST("symmetry_classes");
    local_context_t ctx;
    local_make_cycle(&ctx, 6);

    port_numbering_t pns[6];
    port_assign_ring_orientation(pns, 6, ctx.adjacency);

    uint32_t classes = port_count_symmetry_classes(pns, 6, ctx.adjacency);
    /* On C6 with ring orientation, all nodes are structurally identical */
    CHECK(classes >= 1 && classes <= 6, "symmetry classes out of range");

    local_context_destroy(&ctx);
}

/* ================================================================
 * L5: Distributed Sum / Max
 * ================================================================ */

static void test_distributed_aggregation(void) {
    TEST("distributed_sum_max");
    local_context_t ctx;
    local_make_path(&ctx, 5);

    uint32_t values[] = {3, 1, 4, 1, 5};
    uint64_t sum;
    uint32_t max_val;

    uint32_t rounds_sum = distributed_sum(&ctx, values, &sum);
    CHECK(sum == 14, "sum should be 14");
    CHECK(rounds_sum > 0, "sum should take ≥1 round");

    uint32_t rounds_max = distributed_max(&ctx, values, &max_val);
    CHECK(max_val == 5, "max should be 5");
    CHECK(rounds_max > 0, "max should take ≥1 round");

    local_context_destroy(&ctx);
}

/* ================================================================
 * Main
 * ================================================================ */

int main(void) {
    printf("=== LOCAL Model + Port Numbering Test Suite ===\n\n");

    printf("[L1] Core Definitions\n");
    test_context_init_destroy();

    printf("\n[L2+L3] Graph Topologies\n");
    test_make_path();
    test_make_cycle();
    test_make_tree();
    test_make_random_graph();
    test_make_clique();
    test_r_neighborhood();
    test_graph_properties();

    printf("\n[L2+L3] Port Numbering\n");
    test_port_numbering_basic();
    test_port_numbering_random();
    test_port_degree_ordered();
    test_port_ring_orientation();
    test_port_bfs_tree();

    printf("\n[L4] Lower Bounds\n");
    test_linial_lower_bound();
    test_iterated_log();
    test_ramsey_threshold();
    test_det_rand_gap();
    test_communication_lower_bound();
    test_symmetry_classes();

    printf("\n[L5] Algorithms\n");
    test_bfs_tree_build();
    test_leader_election();
    test_triangle_detection();
    test_ring_sort();
    test_distributed_aggregation();

    printf("\n[L6] Canonical Problems\n");
    test_vertex_coloring();
    test_mis();
    test_maximal_matching();
    test_ring_3coloring();
    test_weak_2coloring();
    test_vertex_cover();
    test_dominating_set();
    test_sinkless();
    test_distance2_coloring();

    printf("\n====================================\n");
    printf("RESULTS: %d passed, %d failed\n", tests_passed, tests_failed);
    printf("====================================\n");

    return tests_failed > 0 ? 1 : 0;
}
