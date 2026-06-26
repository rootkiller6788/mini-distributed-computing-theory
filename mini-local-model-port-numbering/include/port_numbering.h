/**
 * @file port_numbering.h
 * @brief Port Numbering in the LOCAL Model
 *
 * In the LOCAL model (with port numbering), each node v assigns a distinct
 * integer from 0 to deg(v)-1 to each incident edge. The port numbering is
 * LOCAL: each node picks its own port assignments independently, without
 * coordination with neighbors.
 *
 * This file implements the three classical types of port numbering:
 *  - Arbitrary (adversarial): the worst-case assignment
 *  - Consistent: across an edge {u,v}, both endpoints agree on some property
 *  - Orientation-based: consistent with a global orientation of edges
 *
 * Key concepts:
 *  - Port-to-neighbor mapping: given a port number, which neighbor?
 *  - Neighbor-to-port mapping: given a neighbor UID, which port?
 *  - Symmetry breaking: port numbering is a form of local symmetry breaking
 *
 * References:
 *  - Angluin, D. (1980). "Local and global properties in networks of processors."
 *  - Yamashita & Kameda (1996). "Computing on anonymous networks."
 *  - Suomela, J. (2013). "Survey of local algorithms."
 *
 * Level: L1 (Definitions), L2 (Core Concepts), L3 (Mathematical Structures)
 */

#ifndef PORT_NUMBERING_H
#define PORT_NUMBERING_H

#include "local_model.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ================================================================
 * L1: Port Numbering Definitions
 * ================================================================ */

/**
 * @brief Types of port numbering strategies.
 *
 * These correspond to different assumptions about how nodes locally
 * label their incident edges. The choice of port numbering can drastically
 * affect round complexity.
 */
typedef enum {
    PORT_ARBITRARY,          /* Adversarial: worst-case assignment */
    PORT_CONSISTENT,         /* Edge-consistent: u-port matches some invariant across {u,v} */
    PORT_ORIENTATION,        /* Orientation-based: consistent with a global edge orientation */
    PORT_ID_ORDERED,         /* Ports ordered by neighbor UID (ascending) */
    PORT_DEGREE_ORDERED,     /* Ports ordered by neighbor degree (descending) */
    PORT_DISTANCE_ORDERED,   /* Ports ordered by known distances */
    PORT_RANDOM              /* Randomized port assignment */
} port_numbering_type_t;

/**
 * @brief Port numbering structure attached to a node.
 *
 * For each incident edge {v, u}, the node v assigns a port number
 * p in [0, deg(v)-1]. This structure maps ports to neighbors and vice versa.
 */
typedef struct {
    uint32_t   degree;                          /* Node degree = number of ports */
    uint32_t   neighbor_by_port[LOCAL_MAX_DEGREE];  /* neighbor_by_port[p] = neighbor index/UUID */
    uint32_t   port_by_neighbor[LOCAL_MAX_NODES];   /* port_by_neighbor[u] = port (sentinel if no edge) */
    uint32_t   owner_uid;                       /* Which node owns this port numbering */
    port_numbering_type_t type;                 /* Strategy used */
    bool       is_consistent;                   /* Does this satisfy edge-consistency? */
} port_numbering_t;

/**
 * @brief Get the port number that node v uses for its edge to neighbor u.
 *
 * @param pn       Port numbering of node v.
 * @param neighbor_u UID of neighbor u.
 * @return          Port number, or UINT32_MAX if u is not a neighbor.
 *
 * Theorem: In any valid port numbering, the mapping is a bijection
 * between [0, deg(v)-1] and the set of neighbors of v.
 */
uint32_t port_get_port_for_neighbor(const port_numbering_t *pn, uint32_t neighbor_u);

/**
 * @brief Get the neighbor UID accessible through port p of node v.
 *
 * @param pn    Port numbering of node v.
 * @param port  Port number (0 <= port < pn->degree).
 * @return      Neighbor UID, or UINT32_MAX if port is invalid.
 */
uint32_t port_get_neighbor_for_port(const port_numbering_t *pn, uint32_t port);

/* ================================================================
 * L2: Port Numbering Strategies
 * ================================================================ */

/**
 * @brief Assign arbitrary port numbers to all incident edges of a node.
 *
 * This is the default adversarial model. The assignment is a bijection
 * from {0, ..., deg(v)-1} to the set of neighbors, but the specific mapping
 * is arbitrary and may be chosen adversarially.
 *
 * @param pn            Port numbering to fill.
 * @param neighbors     Array of neighbor UIDs.
 * @param degree        Number of neighbors.
 * @param seed          Optional seed (0 if arbitrary is fully adversarial).
 *
 * Complexity: O(degree log degree) for arbitrary permutation.
 */
int port_assign_arbitrary(port_numbering_t *pn, const uint32_t *neighbors,
                          uint32_t degree, uint64_t seed);

