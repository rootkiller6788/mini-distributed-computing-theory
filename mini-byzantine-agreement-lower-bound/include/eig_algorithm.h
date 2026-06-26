/**
 * @file eig_algorithm.h
 * @brief Exponential Information Gathering (EIG) algorithm for Byzantine agreement.
 *
 * Covers L5 (Algorithms/Methods): The EIG algorithm is the canonical
 * solution for Byzantine agreement in the synchronous oral message model
 * when f < N/3. It builds a tree of depth t = f+1 where each node stores
 * the value reported along a specific relay chain.
 *
 * The EIG tree captures the recursive structure:
 *   At round k, every process sends its entire level-(k-1) subtree to all others.
 *
 * Decision rule (recursive majority):
 *   leaf:                return the stored value
 *   internal node:       return majority(children); if no majority, return ⊥
 *   root (PID = p):      return majority(children of root)
 *
 * This algorithm achieves the matching upper bound: if f < N/3,
 * Byzantine agreement is solvable in f+1 rounds.
 *
 * Reference: Bar-Noy, Dolev, Dwork, Strong (1987)
 *            Lynch (1996) "Distributed Algorithms" §6.2.2
 *            Attiya & Welch (2004) "Distributed Computing" §5.2
 *
 * Course mapping: MIT 6.852 Lecture 6, CMU 15-855, ETH 263-4650
 */

#ifndef EIG_ALGORITHM_H
#define EIG_ALGORITHM_H

#include <stdint.h>
#include <stdbool.h>
#include "byzantine_agreement.h"

/* ================================================================
 * L5: EIG Tree Data Structure
 * ================================================================ */

/**
 * @brief A node in the EIG (Exponential Information Gathering) tree.
 *
 * The tree has N^(t+1) nodes in the worst case. Each node is identified
 * by a path (sequence of process IDs) representing the relay chain.
 *
 * For example, the path [2, 5, 1] means:
 *   "Process 1 told me that process 5 told him that process 2 said value v."
 */
typedef struct eig_node_t {
    int32_t             path[BA_MAX_PROCESSES]; /**< Relay chain (PID sequence) */
    int32_t             path_len;               /**< Length of the path */
    int32_t             stored_value;           /**< Value stored at this node */
    int32_t             resolved_value;         /**< After recursive majority */
    int32_t             num_children;           /**< Number of child nodes */
    struct eig_node_t **children;               /**< Pointer array to children */
    struct eig_node_t  *parent;                 /**< Parent node (NULL for root) */
    int32_t             level;                  /**< Depth in tree (0 = root) */
    bool                is_leaf;                /**< True if level == t */
} eig_node_t;

/**
 * @brief The full EIG tree for one process.
 *
 * Each correct process p constructs its own EIG tree of depth t = f+1.
 * After t+1 rounds, process p applies the recursive majority rule
 * to compute its decision.
 */
typedef struct {
    eig_node_t *root;            /**< Root node: value that p itself proposed */
    int32_t     depth;           /**< Tree depth = f+1 = t+1 */
    int32_t     N;               /**< Number of processes */
    int32_t     f;               /**< Fault tolerance */
    int32_t     owner_pid;       /**< Which process owns this tree */
    int32_t     total_nodes;     /**< Total nodes in the tree */
    int32_t     decision;        /**< Final decision after recursive majority */
    bool        has_decided;     /**< Whether decision has been made */
} eig_tree_t;

/* ================================================================
 * L5: Recursive Majority Rule
 *
 * The key insight of the EIG algorithm:
 *
 *   majority(S) = v  if v appears > |S|/2 times in S
 *                = ⊥ otherwise
 *
 *   resolve(node):
 *     if node is leaf:
 *       return node.stored_value
 *     else:
 *       child_values = []
 *       for each child c of node (one per possible sender):
 *         if c exists:
 *           child_values.append(resolve(c))
 *         else:
 *           child_values.append(⊥)
 *       return majority(child_values)
 *
 * The decision of process p is resolve(root of p's tree).
 *
 * Correctness depends on the property that for any two correct processes
 * p and q, the subtrees at nodes with paths ending in correct processes
 * are identical.
 * ================================================================ */

/* ================================================================
 * L6: Canonical Problem — 3-process, 1-fault instance
 * ================================================================ */

/**
 * @brief Built-in test case: N=3, f=1, oral messages.
 *
 * When N=3 and f=1, Byzantine agreement is provably impossible
 * (LSP82 Theorem 1). But when N=4 and f=1, it becomes possible using
 * the EIG algorithm with 2 rounds. This structure encodes both cases
 * for demonstration.
 */
typedef struct {
    int32_t N;
    int32_t f;
    int32_t initial_values[BA_MAX_PROCESSES];
    int32_t faulty_pids[BA_MAX_PROCESSES];
    int32_t num_faulty;
    int32_t expected_decision;      /**< -1 if no agreement possible */
    bool    agreement_possible;
} eig_canonical_case_t;

/* ================================================================
 * L5: API
 * ================================================================ */

/**
 * @brief Create and initialize an EIG tree for process pid.
 *
 * Allocates the tree structure of depth t = f+1 with branching factor N.
 * The root stores pid's own initial value.
 *
 * @param tree     Pointer to tree to initialize.
 * @param N        Number of processes.
 * @param f        Maximum faulty processes.
 * @param pid      Owner process ID.
 * @param init_val Initial value of the owner.
 * @return 0 on success, -1 on error.
 *
 * Complexity: O(N^t) — exponential in t
 * Reference: Lynch §6.2.2
 */
