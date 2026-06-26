/**
 * @file flp_message.h
 * @brief Message buffer operations for the FLP system model.
 *
 * The message buffer M is a multiset of messages. Delivery is reliable
 * but asynchronous: every message sent is eventually delivered exactly
 * once, but the delay is arbitrary and unbounded.
 *
 * Knowledge Coverage:
 *   L1: Message buffer definition (multiset semantics)
 *   L2: Reliable asynchronous communication channel
 *   L3: Multiset operations (insert, remove, query)
 *   L5: Message scheduling strategies
 *
 * Course Alignment: MIT 6.852 Lec 5, CMU 15-712, Oxford CS Theory
 */

#ifndef FLP_MESSAGE_H
#define FLP_MESSAGE_H

#include "flp_common.h"

/* ---------------------------------------------------------------------------
 * L1: Message Buffer (Multiset)
 * ---------------------------------------------------------------------------
 * The message buffer is a multiset: messages may be duplicated, and
 * there is no FIFO ordering guarantee. A message is consumed (removed
 * from the buffer) when it is delivered to its destination process.
 */

/**
 * Initialize an empty message buffer within a configuration.
 * @param cfg  Configuration whose buffer will be cleared.
 */
void flp_msg_buffer_clear(flp_configuration *cfg);

/**
 * Add a message to the buffer.
 * @param cfg  Configuration containing the buffer.
 * @param msg  Message to add (copied by value).
 * @return     true if successful, false if buffer is full.
 */
bool flp_msg_buffer_send(flp_configuration *cfg, const flp_message *msg);

/**
 * Send a message with specific fields (convenience wrapper).
 * @param cfg      Target configuration.
 * @param sender   Sender process ID.
 * @param receiver Receiver process ID.
 * @param type     Message type.
 * @param value    Integer payload.
 * @param round    Round number.
 * @return         true if sent successfully.
 */
bool flp_msg_send_to(flp_configuration *cfg,
                      flp_process_id sender, flp_process_id receiver,
                      flp_msg_type type, int32_t value, flp_round_t round);

/**
 * Remove and return a message addressed to a specific process.
 * The choice of which message (if multiple are available) is
 * non-deterministic -- this models the asynchronous adversary.
 * @param cfg       Configuration containing the buffer.
 * @param receiver  Process to receive a message.
 * @param out       Output: the delivered message.
 * @return          true if a message was available, false otherwise.
 */
bool flp_msg_buffer_deliver(flp_configuration *cfg,
                             flp_process_id receiver,
                             flp_message *out);

/**
 * Deliver a specific message by index (deterministic version,
 * used for replay and specific schedule exploration).
 * @param cfg        Configuration.
 * @param msg_index  Index in the message array.
 * @param out        Output: the message.
 * @return           true if index is valid.
 */
bool flp_msg_buffer_deliver_index(flp_configuration *cfg,
                                   int32_t msg_index,
                                   flp_message *out);

/**
 * Check if a process has any pending messages.
 * @param cfg  Configuration.
 * @param pid  Process ID to check.
 * @return     Number of messages pending for pid.
 */
int32_t flp_msg_buffer_pending_count(const flp_configuration *cfg,
                                      flp_process_id pid);

/**
 * Get all messages pending for a process without removing them.
 * @param cfg       Configuration.
 * @param pid       Process ID.
 * @param out       Output array (caller-provided).
 * @param max_out   Maximum entries in out array.
 * @return          Number of messages found.
 */
int32_t flp_msg_buffer_peek(const flp_configuration *cfg,
                             flp_process_id pid,
                             flp_message *out, int32_t max_out);

/**
 * Remove all messages to/from a crashed process (cleanup).
 * In the FLP model, a crashed process cannot send or receive.
 * @param cfg  Configuration.
 * @param pid  Crashed process ID.
 */
void flp_msg_buffer_purge_process(flp_configuration *cfg, flp_process_id pid);

/* ---------------------------------------------------------------------------
 * L2: Message Ordering Strategies
 * ---------------------------------------------------------------------------
 * The asynchronous adversary controls message delivery order.
 * Different strategies model different network behaviors.
 */

/** Message selection strategies for the adversary. */
typedef enum {
    FLP_MSG_FIFO = 0,           /**< First-in-first-out (per receiver) */
    FLP_MSG_LIFO = 1,           /**< Last-in-first-out (per receiver) */
    FLP_MSG_OLDEST_FIRST = 2,   /**< Oldest message in buffer first */
    FLP_MSG_NEWEST_FIRST = 3,   /**< Newest message first */
    FLP_MSG_ADVERSARIAL = 4,    /**< Adversarial (worst-case) ordering */
    FLP_MSG_RANDOM = 5,         /**< Uniformly random selection */
    FLP_MSG_BY_SENDER = 6,      /**< Round-robin by sender */
} flp_msg_strategy;

/**
 * Set the message delivery strategy for the simulation.
 * @param strategy  The ordering strategy to use.
 */
void flp_msg_set_strategy(flp_msg_strategy strategy);

/**
 * Get the current message delivery strategy.
 */
flp_msg_strategy flp_msg_get_strategy(void);

/* ---------------------------------------------------------------------------
 * L3: Message Buffer Statistics
 * ---------------------------------------------------------------------------
 */

/**
 * Count total messages currently in the buffer.
 */
int32_t flp_msg_buffer_total_count(const flp_configuration *cfg);

/**
 * Count unique sender-receiver pairs with pending messages.
 */
int32_t flp_msg_buffer_channel_count(const flp_configuration *cfg);

/**
 * Count messages by type in the buffer.
 * @param cfg   Configuration.
 * @param type  Message type to count.
 * @return      Number of messages of this type.
 */
int32_t flp_msg_buffer_count_by_type(const flp_configuration *cfg,
                                      flp_msg_type type);

/**
 * Check if the buffer is empty (no pending messages anywhere).
 */
bool flp_msg_buffer_is_empty(const flp_configuration *cfg);

/**
 * Get the oldest message timestamp (by msg_id) in the buffer.
 */
int32_t flp_msg_buffer_oldest_msg_id(const flp_configuration *cfg);

/**
 * Print the message buffer contents to stdout for debugging.
 */
void flp_msg_buffer_print(const flp_configuration *cfg);

#endif /* FLP_MESSAGE_H */
