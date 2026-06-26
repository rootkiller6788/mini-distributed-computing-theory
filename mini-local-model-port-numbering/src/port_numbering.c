/**
 * @file port_numbering.c
 * @brief Implementation of port numbering in the LOCAL model.
 *
 * Port numbering is how each node locally labels its incident edges.
 * The choice of port numbering affects the power of LOCAL algorithms.
 *
 * Knowledge points covered:
 *  L1: Port numbering definition, port-to-neighbor mapping
 *  L2: Arbitrary, ID-ordered, degree-ordered, random port assignments
 *  L2: Consistency checking (edge-consistent, globally symmetric)
 *  L3: Ring orientation port numbering, BFS-tree port numbering
 *  L3: View computation (labeled neighborhood trees)
 *  L4: Symmetry classes and indistinguishability via 1-WL refinement
 *
 * References:
 *  - Angluin (1980). "Local and global properties in networks of processors."
 *  - Yamashita & Kameda (1996). "Computing on anonymous networks."
 *  - Suomela (2013). "Survey of local algorithms." ACM Comput. Surv.
 */

#include "port_numbering.h"
#include "local_model.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Sentinel for "no edge" in port_by_neighbor */
#define PORT_NONE   UINT32_MAX

/* ================================================================
 * L1: Basic Port Numbering Accessors
 * ================================================================ */

uint32_t port_get_port_for_neighbor(const port_numbering_t *pn, uint32_t neighbor_u) {
    if (!pn || neighbor_u >= LOCAL_MAX_NODES) return PORT_NONE;
    return pn->port_by_neighbor[neighbor_u];
}

uint32_t port_get_neighbor_for_port(const port_numbering_t *pn, uint32_t port) {
    if (!pn || port >= pn->degree) return PORT_NONE;
    return pn->neighbor_by_port[port];
}

void port_init(port_numbering_t *pn, uint32_t owner_uid) {
    if (!pn) return;
    memset(pn, 0, sizeof(*pn));
    pn->owner_uid = owner_uid;
    for (uint32_t i = 0; i < LOCAL_MAX_NODES; i++) {
        pn->port_by_neighbor[i] = PORT_NONE;
    }
}

void port_copy(port_numbering_t *dst, const port_numbering_t *src) {
    if (!dst || !src) return;
    memcpy(dst, src, sizeof(port_numbering_t));
}

const char *port_type_name(port_numbering_type_t type) {
    switch (type) {
        case PORT_ARBITRARY:        return "Arbitrary";
        case PORT_CONSISTENT:       return "Consistent";
        case PORT_ORIENTATION:      return "Orientation-Based";
        case PORT_ID_ORDERED:       return "ID-Ordered";
        case PORT_DEGREE_ORDERED:   return "Degree-Ordered";
        case PORT_DISTANCE_ORDERED: return "Distance-Ordered";
        case PORT_RANDOM:           return "Random";
        default:                    return "Unknown";
    }
}

/* ================================================================
 * L2: Port Assignment Strategies
 * ================================================================ */

/* Simple Fisher-Yates shuffle (used for arbitrary/random assignments) */
static void shuffle_indices(uint32_t *arr, uint32_t n, uint64_t seed) {
    /* Use a simple LCG for deterministic shuffling */
    uint64_t state = (seed == 0) ? 123456789ULL : seed;

    for (uint32_t i = n; i > 1; i--) {
        /* Generate random index in [0, i-1] */
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        uint32_t j = (uint32_t)((state >> 33) % i);
        uint32_t tmp = arr[i - 1];
        arr[i - 1] = arr[j];
        arr[j] = tmp;
    }
}