/**
 * @brief Assign port numbers ordered by neighbor UID (ascending).
 *
 * Port 0 goes to the neighbor with smallest UID, port 1 to the second
 * smallest, etc. This is a deterministic, local rule that breaks symmetry.
 *
 * Theorem (Angluin, 1980): In networks where UIDs are globally unique,
 * ID-ordered port numbering can replace arbitrary port numbering at no
 * asymptotic round cost.
 *
 * @param pn            Port numbering to fill.
 * @param neighbors     Array of neighbor UIDs.
 * @param degree        Number of neighbors.
 */
int port_assign_id_ordered(port_numbering_t *pn, const uint32_t *neighbors, uint32_t degree);

/**
 * @brief Assign port numbers ordered by neighbor degree (descending),
 *        breaking ties by UID (ascending).
 *
 * Useful in algorithms that prioritize high-degree neighbors.
 *
 * @param pn            Port numbering to fill.
 * @param neighbors     Array of neighbor UIDs.
 * @param degrees       Array of neighbor degrees (same indexing as neighbors).
 * @param degree        Number of neighbors.
 */
int port_assign_degree_ordered(port_numbering_t *pn, const uint32_t *neighbors,
                               const uint32_t *degrees, uint32_t degree);

/**
 * @brief Assign port numbers randomly.
 *
 * Each port is assigned to a uniformly random neighbor (without replacement).
 *
 * @param pn            Port numbering to fill.
 * @param neighbors     Array of neighbor UIDs.
 * @param degree        Number of neighbors.
 * @param seed          Random seed for reproducibility.
 */
int port_assign_random(port_numbering_t *pn, const uint32_t *neighbors,
                       uint32_t degree, uint64_t seed);

/* ================================================================
 * L2: Port Numbering Properties and Consistency
 * ================================================================ */

/**
 * @brief Check if the port numbering is a valid bijection.
 *
 * A valid port numbering maps each neighbor to exactly one port
 * in [0, deg-1], and each port to exactly one neighbor.
 *
 * @param pn    Port numbering to validate.
 * @return      true iff valid.
 */
bool port_is_valid(const port_numbering_t *pn);

/**
 * @brief Check edge-consistency between two nodes.
 *
 * Edge-consistency means: for edge {u,v}, the port number that u uses
 * for v can be computed from the port number that v uses for u, via
 * some globally known function f (e.g., f(p) = p for symmetric consistency,
 * or f(p) = deg(v)-1-p for anti-symmetric).
 *
 * @param pn_u      Port numbering of node u.
 * @param pn_v      Port numbering of node v.
 * @param edge_uid_u_v  UID of v as seen by u, and vice versa.
 * @param f         Consistency function: f(u_port) should equal v_port.
 *                  If NULL, checks symmetric consistency (u_port == v_port).
 * @return          true iff edge-consistent.
 */
typedef uint32_t (*port_consistency_fn_t)(uint32_t port, uint32_t from_deg, uint32_t to_deg);
bool port_check_edge_consistency(const port_numbering_t *pn_u, const port_numbering_t *pn_v,
                                 uint32_t v_uid, uint32_t u_uid,
                                 port_consistency_fn_t f);

/**
 * @brief Global symmetric consistency: for every edge {u,v}, u's port to v
 *        equals v's port to u.
 *
 * @param pns           Array of port numberings (one per node).
 * @param num_nodes     Number of nodes.
 * @param adjacency     Row-major adjacency matrix.
 * @return              true iff all edges are symmetrically consistent.
 */
bool port_is_globally_symmetric(const port_numbering_t *pns, uint32_t num_nodes,
                                const bool *adjacency);

/**
 * @brief Find the "flip" consistency function: f(p, deg_from, deg_to) = deg_to - 1 - p.
 *
 * This is used in anti-symmetric port numbering, common in ring networks
 * where one direction is port 0 and the other is port 1.
 *
 * @param port          Port number at the sending endpoint.
 * @param from_deg      Degree of the sending node.
 * @param to_deg        Degree of the receiving node.
 * @return              The "flipped" port number.
 */
uint32_t port_consistency_flip(uint32_t port, uint32_t from_deg, uint32_t to_deg);

/* ================================================================
 * L3: Port Numbering in Specific Topologies
 * ================================================================ */

/**
 * @brief Assign a consistent orientation-based port numbering on a ring.
 *
 * On a ring C_n, there are two natural orientations: clockwise and
 * counter-clockwise. Port 0 is assigned to the clockwise neighbor,
 * port 1 to the counter-clockwise neighbor.
 *
 * This function determines the orientation from the neighbor UIDs:
 * the clockwise neighbor is the one with the smaller UID (modulo wrapping).
 *
 * @param pns           Array of port numberings (caller allocates, n entries).
 * @param n             Number of nodes (ring size).
 * @param adjacency     Adjacency matrix of the ring.
 * @return              0 on success.
 */
int port_assign_ring_orientation(port_numbering_t *pns, uint32_t n, const bool *adjacency);

