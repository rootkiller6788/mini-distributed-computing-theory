/**
 * @file flp_config.c
 * @brief Configuration management for the FLP impossibility proof.
 *
 * A configuration C = (s[0],...,s[N-1], M) is the global system state.
 * This module implements creation, cloning, hashing, comparison,
 * valence classification, and configuration graph enumeration.
 *
 * Core mathematical structure:
 *   Config = StateVector x MessageMultiset
 *
 * Reference: FLP (1985) Section 2, Definition 1.
 */

#include "flp_config.h"
#include "flp_message.h"
#include "flp_protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Configuration Initialization ===== */

void flp_config_init(flp_configuration *cfg, int32_t n, int32_t config_id)
{
    if (cfg == NULL || n <= 0 || n > FLP_MAX_PROCESSES) return;
    memset(cfg, 0, sizeof(*cfg));
    cfg->num_processes = n;
    cfg->config_id = config_id;
    cfg->valence = FLP_VALENCE_UNKNOWN;
    cfg->depth = 0;
    cfg->num_messages = 0;

    for (int32_t i = 0; i < n; i++) {
        cfg->processes[i].pid = i;
        cfg->processes[i].status = FLP_PROCESS_CORRECT;
        cfg->processes[i].decision = FLP_UNDECIDED;
        cfg->processes[i].input_value = 0;
        cfg->processes[i].num_state_vars = 0;
        cfg->processes[i].current_round = 0;
        cfg->processes[i].msg_count = 0;
        cfg->processes[i].step_count = 0;
        cfg->processes[i].has_decided = false;
    }
}

void flp_config_init_with_inputs(flp_configuration *cfg, int32_t n,
                                  const int32_t *inputs, int32_t cid)
{
    flp_config_init(cfg, n, cid);
    if (inputs != NULL) {
        for (int32_t i = 0; i < n && i < FLP_MAX_PROCESSES; i++) {
            cfg->processes[i].input_value = (inputs[i] != 0) ? 1 : 0;
        }
    }
}

/* ===== Configuration Cloning ===== */

void flp_config_clone(flp_configuration *dst, const flp_configuration *src)
{
    if (dst == NULL || src == NULL) return;
    memcpy(dst, src, sizeof(flp_configuration));
}

/* ===== Configuration Equality ===== */

bool flp_config_equal(const flp_configuration *a, const flp_configuration *b)
{
    if (a == NULL || b == NULL) return (a == b);
    if (a->num_processes != b->num_processes) return false;
    if (a->num_messages != b->num_messages) return false;

    /* Compare process states */
    for (int32_t i = 0; i < a->num_processes; i++) {
        const flp_process_state *pa = &a->processes[i];
        const flp_process_state *pb = &b->processes[i];
        if (pa->pid != pb->pid) return false;
        if (pa->status != pb->status) return false;
        if (pa->decision != pb->decision) return false;
        if (pa->input_value != pb->input_value) return false;
        if (pa->current_round != pb->current_round) return false;
        if (pa->has_decided != pb->has_decided) return false;
        if (pa->num_state_vars != pb->num_state_vars) return false;
        for (int32_t j = 0; j < pa->num_state_vars; j++) {
            if (pa->state_vars[j] != pb->state_vars[j]) return false;
        }
    }

    /* Compare message buffers (as multisets -- order independent) */
    /* Use a simple marking approach: for each msg in a, find match in b */
    bool matched_b[FLP_MAX_MESSAGES];
    memset(matched_b, 0, sizeof(matched_b));

    for (int32_t i = 0; i < a->num_messages; i++) {
        bool found = false;
        for (int32_t j = 0; j < b->num_messages; j++) {
            if (matched_b[j]) continue;
            if (a->messages[i].sender == b->messages[j].sender &&
                a->messages[i].receiver == b->messages[j].receiver &&
                a->messages[i].type == b->messages[j].type &&
                a->messages[i].round == b->messages[j].round &&
                a->messages[i].value == b->messages[j].value) {
                matched_b[j] = true;
                found = true;
                break;
            }
        }
        if (!found) return false;
    }

    return true;
}

/* ===== Configuration Hashing ===== */

