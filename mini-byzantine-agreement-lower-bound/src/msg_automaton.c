/**
 * @file msg_automaton.c
 * @brief Implementation of the message automaton formal model for
 *        distributed agreement protocols.
 *
 * Knowledge coverage:
 *   L3: Automaton tuple (Q, q0, Σ_in, Σ_out, δ, λ) implementation
 *   L3: Configuration and execution structures
 *   L3: Bivalence computation (for FLP)
 *   L5: Automaton simulation with fault injection
 *
 * Reference: Lynch (1996) "Distributed Algorithms" Ch. 5-6
 *            Fischer, Lynch, Paterson (1985)
 */

#include "msg_automaton.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ================================================================
 * L3: Automaton Initialization
 * ================================================================ */

/**
 * @brief Default transition function: simple relay.
 *
 * A correct process updates its estimate if it receives a new value
 * from a majority of received messages.
 */
static int32_t default_delta_oral(const automaton_state_t *q,
                                   const ba_message_t *incoming,
                                   int32_t num_incoming,
                                   automaton_state_t *q_next)
{
    if (!q || !q_next) return -1;

    memcpy(q_next, q, sizeof(*q_next));
    q_next->received_count = 0;

    if (num_incoming == 0) return 0;

    /* Count votes from incoming messages */
    int32_t vote_counts[BA_MAX_VALUES] = {0};
    int32_t total_votes = 0;

    for (int32_t i = 0; i < num_incoming; i++) {
        if (incoming[i].value >= 0 && incoming[i].value < BA_MAX_VALUES) {
            vote_counts[incoming[i].value]++;
            total_votes++;
        }
    }

    /* Find majority */
    int32_t majority = -1;
    int32_t threshold = total_votes / 2;
    for (int32_t v = 0; v < BA_MAX_VALUES; v++) {
        if (vote_counts[v] > threshold) {
            majority = v;
            break;
        }
    }

    if (majority >= 0) {
        q_next->estimate = majority;
    }
    /* If no majority, keep current estimate */

    q_next->round = q->round + 1;
    q_next->received = NULL;  /* Will be set by caller */
    q_next->received_count = num_incoming;

    return 0;
}

/**
 * @brief Default output function: broadcast current estimate.
 */
static int32_t default_lambda_oral(const automaton_state_t *q,
                                    ba_message_t *outgoing,
                                    int32_t max_outgoing)
{
    if (!q || !outgoing || max_outgoing < 1) return 0;

    /* In the simplest case, just broadcast the current estimate.
     * The caller specifies how many messages to generate. */
    outgoing[0].value = q->estimate;
    outgoing[0].sender = q->pid;
    outgoing[0].round = q->round;

    return 1;  /* One message generated */
}

int automaton_init(msg_automaton_t *ma, int32_t N, int32_t f,
                   int32_t pid, int32_t init_val,
                   transition_function_t delta, output_function_t lambda)
{
    if (!ma || N < 1 || N > BA_MAX_PROCESSES || f < 0 || pid < 0 || pid >= N) {
        return -1;
    }

    memset(ma, 0, sizeof(*ma));
    ma->N = N;
    ma->f = f;

    /* Initialize state */
    ma->state.pid = pid;
    ma->state.round = 0;
    ma->state.estimate = init_val;
    ma->state.decided = false;
    ma->state.decision = -1;
    ma->state.tree_depth = f + 1;
    ma->state.tree_width = N;

    /* Allocate EIG-style value tree */
    ma->state.value_tree = (int32_t **)calloc((size_t)(f + 2), sizeof(int32_t *));
    if (!ma->state.value_tree) return -1;
    for (int32_t d = 0; d <= f + 1; d++) {
        /* Level d has N^d nodes (simplified: allocate N^(f+1) for simplicity) */
        int32_t level_size = 1;
        for (int32_t j = 0; j < d; j++) level_size *= N;
        ma->state.value_tree[d] = (int32_t *)calloc((size_t)level_size, sizeof(int32_t));
        if (!ma->state.value_tree[d]) {
            for (int32_t k = 0; k < d; k++) free(ma->state.value_tree[k]);
            free(ma->state.value_tree);
            ma->state.value_tree = NULL;
            return -1;
        }
        /* Initialize all to -1 (unknown) */
        for (int32_t j = 0; j < level_size; j++) {
            ma->state.value_tree[d][j] = -1;
        }
    }
    /* Root stores init_val */
    ma->state.value_tree[0][0] = init_val;

    /* Set transition and output functions */
    if (delta.apply) {
        ma->delta = delta;
    } else {
        ma->delta.apply = default_delta_oral;
        ma->delta.rule_name = "DefaultOral";
        ma->delta.family = TRANSITION_ORAL;
    }

    if (lambda.generate) {
        ma->lambda = lambda;
    } else {
        ma->lambda.generate = default_lambda_oral;
        ma->lambda.output_name = "DefaultBroadcast";
    }

    /* Save initial state */
    memcpy(&ma->initial_state, &ma->state, sizeof(ma->state));

    return 0;
}