int port_assign_arbitrary(port_numbering_t *pn, const uint32_t *neighbors,
                          uint32_t degree, uint64_t seed) {
    if (!pn || !neighbors || degree > LOCAL_MAX_DEGREE) return -1;

    pn->degree = degree;
    pn->type = PORT_ARBITRARY;
    pn->is_consistent = false;

    /* Initialize neighbor_by_port with neighbors in order */
    for (uint32_t i = 0; i < degree; i++) {
        pn->neighbor_by_port[i] = neighbors[i];
    }

    /* Shuffle if seed is provided */
    if (seed != 0) {
        shuffle_indices(pn->neighbor_by_port, degree, seed);
    }
    /* If seed == 0, leave in original (adversarial) order */

    /* Build reverse mapping */
    for (uint32_t i = 0; i < LOCAL_MAX_NODES; i++) {
        pn->port_by_neighbor[i] = PORT_NONE;
    }
    for (uint32_t p = 0; p < degree; p++) {
        uint32_t nb = pn->neighbor_by_port[p];
        if (nb < LOCAL_MAX_NODES) {
            pn->port_by_neighbor[nb] = p;
        }
    }

    return 0;
}

/* Comparison function for qsort: ascending UID */
static int cmp_uid_asc(const void *a, const void *b) {
    uint32_t ua = *(const uint32_t *)a;
    uint32_t ub = *(const uint32_t *)b;
    if (ua < ub) return -1;
    if (ua > ub) return 1;
    return 0;
}

int port_assign_id_ordered(port_numbering_t *pn, const uint32_t *neighbors, uint32_t degree) {
    if (!pn || !neighbors || degree > LOCAL_MAX_DEGREE) return -1;

    pn->degree = degree;
    pn->type = PORT_ID_ORDERED;
    pn->is_consistent = false;

    /* Copy and sort neighbors by UID ascending */
    uint32_t *sorted = (uint32_t *)malloc(degree * sizeof(uint32_t));
    if (!sorted) return -1;
    memcpy(sorted, neighbors, degree * sizeof(uint32_t));
    qsort(sorted, degree, sizeof(uint32_t), cmp_uid_asc);

    for (uint32_t p = 0; p < degree; p++) {
        pn->neighbor_by_port[p] = sorted[p];
    }
    free(sorted);

    /* Build reverse mapping */
    for (uint32_t i = 0; i < LOCAL_MAX_NODES; i++) {
        pn->port_by_neighbor[i] = PORT_NONE;
    }
    for (uint32_t p = 0; p < degree; p++) {
        pn->port_by_neighbor[pn->neighbor_by_port[p]] = p;
    }

    return 0;
}

/* Structure for sorting by degree desc, then UID asc */
typedef struct {
    uint32_t uid;
    uint32_t deg;
} neighbor_info_t;

static int cmp_deg_desc(const void *a, const void *b) {
    const neighbor_info_t *na = (const neighbor_info_t *)a;
    const neighbor_info_t *nb = (const neighbor_info_t *)b;
    if (na->deg > nb->deg) return -1;
    if (na->deg < nb->deg) return 1;
    /* Tie-break by UID ascending */
    if (na->uid < nb->uid) return -1;
    if (na->uid > nb->uid) return 1;
    return 0;
}

int port_assign_degree_ordered(port_numbering_t *pn, const uint32_t *neighbors,
                               const uint32_t *degrees, uint32_t degree) {
    if (!pn || !neighbors || !degrees || degree > LOCAL_MAX_DEGREE) return -1;

    pn->degree = degree;
    pn->type = PORT_DEGREE_ORDERED;
    pn->is_consistent = false;

    neighbor_info_t *info = (neighbor_info_t *)malloc(degree * sizeof(neighbor_info_t));
    if (!info) return -1;

    for (uint32_t i = 0; i < degree; i++) {
        info[i].uid = neighbors[i];
        info[i].deg = degrees[i];
    }
    qsort(info, degree, sizeof(neighbor_info_t), cmp_deg_desc);

    for (uint32_t p = 0; p < degree; p++) {
        pn->neighbor_by_port[p] = info[p].uid;
    }
    free(info);

    /* Build reverse mapping */
    for (uint32_t i = 0; i < LOCAL_MAX_NODES; i++) {
        pn->port_by_neighbor[i] = PORT_NONE;
    }
    for (uint32_t p = 0; p < degree; p++) {
        pn->port_by_neighbor[pn->neighbor_by_port[p]] = p;
    }

    return 0;
}

