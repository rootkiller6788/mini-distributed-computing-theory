/**
 * @file flp_message.c
 * @brief Message buffer (multiset) operations for the FLP model.
 *
 * The message buffer M is the communication medium. It is a multiset:
 * messages are not ordered, and duplicates are possible. Delivery is
 * reliable (no loss) but asynchronous (arbitrary delay).
 *
 * Key operations:
 *   - send: add a message to the buffer
 *   - deliver: remove and return a message addressed to a process
 *   - peek: inspect pending messages without removing
 *
 * Reference: FLP (1985) Section 2, "The Model"
 */

#include "flp_message.h"
#include "flp_common.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Global message delivery strategy */
static flp_msg_strategy g_msg_strategy = FLP_MSG_FIFO;

/* Global message ID counter */
static int32_t g_next_msg_id = 0;

/* ===== Buffer Management ===== */

void flp_msg_buffer_clear(flp_configuration *cfg)
{
    if (cfg != NULL) {
        cfg->num_messages = 0;
        memset(cfg->messages, 0, sizeof(cfg->messages));
    }
}

/* ===== Message Sending ===== */

bool flp_msg_buffer_send(flp_configuration *cfg, const flp_message *msg)
{
    if (cfg == NULL || msg == NULL) return false;
    if (cfg->num_messages >= FLP_MAX_MESSAGES) return false;

    cfg->messages[cfg->num_messages] = *msg;
    cfg->messages[cfg->num_messages].msg_id = g_next_msg_id++;
    cfg->num_messages++;
    return true;
}

bool flp_msg_send_to(flp_configuration *cfg,
                      flp_process_id sender, flp_process_id receiver,
                      flp_msg_type type, int32_t value, flp_round_t round)
{
    flp_message msg;
    memset(&msg, 0, sizeof(msg));
    msg.sender = sender;
    msg.receiver = receiver;
    msg.type = type;
    msg.value = value;
    msg.round = round;
    msg.msg_id = g_next_msg_id++;
    return flp_msg_buffer_send(cfg, &msg);
}

/* ===== Message Delivery ===== */

static int32_t select_message_by_strategy(const flp_configuration *cfg,
                                           flp_process_id receiver,
                                           flp_msg_strategy strategy)
{
    if (cfg == NULL || cfg->num_messages == 0) return -1;

    int32_t candidates[FLP_MAX_MESSAGES];
    int32_t num_candidates = 0;

    /* Gather all messages addressed to receiver */
    for (int32_t i = 0; i < cfg->num_messages; i++) {
        if (cfg->messages[i].receiver == receiver) {
            candidates[num_candidates++] = i;
        }
    }

    if (num_candidates == 0) return -1;
    if (num_candidates == 1) return candidates[0];

    /* Apply strategy to select among candidates */
    switch (strategy) {
        case FLP_MSG_FIFO: {
            /* Oldest by msg_id (lowest msg_id) */
            int32_t best = candidates[0];
            int32_t best_id = cfg->messages[best].msg_id;
            for (int32_t j = 1; j < num_candidates; j++) {
                int32_t idx = candidates[j];
                if (cfg->messages[idx].msg_id < best_id) {
                    best = idx;
                    best_id = cfg->messages[idx].msg_id;
                }
            }
            return best;
        }

        case FLP_MSG_LIFO: {
            /* Newest by msg_id (highest msg_id) */
            int32_t best = candidates[0];
            int32_t best_id = cfg->messages[best].msg_id;
            for (int32_t j = 1; j < num_candidates; j++) {
                int32_t idx = candidates[j];
                if (cfg->messages[idx].msg_id > best_id) {
                    best = idx;
                    best_id = cfg->messages[idx].msg_id;
                }
            }
            return best;
        }

        case FLP_MSG_ADVERSARIAL: {
            /* Adversary: choose message that delays decision.
               Heuristic: prefer lower-round messages (older phase). */
            int32_t best = candidates[0];
            int32_t best_round = cfg->messages[best].round;
            for (int32_t j = 1; j < num_candidates; j++) {
                int32_t idx = candidates[j];
                if (cfg->messages[idx].round < best_round) {
                    best = idx;
                    best_round = cfg->messages[idx].round;
                }
            }
            return best;
        }

        case FLP_MSG_RANDOM: {
            return candidates[rand() % num_candidates];
        }

        case FLP_MSG_BY_SENDER: {
            /* Round-robin by sender: prefer senders we have not heard from */
            /* Simplified: pick lowest sender ID */
            int32_t best = candidates[0];
            for (int32_t j = 1; j < num_candidates; j++) {
                int32_t idx = candidates[j];
                if (cfg->messages[idx].sender < cfg->messages[best].sender) {
                    best = idx;
                }
            }
            return best;
        }

        case FLP_MSG_OLDEST_FIRST:
        case FLP_MSG_NEWEST_FIRST:
        default:
            return candidates[0];
    }
}

