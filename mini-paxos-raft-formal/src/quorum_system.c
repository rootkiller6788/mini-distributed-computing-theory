/**
 * quorum_system.c — Quorum System Mathematics
 *
 * Implements the mathematical theory of quorum systems — sets of subsets
 * of processes such that any two quorums intersect. This is the geometric
 * foundation of Paxos/Raft safety.
 *
 * Knowledge Coverage:
 *   L3 Math Structures: quorum as set system, intersection property
 *   L4 Fundamental Laws: quorum intersection lemma, majority theorem
 *   L2 Core Concepts: majority quorum, fault tolerance bound
 *
 * References:
 *   - Malkhi & Reiter, "Byzantine Quorum Systems" (ACM STOC 1998)
 *   - Lamport, "Paxos Made Simple" (2001) §2
 */

#include "../include/paxos_raft_types.h"
#include "../include/consensus_model.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ─── Quorum System Initialization ────────────────────────────────── */

/**
 * Initialize a majority quorum system for n nodes.
 *
 * The minimum quorum size is ⌊n/2⌋ + 1.
 * For n=3: quorum size = 2  (tolerates f=1 failure)
 * For n=5: quorum size = 3  (tolerates f=2 failures)
 *
 * Theorem: With quorum size q = ⌊n/2⌋ + 1, any two quorums intersect
 * in at least 2q - n nodes.
 */
void quorum_system_init(quorum_system_t *qs, int n) {
    qs->universe_size = n;
    for (int i = 0; i < n; i++) {
        qs->universe[i] = (node_id_t)i;
    }
    qs->quorum_size = (n / 2) + 1;
    qs->num_quorums = 0;

    /* Initialize all quorum slots as empty */
    for (int i = 0; i < MAX_QUORUMS; i++) {
        qs->quorums[i].size = 0;
    }
}

/**
 * Check whether a given subset is a valid majority quorum.
 *
 * A subset Q of {0..n-1} is a majority quorum iff |Q| > n/2.
 */