/* ================================================================
 * L3: Automaton Step
 * ================================================================ */

int automaton_step(msg_automaton_t *ma,
                   const ba_message_t *incoming, int32_t num_in,
                   ba_message_t *outgoing, int32_t max_out,
                   int32_t *num_out)
{
    if (!ma || !num_out) return -1;

    automaton_state_t next_state;
    int ret = ma->delta.apply(&ma->state, incoming, num_in, &next_state);
    if (ret != 0) return ret;

    /* Copy received messages for logging */
    if (next_state.received) {
        free(next_state.received);
    }
    next_state.received = NULL;
    next_state.received_count = 0;
    if (num_in > 0 && incoming) {
        next_state.received = (ba_message_t *)malloc(
            (size_t)num_in * sizeof(ba_message_t));
        if (next_state.received) {
            memcpy(next_state.received, incoming,
                   (size_t)num_in * sizeof(ba_message_t));
            next_state.received_count = num_in;
        }
    }

    /* Generate outgoing messages */
    int32_t generated = 0;
    if (max_out > 0 && outgoing && ma->lambda.generate) {
        generated = ma->lambda.generate(&ma->state, outgoing, max_out);
    }
    *num_out = generated;

    /* Update state */
    /* Preserve the value_tree pointer */
    int32_t **saved_tree = ma->state.value_tree;
    int32_t saved_depth = ma->state.tree_depth;
    int32_t saved_width = ma->state.tree_width;
    ba_message_t *saved_sent = ma->state.sent;
    int32_t saved_sent_count = ma->state.sent_count;

    memcpy(&ma->state, &next_state, sizeof(ma->state));
    ma->state.value_tree = saved_tree;
    ma->state.tree_depth = saved_depth;
    ma->state.tree_width = saved_width;
    ma->state.sent = saved_sent;
    ma->state.sent_count = saved_sent_count;

    return 0;
}

/* ================================================================
 * L3: Bivalence Classification
 * ================================================================ */

automaton_valence_t automaton_classify_valence(const automaton_config_t *cfg,
                                                int32_t N, uint16_t fault_mask)
{
    if (!cfg || N < 1) return AUTOMATON_BIVALENT;

    /* For the bivalence analysis, we classify the configuration by
     * checking if all reachable terminal configurations agree on the
     * decision value. */

    /* Count how many correct processes have decided which values */
    int32_t decided_0 = 0, decided_1 = 0;

    for (int32_t i = 0; i < N; i++) {
        if (fault_mask & (1u << i)) continue;  /* Skip faulty */
        if (cfg->states[i].decided) {
            if (cfg->states[i].decision == 0) decided_0++;
            else if (cfg->states[i].decision == 1) decided_1++;
        }
    }

    int32_t total_correct = N;
    for (int32_t i = 0; i < N; i++) {
        if (fault_mask & (1u << i)) total_correct--;
    }

    /* If all correct decided 0: 0-valent */
    if (decided_0 == total_correct && decided_1 == 0) {
        return AUTOMATON_0_VALENT;
    }
    /* If all correct decided 1: 1-valent */
    if (decided_1 == total_correct && decided_0 == 0) {
        return AUTOMATON_1_VALENT;
    }
    /* Otherwise: bivalent (some correct undecided, or disagreement) */
    return AUTOMATON_BIVALENT;
}