bool flp_msg_buffer_deliver(flp_configuration *cfg,
                             flp_process_id receiver,
                             flp_message *out)
{
    if (cfg == NULL || out == NULL) return false;

    int32_t idx = select_message_by_strategy(cfg, receiver, g_msg_strategy);
    if (idx < 0) return false;

    *out = cfg->messages[idx];

    /* Remove from buffer: swap with last element */
    cfg->messages[idx] = cfg->messages[cfg->num_messages - 1];
    cfg->num_messages--;
    return true;
}

bool flp_msg_buffer_deliver_index(flp_configuration *cfg,
                                   int32_t msg_index,
                                   flp_message *out)
{
    if (cfg == NULL || out == NULL) return false;
    if (msg_index < 0 || msg_index >= cfg->num_messages) return false;

    *out = cfg->messages[msg_index];

    /* Remove from buffer */
    cfg->messages[msg_index] = cfg->messages[cfg->num_messages - 1];
    cfg->num_messages--;
    return true;
}

/* ===== Message Querying ===== */

int32_t flp_msg_buffer_pending_count(const flp_configuration *cfg,
                                      flp_process_id pid)
{
    if (cfg == NULL) return 0;
    int32_t count = 0;
    for (int32_t i = 0; i < cfg->num_messages; i++) {
        if (cfg->messages[i].receiver == pid) count++;
    }
    return count;
}

int32_t flp_msg_buffer_peek(const flp_configuration *cfg,
                             flp_process_id pid,
                             flp_message *out, int32_t max_out)
{
    if (cfg == NULL || out == NULL || max_out <= 0) return 0;
    int32_t count = 0;
    for (int32_t i = 0; i < cfg->num_messages && count < max_out; i++) {
        if (cfg->messages[i].receiver == pid) {
            out[count++] = cfg->messages[i];
        }
    }
    return count;
}

/* ===== Buffer Maintenance ===== */

void flp_msg_buffer_purge_process(flp_configuration *cfg, flp_process_id pid)
{
    if (cfg == NULL) return;
    /* Remove all messages to and from pid */
    int32_t write = 0;
    for (int32_t read = 0; read < cfg->num_messages; read++) {
        if (cfg->messages[read].sender != pid &&
            cfg->messages[read].receiver != pid) {
            if (write != read) {
                cfg->messages[write] = cfg->messages[read];
            }
            write++;
        }
    }
    cfg->num_messages = write;
}

/* ===== Strategy Configuration ===== */

void flp_msg_set_strategy(flp_msg_strategy strategy)
{
    g_msg_strategy = strategy;
}

flp_msg_strategy flp_msg_get_strategy(void)
{
    return g_msg_strategy;
}

/* ===== Buffer Statistics ===== */

int32_t flp_msg_buffer_total_count(const flp_configuration *cfg)
{
    if (cfg == NULL) return 0;
    return cfg->num_messages;
}

int32_t flp_msg_buffer_channel_count(const flp_configuration *cfg)
{
    if (cfg == NULL) return 0;
    /* Count unique (sender, receiver) pairs */
    bool seen[FLP_MAX_PROCESSES][FLP_MAX_PROCESSES];
    memset(seen, 0, sizeof(seen));
    int32_t count = 0;
    for (int32_t i = 0; i < cfg->num_messages; i++) {
        int32_t s = cfg->messages[i].sender;
        int32_t r = cfg->messages[i].receiver;
        if (s >= 0 && s < FLP_MAX_PROCESSES &&
            r >= 0 && r < FLP_MAX_PROCESSES && !seen[s][r]) {
            seen[s][r] = true;
            count++;
        }
    }
    return count;
}

int32_t flp_msg_buffer_count_by_type(const flp_configuration *cfg,
                                      flp_msg_type type)
{
    if (cfg == NULL) return 0;
    int32_t count = 0;
    for (int32_t i = 0; i < cfg->num_messages; i++) {
        if (cfg->messages[i].type == type) count++;
    }
    return count;
}

bool flp_msg_buffer_is_empty(const flp_configuration *cfg)
{
    if (cfg == NULL) return true;
    return cfg->num_messages == 0;
}

int32_t flp_msg_buffer_oldest_msg_id(const flp_configuration *cfg)
{
    if (cfg == NULL || cfg->num_messages == 0) return -1;
    int32_t oldest = cfg->messages[0].msg_id;
    for (int32_t i = 1; i < cfg->num_messages; i++) {
        if (cfg->messages[i].msg_id < oldest) {
            oldest = cfg->messages[i].msg_id;
        }
    }
    return oldest;
}

/* ===== Debug Output ===== */

void flp_msg_buffer_print(const flp_configuration *cfg)
{
    if (cfg == NULL) {
        printf("Message buffer: NULL\n");
        return;
    }
    printf("Message buffer (%d messages, strategy=%d):\n",
           cfg->num_messages, g_msg_strategy);
    for (int32_t i = 0; i < cfg->num_messages; i++) {
        const flp_message *m = &cfg->messages[i];
        printf("  [%d] S%d->R%d type=%d val=%d round=%d id=%d\n",
               i, m->sender, m->receiver, m->type,
               m->value, m->round, m->msg_id);
    }
}