int port_assign_random(port_numbering_t *pn, const uint32_t *neighbors,
                       uint32_t degree, uint64_t seed) {
    if (!pn || !neighbors || degree > LOCAL_MAX_DEGREE) return -1;

    pn->degree = degree;
    pn->type = PORT_RANDOM;
    pn->is_consistent = false;

    for (uint32_t i = 0; i < degree; i++) {
        pn->neighbor_by_port[i] = neighbors[i];
    }

    /* Deterministically "randomize" using seed */
    if (seed != 0) {
        shuffle_indices(pn->neighbor_by_port, degree, seed);
    }

    /* Build reverse mapping */
    for (uint32_t i = 0; i < LOCAL_MAX_NODES; i++) {
        pn->port_by_neighbor[i] = PORT_NONE;
    }
    for (uint32_t p = 0; p < degree; p++) {
        pn->port_by_neighbor[pn->neighbor_by_port[p]] = p;
    }

    return 0;
}

/* ================================================================
 * L2: Validation and Consistency Checking
 * ================================================================ */

bool port_is_valid(const port_numbering_t *pn) {
    if (!pn) return false;
    if (pn->degree > LOCAL_MAX_DEGREE) return false;

    /* Check: each port maps to a unique neighbor */
    bool *seen_neighbors = (bool *)calloc(LOCAL_MAX_NODES, sizeof(bool));
    if (!seen_neighbors) return false;

    for (uint32_t p = 0; p < pn->degree; p++) {
        uint32_t nb = pn->neighbor_by_port[p];
        if (nb >= LOCAL_MAX_NODES || seen_neighbors[nb]) {
            free(seen_neighbors);
            return false;
        }
        seen_neighbors[nb] = true;
    }

    /* Check: reverse mapping consistency */
    for (uint32_t p = 0; p < pn->degree; p++) {
        uint32_t nb = pn->neighbor_by_port[p];
        if (pn->port_by_neighbor[nb] != p) {
            free(seen_neighbors);
            return false;
        }
    }

    /* Check: non-neighbor entries map to PORT_NONE */
    for (uint32_t u = 0; u < LOCAL_MAX_NODES; u++) {
        if (!seen_neighbors[u] && pn->port_by_neighbor[u] != PORT_NONE) {
            free(seen_neighbors);
            return false;
        }
    }

    free(seen_neighbors);
    return true;
}

bool port_check_edge_consistency(const port_numbering_t *pn_u, const port_numbering_t *pn_v,
                                 uint32_t v_uid, uint32_t u_uid,
                                 port_consistency_fn_t f) {
    if (!pn_u || !pn_v) return false;

    uint32_t u_port = port_get_port_for_neighbor(pn_u, v_uid);
    uint32_t v_port = port_get_port_for_neighbor(pn_v, u_uid);

    if (u_port == PORT_NONE || v_port == PORT_NONE) return false;

    if (f) {
        return f(u_port, pn_u->degree, pn_v->degree) == v_port;
    } else {
        /* Default: symmetric consistency */
        return u_port == v_port;
    }
}

bool port_is_globally_symmetric(const port_numbering_t *pns, uint32_t num_nodes,
                                const bool *adjacency) {
    if (!pns || !adjacency) return false;

    for (uint32_t u = 0; u < num_nodes; u++) {
        for (uint32_t v = u + 1; v < num_nodes; v++) {
            if (!adjacency[u * num_nodes + v]) continue;

            uint32_t u_p = port_get_port_for_neighbor(&pns[u], v);
            uint32_t v_p = port_get_port_for_neighbor(&pns[v], u);
            if (u_p != v_p) return false;
        }
    }
    return true;
}

uint32_t port_consistency_flip(uint32_t port, uint32_t from_deg, uint32_t to_deg) {
    (void)from_deg;
    if (port >= to_deg) return PORT_NONE;
    return to_deg - 1 - port;
}

/* ================================================================
 * L3: Ring Orientation Port Numbering
 * ================================================================ */