bool automaton_config_has_agreement(const automaton_config_t *cfg,
                                     int32_t N, uint16_t fault_mask)
{
    if (!cfg) return false;

    int32_t agreed_value = -1;
    bool first = true;

    for (int32_t i = 0; i < N; i++) {
        if (fault_mask & (1u << i)) continue;
        if (!cfg->states[i].decided) return false;  /* Not all decided */
        if (first) {
            agreed_value = cfg->states[i].decision;
            first = false;
        } else if (cfg->states[i].decision != agreed_value) {
            return false;
        }
    }

    return true;
}

bool automaton_config_is_terminal(const automaton_config_t *cfg,
                                   int32_t N, uint16_t fault_mask)
{
    if (!cfg) return false;

    for (int32_t i = 0; i < N; i++) {
        if (fault_mask & (1u << i)) continue;
        if (!cfg->states[i].decided) return false;
    }

    return true;
}

/* ================================================================
 * L3: Execution
 * ================================================================ */

int automaton_execute(automaton_execution_t *exec,
                      const automaton_config_t *init_cfg,
                      int32_t N, int32_t f, int32_t rounds,
                      uint16_t fault_mask)
{
    if (!exec || !init_cfg || N < 1 || rounds < 0) return -1;

    exec->capacity = (size_t)(rounds + 2);
    exec->configs = (automaton_config_t *)calloc(
        exec->capacity, sizeof(automaton_config_t));
    if (!exec->configs) return -1;

    /* Store initial configuration */
    memcpy(&exec->configs[0], init_cfg, sizeof(*init_cfg));
    exec->configs[0].config_id = 0;
    exec->length = 1;

    /* Simulate round by round */
    for (int32_t r = 0; r < rounds; r++) {
        automaton_config_t *prev = &exec->configs[exec->length - 1];
        automaton_config_t *next = &exec->configs[exec->length];

        memcpy(next, prev, sizeof(*prev));
        next->config_id = exec->length;

        /* Each correct process takes a step */
        for (int32_t pid = 0; pid < N; pid++) {
            if (fault_mask & (1u << pid)) continue;

            msg_automaton_t ma;
            transition_function_t delta = {
                .apply = default_delta_oral,
                .rule_name = "OralDefault",
                .family = TRANSITION_ORAL
            };
            output_function_t lambda = {
                .generate = default_lambda_oral,
                .output_name = "Broadcast"
            };

            if (automaton_init(&ma, N, f, pid,
                               prev->states[pid].estimate,
                               delta, lambda) != 0) {
                continue;
            }

            /* Collect messages from other processes in the previous round */
            ba_message_t incoming_buf[BA_MAX_PROCESSES];
            int32_t num_in = 0;
            for (int32_t sender = 0; sender < N; sender++) {
                if (sender == pid) continue;
                incoming_buf[num_in].sender = sender;
                incoming_buf[num_in].value = prev->states[sender].estimate;
                incoming_buf[num_in].round = r;
                num_in++;
            }

            ba_message_t outgoing_buf[BA_MAX_PROCESSES];
            int32_t num_out = 0;
            automaton_step(&ma, incoming_buf, num_in,
                          outgoing_buf, BA_MAX_PROCESSES, &num_out);

            /* Update configuration state */
            next->states[pid] = ma.state;
            next->states[pid].value_tree = NULL;  /* Don't carry pointer */

            automaton_destroy(&ma);
        }

        exec->length++;
    }

    return 0;
}

/* ================================================================
 * L3: Resource Management
 * ================================================================ */

void automaton_destroy(msg_automaton_t *ma)
{
    if (!ma) return;
    if (ma->state.value_tree) {
        for (int32_t d = 0; d <= ma->state.tree_depth; d++) {
            free(ma->state.value_tree[d]);
        }
        free(ma->state.value_tree);
        ma->state.value_tree = NULL;
    }
    free(ma->state.received);
    free(ma->state.sent);
    memset(ma, 0, sizeof(*ma));
}

void automaton_execution_destroy(automaton_execution_t *exec)
{
    if (!exec) return;
    free(exec->configs);
    memset(exec, 0, sizeof(*exec));
}