uint32_t flp_config_hash(const flp_configuration *cfg)
{
    if (cfg == NULL) return 0;
    uint32_t hash = 5381; /* djb2 initial value */

    /* Hash process states */
    for (int32_t i = 0; i < cfg->num_processes; i++) {
        const flp_process_state *ps = &cfg->processes[i];
        hash = ((hash << 5) + hash) + (uint32_t)ps->pid;
        hash = ((hash << 5) + hash) + (uint32_t)ps->status;
        hash = ((hash << 5) + hash) + (uint32_t)ps->decision;
        hash = ((hash << 5) + hash) + (uint32_t)ps->input_value;
        hash = ((hash << 5) + hash) + (uint32_t)ps->has_decided;
        for (int32_t j = 0; j < ps->num_state_vars; j++) {
            hash = ((hash << 5) + hash) + (uint32_t)ps->state_vars[j];
        }
    }

    /* Hash message buffer */
    for (int32_t i = 0; i < cfg->num_messages; i++) {
        const flp_message *m = &cfg->messages[i];
        hash = ((hash << 5) + hash) + (uint32_t)m->sender;
        hash = ((hash << 5) + hash) + (uint32_t)m->receiver;
        hash = ((hash << 5) + hash) + (uint32_t)m->type;
        hash = ((hash << 5) + hash) + (uint32_t)m->value;
    }

    return hash;
}

/* ===== Valence Cache Management ===== */

void flp_config_invalidate_valence(flp_configuration *cfg)
{
    if (cfg != NULL) {
        cfg->valence = FLP_VALENCE_UNKNOWN;
    }
}

/* ===== Configuration Classification ===== */

bool flp_config_is_initial(const flp_configuration *cfg)
{
    if (cfg == NULL) return false;
    /* Initial config: empty message buffer, all processes at round 0,
       no steps taken, not decided. */
    if (cfg->num_messages != 0) return false;
    for (int32_t i = 0; i < cfg->num_processes; i++) {
        if (cfg->processes[i].step_count != 0) return false;
        if (cfg->processes[i].has_decided) return false;
        if (cfg->processes[i].current_round != 0) return false;
    }
    return true;
}

bool flp_config_is_decided(const flp_configuration *cfg)
{
    return flp_config_has_decision(cfg);
}

flp_decision_value flp_config_decision(const flp_configuration *cfg)
{
    if (cfg == NULL) return FLP_UNDECIDED;
    for (int32_t i = 0; i < cfg->num_processes; i++) {
        if (flp_process_is_correct(&cfg->processes[i]) &&
            cfg->processes[i].has_decided) {
            return cfg->processes[i].decision;
        }
    }
    return FLP_UNDECIDED;
}

/* ===== Configuration Graph Exploration (BFS) ===== */

/**
 * Internal: visited set for BFS, using simple linear search.
 * For small state spaces, this suffices; for larger, a hash table
 * would be needed.
 */
typedef struct {
    flp_configuration  *configs;
    int32_t             count;
    int32_t             capacity;
} flp_visited_set;

static void visited_init(flp_visited_set *vs, int32_t cap)
{
    vs->configs = (flp_configuration*)malloc((size_t)cap * sizeof(flp_configuration));
    vs->count = 0;
    vs->capacity = (vs->configs != NULL) ? cap : 0;
}

static void visited_free(flp_visited_set *vs)
{
    free(vs->configs);
    vs->configs = NULL;
    vs->count = 0;
    vs->capacity = 0;
}

static bool visited_contains(const flp_visited_set *vs,
                              const flp_configuration *cfg)
{
    for (int32_t i = 0; i < vs->count; i++) {
        if (flp_config_equal(&vs->configs[i], cfg)) return true;
    }
    return false;
}

static bool visited_add(flp_visited_set *vs, const flp_configuration *cfg)
{
    if (vs->count >= vs->capacity) return false;
    flp_config_clone(&vs->configs[vs->count], cfg);
    vs->count++;
    return true;
}

/* ===== BFS for Reachability ===== */