int  eig_tree_init(eig_tree_t *tree, int32_t N, int32_t f,
                   int32_t pid, int32_t init_val);

/**
 * @brief Insert a value into the EIG tree at the node specified by a path.
 *
 * During each round, process p receives values from others and stores
 * them at the appropriate path in its tree. The path encodes the relay
 * chain: "sender told me that ...".
 *
 * @param tree    The EIG tree.
 * @param path    Relay chain (last element is the direct sender).
 * @param path_len Length of the path.
 * @param value   The value to store.
 * @return 0 on success, -1 if path invalid.
 *
 * Complexity: O(path_len)
 */
int  eig_tree_insert(eig_tree_t *tree, const int32_t *path,
                     int32_t path_len, int32_t value);

/**
 * @brief Apply the recursive majority rule to compute the decision.
 *
 * Traverses the entire tree bottom-up, computing resolved values.
 * The decision is resolve(root).
 *
 * @param tree The EIG tree.
 * @param decision Output parameter for the decision value.
 * @return 0 on success, -1 on error.
 *
 * Complexity: O(N^t)
 * Theorem: LSP82 — if f < N/3, all correct processes decide the same value.
 */
int  eig_tree_resolve(eig_tree_t *tree, int32_t *decision);

/**
 * @brief Look up the value stored at a given path in the tree.
 * @param tree The EIG tree.
 * @param path Relay chain.
 * @param path_len Length of path.
 * @param value Output parameter for stored value.
 * @return 0 if found, -1 if path doesn't exist.
 *
 * Complexity: O(path_len)
 */
int  eig_tree_lookup(const eig_tree_t *tree, const int32_t *path,
                     int32_t path_len, int32_t *value);

/**
 * @brief Compute the majority value from an array of values.
 *
 * majority(S) = v if count(v) > |S|/2, else -1 (no majority).
 *
 * @param values Array of values.
 * @param n      Number of elements.
 * @return The majority value, or -1 if no strict majority.
 *
 * Complexity: O(n)
 */
int32_t eig_majority(const int32_t *values, int32_t n);

/**
 * @brief Simulate one round of the EIG protocol for all processes.
 *
 * Each process sends its level-(r-1) subtree to all other processes.
 * Correct processes relay values faithfully; faulty processes may
 * send arbitrary values (specified by the fault_behavior callback).
 *
 * @param sys             The system state.
 * @param round           Current round number (0-indexed).
 * @param fault_behavior  Callback: given (faulty_pid, target_pid, expected_value),
 *                        returns the value the faulty process actually sends.
 *                        NULL means faulty processes send random values.
 * @return 0 on success, -1 on error.
 *
 * Complexity: O(N^t * N) per round
 */
int  eig_simulate_round(ba_system_t *sys, int32_t round,
                        int32_t (*fault_behavior)(int32_t, int32_t, int32_t));

/**
 * @brief Run the full EIG protocol to completion.
 *
 * Executes t+1 rounds of message exchange, then all processes apply
 * recursive majority to decide.
 *
 * @param sys              The system to run.
 * @param fault_behavior   Faulty process behavior callback.
 * @param result           Output: agreement verification result.
 * @return 0 on success, -1 on error.
 *
 * Complexity: O((t+1) * N^t * N)
 */
int  eig_run_protocol(ba_system_t *sys,
                      int32_t (*fault_behavior)(int32_t, int32_t, int32_t),
                      ba_result_t *result);

/**
 * @brief Verify that the EIG tree invariant holds for correct processes.
 *
 * Invariant: For any two correct processes p and q, the subtrees rooted
 * at nodes corresponding to paths of correct processes are identical.
 * This is the key lemma for EIG correctness.
 *
 * @param tree_p EIG tree of process p.
 * @param tree_q EIG tree of process q.
 * @param path   Path to the subtree root to compare.
 * @param path_len Length of path.
 * @return true if the subtrees are identical.
 *
 * Complexity: O(N^(t - path_len))
 * Lemma: "Correct processes agree on values from correct senders"
 */
bool eig_verify_subtree_equality(const eig_tree_t *tree_p,
                                  const eig_tree_t *tree_q,
                                  const int32_t *path, int32_t path_len);

/**
 * @brief Destroy an EIG tree and free all memory.
 * @param tree Tree to destroy.
 */
void eig_tree_destroy(eig_tree_t *tree);

/**
 * @brief Pretty-print an EIG tree for debugging/visualization.
 * @param tree  Tree to print.
 * @param depth Maximum depth to print.
 */
void eig_tree_print(const eig_tree_t *tree, int32_t depth);

/**
 * @brief Built-in canonical test: N=4, f=1, EIG succeeds.
 * @return 0 if agreement holds, -1 otherwise.
 */
int  eig_canonical_n4_f1_demo(void);

/**
 * @brief Validate N=7, f=2 threshold is correct.
 * @return 0 if threshold is 2, -1 otherwise.
 */
int  eig_canonical_n7_f2_validate(void);

#endif /* EIG_ALGORITHM_H */