int port_assign_ring_orientation(port_numbering_t *pns, uint32_t n, const bool *adjacency) {
    if (!pns || n < 3 || !adjacency) return -1;

    for (uint32_t i = 0; i < n; i++) {
        port_init(&pns[i], i);

        /* Find two neighbors in the ring */
        uint32_t nbrs[2];
        uint32_t cnt = 0;
        for (uint32_t j = 0; j < n && cnt < 2; j++) {
            if (adjacency[i * n + j] && i != j) {
                nbrs[cnt++] = j;
            }
        }

        if (cnt != 2) continue;  /* Not a valid ring node */

        /* Port 0 = neighbor with smaller index (clockwise direction) */
        if (nbrs[0] < nbrs[1]) {
            pns[i].neighbor_by_port[0] = nbrs[0];
            pns[i].neighbor_by_port[1] = nbrs[1];
        } else {
            pns[i].neighbor_by_port[0] = nbrs[1];
            pns[i].neighbor_by_port[1] = nbrs[0];
        }

        pns[i].degree = 2;
        pns[i].type = PORT_ORIENTATION;
        pns[i].port_by_neighbor[pns[i].neighbor_by_port[0]] = 0;
        pns[i].port_by_neighbor[pns[i].neighbor_by_port[1]] = 1;

        /* On a ring, this orientation is locally consistent */
        pns[i].is_consistent = true;
    }

    return 0;
}

/* ================================================================
 * L3: BFS Tree Port Numbering
 * ================================================================ */

int port_assign_bfs_tree(port_numbering_t *pns, uint32_t num_nodes,
                         const bool *adjacency, uint32_t root,
                         uint32_t *parent) {
    if (!pns || !adjacency || !parent || root >= num_nodes) return -1;

    /* BFS to build tree */
    uint32_t *dist = (uint32_t *)malloc(num_nodes * sizeof(uint32_t));
    uint32_t *queue = (uint32_t *)malloc(num_nodes * sizeof(uint32_t));
    if (!dist || !queue) {
        free(dist); free(queue);
        return -1;
    }

    for (uint32_t i = 0; i < num_nodes; i++) {
        dist[i] = UINT32_MAX;
        parent[i] = UINT32_MAX;
    }

    dist[root] = 0;
    parent[root] = root;
    uint32_t qh = 0, qt = 0;
    queue[qt++] = root;

    while (qh < qt) {
        uint32_t u = queue[qh++];
        for (uint32_t v = 0; v < num_nodes; v++) {
            if (adjacency[u * num_nodes + v] && u != v && dist[v] == UINT32_MAX) {
                dist[v] = dist[u] + 1;
                parent[v] = u;
                queue[qt++] = v;
            }
        }
    }

    /* Assign ports: port 0 = parent, remaining ports = children (by UID ascending) */
    for (uint32_t i = 0; i < num_nodes; i++) {
        port_init(&pns[i], i);
        pns[i].type = PORT_CONSISTENT;

        uint32_t port_idx = 0;

        /* Port 0 = parent (if any, and not root) */
        if (i != root && parent[i] != UINT32_MAX) {
            pns[i].neighbor_by_port[port_idx] = parent[i];
            pns[i].port_by_neighbor[parent[i]] = port_idx;
            port_idx++;
        }

        /* Remaining ports = children, sorted by UID ascending */
        uint32_t children[LOCAL_MAX_DEGREE];
        uint32_t child_cnt = 0;
        for (uint32_t v = 0; v < num_nodes; v++) {
            if (parent[v] == i && v != i) {
                children[child_cnt++] = v;
            }
        }

        /* Sort children by UID */
        for (uint32_t a = 0; a < child_cnt; a++) {
            for (uint32_t b = a + 1; b < child_cnt; b++) {
                if (children[a] > children[b]) {
                    uint32_t tmp = children[a];
                    children[a] = children[b];
                    children[b] = tmp;
                }
            }
        }

        for (uint32_t c = 0; c < child_cnt; c++) {
            pns[i].neighbor_by_port[port_idx] = children[c];
            pns[i].port_by_neighbor[children[c]] = port_idx;
            port_idx++;
        }

        pns[i].degree = port_idx;
        pns[i].is_consistent = true;
    }

    free(dist);
    free(queue);
    return 0;
}

/* ================================================================
 * L3: View Computation (Labeled Neighborhood Trees)
 * ================================================================ */