int32_t flp_config_reachable_count(const flp_configuration *start,
                                    int32_t max_depth)
{
    if (start == NULL || max_depth < 0) return 0;

    /* Queue for BFS */
    int32_t queue_cap = 4096;
    flp_configuration *queue = (flp_configuration*)malloc(
        (size_t)queue_cap * sizeof(flp_configuration));
    if (queue == NULL) return 0;

    int32_t front = 0, back = 0;
    flp_visited_set visited;
    visited_init(&visited, queue_cap);

    flp_config_clone(&queue[back++], start);
    visited_add(&visited, start);

    while (front < back && visited.count < queue_cap) {
        flp_configuration current;
        flp_config_clone(&current, &queue[front++]);

        if (current.depth >= max_depth) continue;

        /* Generate all possible next configurations */
        for (int32_t pid = 0; pid < current.num_processes; pid++) {
            if (!flp_process_is_correct(&current.processes[pid])) continue;

            int32_t pending = flp_msg_buffer_pending_count(&current, pid);
            if (pending == 0) continue;

            /* Get all pending messages for this process */
            flp_message msgs[16];
            int32_t n_msgs = flp_msg_buffer_peek(&current, pid, msgs, 16);

            for (int32_t mi = 0; mi < n_msgs; mi++) {
                /* Create next config by delivering message mi to pid */
                flp_configuration next;
                flp_config_clone(&next, &current);

                /* Find and deliver the specific message */
                flp_message delivered;
                bool found = false;
                for (int32_t mi2 = 0; mi2 < next.num_messages; mi2++) {
                    if (next.messages[mi2].sender == msgs[mi].sender &&
                        next.messages[mi2].receiver == msgs[mi].receiver &&
                        next.messages[mi2].type == msgs[mi].type &&
                        next.messages[mi2].value == msgs[mi].value) {
                        /* Remove message from buffer (swap with last) */
                        delivered = next.messages[mi2];
                        next.messages[mi2] = next.messages[next.num_messages - 1];
                        next.num_messages--;
                        found = true;
                        break;
                    }
                }
                if (!found) continue;

                /* Apply protocol step */
                flp_protocol_desc desc;
                flp_protocol_desc_init(&desc, FLP_PROTO_FLOOD_SET,
                                        next.num_processes, 1);
                flp_step_result step_res;
                flp_protocol_step(&desc, &next.processes[pid],
                                  &delivered, &step_res);
                next.processes[pid] = step_res.new_state;

                /* Add outgoing messages */
                for (int32_t oi = 0; oi < step_res.num_outgoing; oi++) {
                    flp_msg_buffer_send(&next, &step_res.outgoing[oi]);
                }

                next.depth = current.depth + 1;
                flp_config_invalidate_valence(&next);

                if (!visited_contains(&visited, &next)) {
                    if (back < queue_cap) {
                        flp_config_clone(&queue[back++], &next);
                    }
                    visited_add(&visited, &next);
                }
            }
        }
    }

    int32_t result = visited.count;
    visited_free(&visited);
    free(queue);
    return result;
}

/* ===== Valence Classification ===== */

/**
 * Determine valence by exploring reachable configurations and checking
 * what decision values are reachable.
 */
flp_valence flp_config_classify_valence(flp_configuration *cfg,
                                         int32_t max_depth)
{
    if (cfg == NULL) return FLP_VALENCE_UNKNOWN;

    bool can_0 = flp_config_can_decide_0(cfg, max_depth);
    bool can_1 = flp_config_can_decide_1(cfg, max_depth);

    if (can_0 && can_1) return FLP_VALENCE_BIVALENT;
    if (can_0) return FLP_VALENCE_0;
    if (can_1) return FLP_VALENCE_1;
    return FLP_VALENCE_UNKNOWN;
}

/**
 * BFS search for a deciding configuration with a specific decision value.
 */
