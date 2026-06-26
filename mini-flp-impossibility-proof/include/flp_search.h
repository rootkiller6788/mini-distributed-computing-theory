/**
 * @file flp_search.h
 * @brief Bivalence search: the computational heart of the FLP proof.
 *
 * The FLP impossibility proof constructs a non-terminating execution
 * by showing that from any bivalent configuration, one can always
 * reach another bivalent configuration indefinitely.
 *
 * Knowledge Coverage:
 *   L1: Bivalent/univalent configuration definitions
 *   L2: Configuration graph reachability
 *   L4: Lemma 1 (commutativity), Lemma 2 (initial bivalency),
 *       Lemma 3 (bivalence preservation)
 *   L5: BFS/DFS configuration exploration
 *
 * Course Alignment: MIT 6.852 Lec 6-7, CMU 15-712 Lec 9-10
 */

#ifndef FLP_SEARCH_H
#define FLP_SEARCH_H

#include "flp_common.h"
#include "flp_config.h"
#include "flp_protocol.h"
#include "flp_message.h"

/* ---------------------------------------------------------------------------
 * L4 Lemma 1: Commutativity of Disjoint Events
 * ---------------------------------------------------------------------------
 * If events e1 = (p1, m1) and e2 = (p2, m2) involve different
 * processes (p1 != p2) and neither message is addressed to the
 * other process, then e1(e2(C)) = e2(e1(C)).
 */

/**
 * Check whether two events are disjoint (involve different processes).
 */
bool flp_events_are_disjoint(const flp_event *e1, const flp_event *e2);

/**
 * Verify commutativity for two disjoint events applied to a config.
 * Applies e1 then e2 to cfg, and e2 then e1 to a clone, and compares.
 */
bool flp_verify_commutativity(const flp_configuration *cfg,
                               const flp_event *e1, const flp_event *e2);

/* ---------------------------------------------------------------------------
 * L4 Lemma 2: Existence of Initial Bivalent Configuration
 * ---------------------------------------------------------------------------
 * For any consensus protocol tolerating one fault, there exists an
 * initial configuration that is bivalent.
 *
 * Proof: Assume all initial configs are univalent. Adjacent configs
 * (differing in one input) must have different valences if they
 * differ. The chain from all-0 to all-1 must have a transition
 * from 0-valent to 1-valent. The configs at the boundary are
 * shown to be bivalent, contradiction.
 */

/**
 * Search for a bivalent initial configuration.
 */
bool flp_find_bivalent_initial(const flp_protocol_desc *desc,
                                int32_t n, int32_t max_depth,
                                flp_configuration *out);

/**
 * Verify Lemma 2 for a protocol.
 */
bool flp_verify_initial_bivalency(const flp_protocol_desc *desc,
                                   int32_t n, int32_t max_depth);

/* ---------------------------------------------------------------------------
 * L4 Lemma 3: Bivalence Preservation (the "Fork" Lemma)
 * ---------------------------------------------------------------------------
 * From a bivalent configuration C and an event e applicable to C,
 * there exists a finite schedule sigma (not containing e.process)
 * such that e(sigma(C)) is bivalent.
 */

/**
 * Search for a preserving schedule from a bivalent configuration.
 * Given bivalent C and event e, find sigma such that:
 *   sigma does not involve e.process
 *   AND e(sigma(C)) is bivalent.
 */
bool flp_find_preserving_schedule(const flp_configuration *cfg,
                                   flp_process_id event_proc,
                                   int32_t msg_idx,
                                   int32_t max_depth,
                                   flp_schedule *out_sched);

/**
 * Construct an infinite non-deciding schedule using Lemma 3 repeatedly.
 */
int32_t flp_construct_infinite_run(const flp_protocol_desc *desc,
                                    int32_t n,
                                    int32_t max_steps,
                                    int32_t max_depth,
                                    flp_schedule *out_sched);

/* ---------------------------------------------------------------------------
 * L5: Configuration Graph Exploration
 * ---------------------------------------------------------------------------
 */

/** Configuration graph node for BFS/DFS. */
typedef struct {
    flp_configuration   config;
    int32_t             parent_id;
    flp_event           incoming_event;
    int32_t             depth;
    bool                visited;
} flp_graph_node;

/** Configuration graph structure. */
typedef struct {
    flp_graph_node  *nodes;
    int32_t         capacity;
    int32_t         size;
    int32_t         *adjacency;
    int32_t         *adj_start;
    int32_t         *adj_count;
    int32_t         adj_capacity;
    int32_t         adj_size;
} flp_config_graph;

/**
 * Initialize a configuration graph for exploration.
 */
void flp_graph_init(flp_config_graph *graph, int32_t capacity);

/**
 * Free memory associated with a configuration graph.
 */
void flp_graph_destroy(flp_config_graph *graph);

/**
 * Add a configuration node to the graph.
 */
int32_t flp_graph_add_node(flp_config_graph *graph,
                            const flp_configuration *cfg);

/**
 * Add an edge between two nodes in the graph.
 */
void flp_graph_add_edge(flp_config_graph *graph,
                         int32_t from, int32_t to,
                         const flp_event *event);

/**
 * Build the configuration graph reachable from a start configuration.
 */
int32_t flp_graph_build(flp_config_graph *graph,
                         const flp_protocol_desc *desc,
                         const flp_configuration *start,
                         int32_t max_depth);

/**
 * BFS traversal of the configuration graph.
 */
typedef bool (*flp_graph_visitor)(const flp_graph_node *node,
                                   int32_t depth, void *user_data);

void flp_graph_bfs(const flp_config_graph *graph, int32_t start_id,
                    flp_graph_visitor visitor, void *user_data);

/**
 * DFS traversal of the configuration graph.
 */
void flp_graph_dfs(const flp_config_graph *graph, int32_t start_id,
                    flp_graph_visitor visitor, void *user_data);

/**
 * Find a path from start to a node satisfying a predicate.
 */
int32_t flp_graph_find_path(const flp_config_graph *graph,
                             int32_t start_id,
                             bool (*predicate)(const flp_graph_node*, void*),
                             void *pred_data,
                             int32_t max_depth,
                             int32_t *path_out, int32_t path_capacity);

/**
 * Print a summary of the configuration graph.
 */
void flp_graph_print_summary(const flp_config_graph *graph);

#endif /* FLP_SEARCH_H */