/**
 * Encode a node's r-view as a string-like representation in the output buffer.
 * The format is a preorder traversal of the tree with:
 *   N<uid>                 - Node with UID
 *   P<port>                - Port to child
 *   U                      - Up (back to parent)
 */
static size_t encode_view_recursive(const port_numbering_t *pns, uint32_t num_nodes,
                                    const bool *adjacency, uint32_t node, uint32_t parent,
                                    uint32_t depth, uint32_t max_depth,
                                    uint8_t *buf, size_t buf_size, size_t offset) {
    if (depth > max_depth) return offset;
    if (offset + 32 > buf_size) return offset;  /* Safety margin */

    /* Encode: N<uid> */
    offset += (size_t)snprintf((char *)buf + offset, buf_size - offset,
                                "N%u", node);

    for (uint32_t p = 0; p < pns[node].degree && offset < buf_size; p++) {
        uint32_t child = pns[node].neighbor_by_port[p];
        if (child == parent || child >= num_nodes) continue;

        /* Encode: P<port> */
        offset += (size_t)snprintf((char *)buf + offset, buf_size - offset,
                                    "P%u", p);

        offset = encode_view_recursive(pns, num_nodes, adjacency,
                                       child, node, depth + 1, max_depth,
                                       buf, buf_size, offset);

        /* Encode: U (backtrack) */
        if (offset < buf_size) {
            buf[offset++] = 'U';
        }
    }

    return offset;
}

size_t port_compute_view(const port_numbering_t *pns, uint32_t num_nodes,
                         const bool *adjacency, uint32_t center, uint32_t r,
                         uint8_t *view_buf, size_t buf_size) {
    if (!pns || !view_buf || buf_size == 0 || center >= num_nodes) return 0;
    memset(view_buf, 0, buf_size);
    return encode_view_recursive(pns, num_nodes, adjacency, center, UINT32_MAX,
                                 0, r, view_buf, buf_size, 0);
}

/* ================================================================
 * L4: Symmetry Classes via 1-WL (Color Refinement)
 * ================================================================ */

/**
 * 1-WL (1-dimensional Weisfeiler-Lehman) algorithm on a port-numbered graph.
 *
 * Initially, each node gets a color based on its port numbering signature
 * (multiset of (port, neighbor color) pairs). Colors are iteratively refined
 * until the partition stabilizes.
 *
 * Two nodes with the same final color are indistinguishable in the LOCAL model
 * (more precisely, they have the same T-neighborhood for any T ≤ iterations).
 */
uint32_t port_count_symmetry_classes(const port_numbering_t *pns, uint32_t num_nodes,
                                     const bool *adjacency) {
    if (!pns || !adjacency || num_nodes == 0) return 0;

    uint32_t *colors = (uint32_t *)calloc(num_nodes, sizeof(uint32_t));
    uint32_t *new_colors = (uint32_t *)calloc(num_nodes, sizeof(uint32_t));
    if (!colors || !new_colors) {
        free(colors); free(new_colors);
        return num_nodes;
    }

    /* Initial coloring: each node has a unique color (its UID) */
    /* In the port-numbered model WITH UIDs, all nodes are immediately distinct. */
    /* But for symmetry analysis, we want to ignore UIDs and look purely at
       the structure. So we start with: node degree */
    for (uint32_t i = 0; i < num_nodes; i++) {
        colors[i] = pns[i].degree;
    }

    /* Iteratively refine */
    uint32_t max_iterations = num_nodes + 10;
    for (uint32_t iter = 0; iter < max_iterations; iter++) {
        /* For each node, collect the multiset of (port, neighbor_color) */
        /* Use a simple hash-based approach */
        bool changed = false;

        for (uint32_t i = 0; i < num_nodes; i++) {
            /* Build a hash from the multiset */
            uint64_t hash = 14695981039346656037ULL;  /* FNV-1a offset */
            uint64_t prime = 1099511628211ULL;

            hash ^= colors[i];
            hash *= prime;

            for (uint32_t p = 0; p < pns[i].degree; p++) {
                uint32_t nb = pns[i].neighbor_by_port[p];
                if (nb < num_nodes) {
                    hash ^= (uint64_t)p;
                    hash *= prime;
                    hash ^= (uint64_t)colors[nb];
                    hash *= prime;
                }
            }

            new_colors[i] = (uint32_t)(hash ^ (hash >> 32));
        }

        /* Check for changes */
        for (uint32_t i = 0; i < num_nodes; i++) {
            if (colors[i] != new_colors[i]) {
                changed = true;
                colors[i] = new_colors[i];
            }
        }

        if (!changed) break;
    }

    /* Count distinct final colors */
    uint32_t num_classes = 0;
    for (uint32_t i = 0; i < num_nodes; i++) {
        bool found = false;
        for (uint32_t j = 0; j < i; j++) {
            if (colors[i] == colors[j]) { found = true; break; }
        }
        if (!found) num_classes++;
    }

    free(colors);
    free(new_colors);
    return num_classes;
}