static bool search_for_decision(const flp_configuration *start,
                                 flp_decision_value target,
                                 int32_t max_depth)
{
    if (start == NULL || max_depth < 0) return false;

    int32_t queue_cap = 2048;
    flp_configuration *queue = (flp_configuration*)malloc(
        (size_t)queue_cap * sizeof(flp_configuration));
    if (queue == NULL) return false;

    int32_t front = 0, back = 0;
    flp_visited_set visited;
    visited_init(&visited, queue_cap);

    flp_config_clone(&queue[back++], start);
    visited_add(&visited, start);

    bool found = false;

    while (front < back && !found && visited.count < queue_cap) {
        flp_configuration current;
        flp_config_clone(&current, &queue[front++]);

        /* Check if we reached the target decision */
        if (current.processes[0].has_decided &&
            current.processes[0].decision == target) {
            found = true;
            break;
        }

        if (current.depth >= max_depth) continue;

        /* Generate successors (same as reachable_count BFS) */
        for (int32_t pid = 0; pid < current.num_processes && !found; pid++) {
            if (!flp_process_is_correct(&current.processes[pid])) continue;

            int32_t pending = flp_msg_buffer_pending_count(&current, pid);
            if (pending == 0) continue;

            flp_message msgs[16];
            int32_t n_msgs = flp_msg_buffer_peek(&current, pid, msgs, 16);

            for (int32_t mi = 0; mi < n_msgs && !found; mi++) {
                flp_configuration next;
                flp_config_clone(&next, &current);

                flp_message delivered;
                bool msg_found = false;
                for (int32_t mi2 = 0; mi2 < next.num_messages; mi2++) {
                    if (next.messages[mi2].sender == msgs[mi].sender &&
                        next.messages[mi2].receiver == msgs[mi].receiver &&
                        next.messages[mi2].type == msgs[mi].type &&
                        next.messages[mi2].value == msgs[mi].value) {
                        delivered = next.messages[mi2];
                        next.messages[mi2] = next.messages[next.num_messages - 1];
                        next.num_messages--;
                        msg_found = true;
                        break;
                    }
                }
                if (!msg_found) continue;

                flp_protocol_desc desc;
                flp_protocol_desc_init(&desc, FLP_PROTO_FLOOD_SET,
                                        next.num_processes, 1);
                flp_step_result step_res;
                flp_protocol_step(&desc, &next.processes[pid],
                                  &delivered, &step_res);
                next.processes[pid] = step_res.new_state;

                for (int32_t oi = 0; oi < step_res.num_outgoing; oi++) {
                    flp_msg_buffer_send(&next, &step_res.outgoing[oi]);
                }

                next.depth = current.depth + 1;
                flp_config_invalidate_valence(&next);

                if (!visited_contains(&visited, &next)) {
                    if (back < queue_cap) {
                        flp_config_clone(&queue[back++], &next);
                    }
                    visited_add(&visited, &next);
                }
            }
        }
    }

    visited_free(&visited);
    free(queue);
    return found;
}

bool flp_config_can_decide_0(flp_configuration *cfg, int32_t max_depth)
{
    return search_for_decision(cfg, FLP_DECIDE_0, max_depth);
}

bool flp_config_can_decide_1(flp_configuration *cfg, int32_t max_depth)
{
    return search_for_decision(cfg, FLP_DECIDE_1, max_depth);
}

/* ===== Schedule Finding ===== */

/**
 * BFS that records the path to a target decision, outputting the schedule.
 */
static int32_t search_schedule_to_decision(const flp_configuration *start,
                                            flp_decision_value target,
                                            int32_t max_depth,
                                            flp_schedule *out)
{
    if (start == NULL || out == NULL || max_depth < 0) return -1;

    /* For simplicity, we implement a DFS that returns first found path */
    int32_t stack_cap = 2048;
    flp_configuration *stack = (flp_configuration*)malloc(
        (size_t)stack_cap * sizeof(flp_configuration));
    flp_schedule *scheds = (flp_schedule*)malloc(
        (size_t)stack_cap * sizeof(flp_schedule));
    if (stack == NULL || scheds == NULL) {
        free(stack);
        free(scheds);
        return -1;
    }

    flp_visited_set visited;
    visited_init(&visited, stack_cap);

    int32_t top = 0;
    flp_config_clone(&stack[top], start);
    flp_schedule_init(&scheds[top]);
    visited_add(&visited, start);
    top++;

    int32_t result_len = -1;

    while (top > 0 && result_len < 0) {
        top--;
        flp_configuration current = stack[top];
        flp_schedule current_sched = scheds[top];

        /* Check if target reached */
        for (int32_t i = 0; i < current.num_processes; i++) {
            if (flp_process_is_correct(&current.processes[i]) &&
                current.processes[i].has_decided &&
                current.processes[i].decision == target) {
                *out = current_sched;
                result_len = current_sched.length;
                break;
            }
        }
        if (result_len >= 0) break;

        if (current.depth >= max_depth) continue;

        /* Generate successors (DFS order, push children) */
        for (int32_t pid = current.num_processes - 1; pid >= 0; pid--) {
            if (!flp_process_is_correct(&current.processes[pid])) continue;

            int32_t pending = flp_msg_buffer_pending_count(&current, pid);
            if (pending == 0) continue;

            flp_message msgs[16];
            int32_t n_msgs = flp_msg_buffer_peek(&current, pid, msgs, 16);

            for (int32_t mi = n_msgs - 1; mi >= 0; mi--) {
                flp_configuration next;
                flp_config_clone(&next, &current);

                flp_message delivered;
                bool msg_found = false;
                for (int32_t mi2 = 0; mi2 < next.num_messages; mi2++) {
                    if (next.messages[mi2].sender == msgs[mi].sender &&
                        next.messages[mi2].receiver == msgs[mi].receiver) {
                        delivered = next.messages[mi2];
                        next.messages[mi2] = next.messages[next.num_messages - 1];
                        next.num_messages--;
                        msg_found = true;
                        break;
                    }
                }
                if (!msg_found) continue;

                flp_protocol_desc desc;
                flp_protocol_desc_init(&desc, FLP_PROTO_FLOOD_SET,
                                        next.num_processes, 1);
                flp_step_result step_res;
                flp_protocol_step(&desc, &next.processes[pid],
                                  &delivered, &step_res);
                next.processes[pid] = step_res.new_state;

                for (int32_t oi = 0; oi < step_res.num_outgoing; oi++) {
                    flp_msg_buffer_send(&next, &step_res.outgoing[oi]);
                }

                next.depth = current.depth + 1;
                flp_config_invalidate_valence(&next);

                if (!visited_contains(&visited, &next) && top < stack_cap) {
                    flp_config_clone(&stack[top], &next);
                    flp_schedule_init(&scheds[top]);
                    flp_schedule_append_schedule(&scheds[top], &current_sched);
                    flp_schedule_append(&scheds[top], pid);
                    visited_add(&visited, &next);
                    top++;
                }
            }
        }
    }

    visited_free(&visited);
    free(stack);
    free(scheds);
    return result_len;
}