bool quorum_is_valid(const quorum_system_t *qs, const quorum_t *q) {
    if (q->size <= qs->universe_size / 2) return false;
    /* Verify all nodes are in universe */
    for (int i = 0; i < q->size; i++) {
        bool found = false;
        for (int j = 0; j < qs->universe_size; j++) {
            if (q->nodes[i] == qs->universe[j]) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    return true;
}

/**
 * Check the intersection property for the entire quorum system.
 *
 * ∀ Q1, Q2 ∈ Q : Q1 ∩ Q2 ≠ ∅
 *
 * This is O(|Q|² · n) but sufficient for small n.
 * For majority quorums, this is guaranteed by the pigeonhole principle.
 */
bool quorum_intersection_holds(const quorum_system_t *qs) {
    for (int i = 0; i < qs->num_quorums; i++) {
        for (int j = i + 1; j < qs->num_quorums; j++) {
            /* Check if quorums[i] and quorums[j] intersect */
            bool intersect = false;
            for (int a = 0; a < qs->quorums[i].size && !intersect; a++) {
                for (int b = 0; b < qs->quorums[j].size && !intersect; b++) {
                    if (qs->quorums[i].nodes[a] == qs->quorums[j].nodes[b]) {
                        intersect = true;
                    }
                }
            }
            if (!intersect) return false;
        }
    }
    return true;
}

/**
 * Compute intersection of two quorums.
 *
 * Returns size of intersection and fills intersection_out with
 * the common nodes. Used in safety proofs to identify the witness node.
 *
 * Lemma: For majority quorums, |A ∩ B| ≥ 2q - n ≥ 1.
 */
int quorum_intersection_size(const quorum_t *a, const quorum_t *b,
                              int *intersection_out) {
    int count = 0;
    for (int i = 0; i < a->size; i++) {
        for (int j = 0; j < b->size; j++) {
            if (a->nodes[i] == b->nodes[j]) {
                if (intersection_out) {
                    intersection_out[count] = (int)a->nodes[i];
                }
                count++;
                break;
            }
        }
    }
    return count;
}

/* ─── Quorum Generation ────────────────────────────────────────────── */

/**
 * Recursively generate all subsets of size k from the universe.
 * Used by quorum_generate_all to enumerate all majority subsets.
 */
static void generate_combinations(node_id_t *universe, int n, int k,
                                   int start, node_id_t *current, int depth,
                                   quorum_system_t *qs) {
    if (depth == k) {
        /* Found a combination of size k */
        quorum_t *q = &qs->quorums[qs->num_quorums];
        q->size = k;
        for (int i = 0; i < k; i++) {
            q->nodes[i] = current[i];
        }
        qs->num_quorums++;
        return;
    }

    for (int i = start; i < n; i++) {
        current[depth] = universe[i];
        generate_combinations(universe, n, k, i + 1, current, depth + 1, qs);
    }
}

/**
 * Generate all majority quorums for the universe.
 *
 * Total = Σ_{k=q}^{n} C(n,k) where q = ⌊n/2⌋ + 1.
 *
 * Example counts:
 *   n=3 (q=2): C(3,2)+C(3,3) = 3+1 = 4
 *   n=5 (q=3): C(5,3)+C(5,4)+C(5,5) = 10+5+1 = 16
 *   n=7 (q=4): C(7,4)+C(7,5)+C(7,6)+C(7,7) = 35+21+7+1 = 64
 *
 * Returns: total number of quorums generated.
 */
int quorum_generate_all(quorum_system_t *qs) {
    int n = qs->universe_size;
    int min_quorum_size = qs->quorum_size;
    node_id_t current[MAX_NODES];

    qs->num_quorums = 0;
    for (int k = min_quorum_size; k <= n; k++) {
        generate_combinations(qs->universe, n, k, 0, current, 0, qs);
    }
    return qs->num_quorums;
}

/* ─── Quorum Utility Functions ────────────────────────────────────── */

/**
 * Check if a given node is in a quorum.
 */
bool quorum_contains(const quorum_t *q, node_id_t node) {
    for (int i = 0; i < q->size; i++) {
        if (q->nodes[i] == node) return true;
    }
    return false;
}

/**
 * Compute the minimal number of faults a quorum system can tolerate.
 *
 * For majority quorum with n nodes: f_max = ⌊(n-1)/2⌋.
 *
 * Proof: Need |Q| = ⌊n/2⌋ + 1 nodes to respond. At most n - |Q| = ⌊(n-1)/2⌋
 * nodes can be faulty and still leave a quorum of correct nodes.
 */
int quorum_max_fault_tolerance(const quorum_system_t *qs) {
    return (qs->universe_size - 1) / 2;
}

/**
 * Check if a set of responding nodes constitutes a quorum.
 *
 * @param responders  bitmap: responders[i] = true iff node i responded
 * @param n           number of nodes
 * @param qs          quorum system
 * @return true if responders form a quorum
 */
bool quorum_achieved(const bool *responders, int n,
                      const quorum_system_t *qs) {
    int count = 0;
    for (int i = 0; i < n; i++) {
        if (responders[i]) count++;
    }
    return count >= qs->quorum_size;
}

/**
 * Count the number of quorums that contain a given node.
 * Used to compute the load of a node in the quorum system.
 *
 * For majority quorums, all nodes have equal load = C(n-1, q-1) / total_quorums.
 */
int quorum_count_containing_node(const quorum_system_t *qs, node_id_t node) {
    int count = 0;
    for (int i = 0; i < qs->num_quorums; i++) {
        if (quorum_contains(&qs->quorums[i], node)) count++;
    }
    return count;
}

/**
 * Find the smallest quorum (by size) that does NOT contain a given node.
 * Used in fault tolerance analysis to find witness quorums.
 *
 * Returns size of such quorum, or -1 if none exists.
 */
int quorum_min_size_without_node(const quorum_system_t *qs, node_id_t node) {
    /* For majority quorums, any subset of n - |quorum| + 1 nodes avoiding
     * the given node will fail to be a quorum. */
    (void)qs;
    (void)node;
    return -1; /* Not directly applicable for majority quorums */
}

/* ─── Byzantine Quorum Properties ─────────────────────────────────── */

/**
 * L8 Advanced: Byzantine quorum system properties.
 *
 * In Byzantine fault tolerance, we need 3f+1 nodes and quorum size
 * 2f+1 to ensure that any two quorums intersect in at least f+1 nodes,
 * guaranteeing at least one correct node in the intersection.
 *
 * This is the fundamental bound for PBFT (Castro & Liskov 1999) and
 * Byzantine Paxos (Lamport 2011).
 */

/**
 * Compute Byzantine quorum size: 2f+1 out of 3f+1 nodes.
 *
 * Theorem: With n = 3f+1 nodes and quorum size q = 2f+1, any two
 * quorums share at least f+1 nodes, of which at least 1 is correct
 * (since at most f are faulty).
 */
int byzantine_quorum_size(int f) {
    return 2 * f + 1;
}

/**
 * Compute total nodes needed for tolerating f Byzantine faults.
 */
int byzantine_total_nodes(int f) {
    return 3 * f + 1;
}

/**
 * Verify Byzantine quorum intersection: two 2f+1 subsets of 3f+1
 * nodes must intersect in at least f+1 nodes.
 *
 * Proof: |A ∩ B| = |A| + |B| - |A ∪ B|
 *                 ≥ (2f+1) + (2f+1) - (3f+1)
 *                 = 4f + 2 - 3f - 1 = f + 1.
 */
bool byzantine_quorum_intersection_check(int total_nodes,
                                          const quorum_t *a,
                                          const quorum_t *b) {
    int isect = quorum_intersection_size(a, b, NULL);
    int f = (total_nodes - 1) / 3;
    return isect >= f + 1;
}