/**
 * @brief Assign a BFS-tree-based port numbering.
 *
 * When a spanning tree (BFS from a root) is available, port 0 can be
 * reserved for the parent (toward the root), and subsequent ports for
 * children (away from the root). This is essential for tree-based
 * LOCAL algorithms (e.g., convergecast, broadcast).
 *
 * @param pns           Array of port numberings.
 * @param num_nodes     Number of nodes.
 * @param adjacency     Adjacency matrix.
 * @param root          Root node index.
 * @param parent        Output array: parent[i] = parent of node i in BFS tree.
 * @return              0 on success.
 */
int port_assign_bfs_tree(port_numbering_t *pns, uint32_t num_nodes,
                         const bool *adjacency, uint32_t root,
                         uint32_t *parent);

/**
 * @brief Compute the view of depth r from a node given port numberings.
 *
 * The view is the labeled tree of radius r around a node, where each
 * node is labeled by its UID and each edge by the port numbers at both
 * endpoints. Two nodes with identical views are indistinguishable in
 * the LOCAL model after r rounds (this is the essence of many lower bounds).
 *
 * @param pns           Array of port numberings.
 * @param num_nodes     Number of nodes.
 * @param adjacency     Adjacency matrix.
 * @param center        Center node index.
 * @param r             Radius.
 * @param view_buf      Output buffer for view representation (caller allocates).
 * @param buf_size      Size of view_buf in bytes.
 * @return              Number of bytes written, 0 on error.
 */
size_t port_compute_view(const port_numbering_t *pns, uint32_t num_nodes,
                         const bool *adjacency, uint32_t center, uint32_t r,
                         uint8_t *view_buf, size_t buf_size);

/* ================================================================
 * L4: Lower Bound: Port Numbering and Symmetry Breaking
 * ================================================================ */

/**
 * @brief Count the number of symmetry classes (orbits) in a port-numbered graph.
 *
 * Two nodes are in the same symmetry class if there exists an automorphism
 * of the port-numbered graph that maps one to the other. Symmetry is the
 * main obstacle to deterministic LOCAL algorithms on anonymous networks.
 *
 * @param pns           Array of port numberings.
 * @param num_nodes     Number of nodes.
 * @param adjacency     Adjacency matrix.
 * @return              Number of distinct symmetry classes (1 <= result <= num_nodes).
 */
uint32_t port_count_symmetry_classes(const port_numbering_t *pns, uint32_t num_nodes,
                                     const bool *adjacency);

/**
 * @brief Check if two nodes are symmetric in the port-numbered graph.
 *
 * Automorphism checking via Weisfeiler-Lehman-style iterative refinement
 * (1-WL, equivalent to color refinement). This captures indistinguishability
 * in the LOCAL model.
 *
 * @param pns           Port numberings.
 * @param num_nodes     Number of nodes.
 * @param adjacency     Adjacency matrix.
 * @param u, v          Node indices to compare.
 * @param max_rounds    Maximum refinement rounds.
 * @return              true if u and v are indistinguishable under 1-WL.
 */
bool port_nodes_indistinguishable(const port_numbering_t *pns, uint32_t num_nodes,
                                  const bool *adjacency, uint32_t u, uint32_t v,
                                  uint32_t max_rounds);

/**
 * @brief Compute the coloring number of a port-numbered graph:
 *        the minimum colors needed such that no two nodes with the
 *        same initial knowledge (UID + port numbering + input) have
 *        the same color. This is the symmetry-breaking number.
 *
 * @param pns           Port numberings.
 * @param num_nodes     Number of nodes.
 * @param adjacency     Adjacency matrix.
 * @return              Symmetry-breaking number.
 */
uint32_t port_symmetry_breaking_number(const port_numbering_t *pns, uint32_t num_nodes,
                                       const bool *adjacency);

/* ================================================================
 * L2: Port Numbering Utilities
 * ================================================================ */

/**
 * @brief Initialize a port numbering structure with zeros.
 *
 * @param pn        Port numbering to initialize.
 * @param owner_uid Node that owns this port numbering.
 */
void port_init(port_numbering_t *pn, uint32_t owner_uid);

/**
 * @brief Copy a port numbering.
 *
 * @param dst   Destination.
 * @param src   Source.
 */
void port_copy(port_numbering_t *dst, const port_numbering_t *src);

/**
 * @brief Get a human-readable description of a port numbering type.
 *
 * @param type  Port numbering type.
 * @return      Constant string.
 */
const char *port_type_name(port_numbering_type_t type);

/**
 * @brief Compute the port-numbered adjacency: for each node i and port p,
 *        store the pair (neighbor UID, neighbor's port on the same edge).
 *
 * This is the fundamental input to any port-numbered LOCAL algorithm.
 *
 * @param pns           Array of port numberings.
 * @param num_nodes     Number of nodes.
 * @param out_neighbor  Output: out_neighbor[i * LOCAL_MAX_DEGREE + p] = neighbor UID.
 * @param out_nport     Output: out_nport[i * LOCAL_MAX_DEGREE + p] = neighbor's port on same edge.
 * @return              0 on success.
 */
int port_build_dual_view(const port_numbering_t *pns, uint32_t num_nodes,
                         uint32_t *out_neighbor, uint32_t *out_nport);

#endif /* PORT_NUMBERING_H */