bool port_nodes_indistinguishable(const port_numbering_t *pns, uint32_t num_nodes,
                                  const bool *adjacency, uint32_t u, uint32_t v,
                                  uint32_t max_rounds) {
    if (!pns || !adjacency || u >= num_nodes || v >= num_nodes) return false;
    if (u == v) return true;

    /* Run the 1-WL refinement and check if u and v converge to the same color */
    uint32_t *colors = (uint32_t *)calloc(num_nodes, sizeof(uint32_t));
    uint32_t *new_colors = (uint32_t *)calloc(num_nodes, sizeof(uint32_t));
    if (!colors || !new_colors) {
        free(colors); free(new_colors);
        return false;
    }

    /* Initial: degree-based */
    for (uint32_t i = 0; i < num_nodes; i++) {
        colors[i] = pns[i].degree;
    }

    for (uint32_t iter = 0; iter < max_rounds; iter++) {
        for (uint32_t i = 0; i < num_nodes; i++) {
            uint64_t hash = 14695981039346656037ULL;
            uint64_t prime = 1099511628211ULL;
            hash ^= colors[i];
            hash *= prime;

            for (uint32_t p = 0; p < pns[i].degree; p++) {
                uint32_t nb = pns[i].neighbor_by_port[p];
                if (nb < num_nodes) {
                    hash ^= (uint64_t)p;
                    hash *= prime;
                    hash ^= (uint64_t)colors[nb];
                    hash *= prime;
                }
            }
            new_colors[i] = (uint32_t)(hash ^ (hash >> 32));
        }

        memcpy(colors, new_colors, num_nodes * sizeof(uint32_t));
    }

    bool result = (colors[u] == colors[v]);

    free(colors);
    free(new_colors);

    (void)adjacency;  /* adjacency is used indirectly via pns; suppress warning */
    return result;
}

uint32_t port_symmetry_breaking_number(const port_numbering_t *pns, uint32_t num_nodes,
                                       const bool *adjacency) {
    /* The symmetry-breaking number is exactly the number of distinct symmetry classes
       when UID information is removed. This is equivalent to the number of distinct
       infinite views (unlabeled by UID). */
    return port_count_symmetry_classes(pns, num_nodes, adjacency);
}

/* ================================================================
 * L2: Dual View Construction
 * ================================================================ */

int port_build_dual_view(const port_numbering_t *pns, uint32_t num_nodes,
                         uint32_t *out_neighbor, uint32_t *out_nport) {
    if (!pns || !out_neighbor || !out_nport) return -1;

    for (uint32_t i = 0; i < num_nodes; i++) {
        for (uint32_t p = 0; p < pns[i].degree; p++) {
            uint32_t nb = pns[i].neighbor_by_port[p];
            if (nb < num_nodes) {
                out_neighbor[i * LOCAL_MAX_DEGREE + p] = nb;
                /* Find which port the neighbor uses for the edge back to i */
                uint32_t nb_port = port_get_port_for_neighbor(&pns[nb], i);
                out_nport[i * LOCAL_MAX_DEGREE + p] = nb_port;
            } else {
                out_neighbor[i * LOCAL_MAX_DEGREE + p] = PORT_NONE;
                out_nport[i * LOCAL_MAX_DEGREE + p] = PORT_NONE;
            }
        }
    }

    return 0;
}