int32_t flp_config_schedule_to_0(const flp_configuration *cfg,
                                  flp_schedule *out, int32_t max_depth)
{
    return search_schedule_to_decision(cfg, FLP_DECIDE_0, max_depth, out);
}

int32_t flp_config_schedule_to_1(const flp_configuration *cfg,
                                  flp_schedule *out, int32_t max_depth)
{
    return search_schedule_to_decision(cfg, FLP_DECIDE_1, max_depth, out);
}

/* ===== Display Functions ===== */

void flp_config_print(const flp_configuration *cfg)
{
    if (cfg == NULL) {
        printf("Configuration: NULL\n");
        return;
    }
    printf("=== Configuration #%d (depth=%d, valence=%s) ===\n",
           cfg->config_id, cfg->depth,
           flp_valence_to_string(cfg->valence));
    printf("Processes (%d):\n", cfg->num_processes);
    for (int32_t i = 0; i < cfg->num_processes; i++) {
        const flp_process_state *ps = &cfg->processes[i];
        printf("  P%d: status=%s input=%d decided=%s step=%d round=%d\n",
               ps->pid,
               ps->status == FLP_PROCESS_CORRECT ? "correct" :
               ps->status == FLP_PROCESS_CRASHED ? "crashed" : "suspected",
               ps->input_value,
               flp_decision_to_string(ps->decision),
               ps->step_count,
               ps->current_round);
    }
    printf("Messages (%d):\n", cfg->num_messages);
    for (int32_t i = 0; i < cfg->num_messages; i++) {
        const flp_message *m = &cfg->messages[i];
        printf("  M%d: P%d->P%d type=%d val=%d round=%d\n",
               i, m->sender, m->receiver, m->type, m->value, m->round);
    }
}

int32_t flp_config_to_string(const flp_configuration *cfg,
                              char *buf, int32_t bufsize)
{
    if (cfg == NULL || buf == NULL || bufsize <= 0) return 0;
    int32_t off = snprintf(buf, (size_t)bufsize,
                            "C#%d N=%d M=%d valence=%s",
                            cfg->config_id, cfg->num_processes,
                            cfg->num_messages,
                            flp_valence_to_string(cfg->valence));
    if (off < bufsize) {
        for (int32_t i = 0; i < cfg->num_processes && off < bufsize; i++) {
            int32_t n = snprintf(buf + off, (size_t)(bufsize - off),
                                  " P%d=%s", i,
                                  flp_decision_to_string(
                                      cfg->processes[i].decision));
            if (n < 0) break;
            off += n;
        }
    }
    return off;
}
