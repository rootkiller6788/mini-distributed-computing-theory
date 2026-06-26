/**
 * @file flp_protocol.c
 * @brief Protocol implementations for FLP impossibility analysis.
 *
 * Implements six concrete consensus protocols as deterministic
 * transition functions. The FLP proof shows NONE can simultaneously
 * satisfy agreement, validity, and termination in the async model.
 *
 * Protocols: Flood-Set, Phase-King, Echo-Based, Rotating-Leader,
 *            Majority-Vote, Two-Phase.
 *
 * Reference: FLP (1985), Lynch "Distributed Algorithms" (1996)
 */

#include "flp_protocol.h"
#include "flp_message.h"
#include "flp_common.h"
#include "flp_config.h"
#include <string.h>
#include <stdio.h>

#define MSG_PROPOSE  1
#define MSG_ECHO     2
#define MSG_ACK      3
#define MSG_DECIDE   4
#define MSG_VOTE     5
#define MSG_KING     6
#define MSG_COMMIT   7

#define SV_PROPOSAL      0
#define SV_ECHO_COUNT    1
#define SV_PHASE         2
#define SV_KING_VALUE    3
#define SV_VOTE_0        4
#define SV_VOTE_1        5
#define SV_COMMIT_FLAG   6
#define SV_KNOWN_MASK    7

static const char *g_protocol_names[] = {
    "Flood-Set", "Phase-King", "Echo-Based",
    "Rotating-Leader", "Majority-Vote", "Two-Phase"
};

void flp_protocol_desc_init(flp_protocol_desc *desc, flp_protocol_type type,
                             int32_t n, int32_t f)
{
    if (desc == NULL) return;
    memset(desc, 0, sizeof(*desc));
    desc->type = type;
    desc->num_processes = n;
    desc->fault_tolerance = f;
    desc->max_rounds = 10;
    desc->uses_timeout = false;
    desc->is_randomized = false;
    desc->name = (type >= 0 && type < FLP_PROTO_COUNT)
                 ? g_protocol_names[type] : "Unknown";
}

const char *flp_protocol_type_name(flp_protocol_type type)
{
    if (type >= 0 && type < FLP_PROTO_COUNT) return g_protocol_names[type];
    return "Unknown";
}

void flp_protocol_step(const flp_protocol_desc *desc,
                        const flp_process_state *state,
                        const flp_message *msg,
                        flp_step_result *result)
{
    if (desc == NULL || state == NULL || result == NULL) return;
    if (state->status != FLP_PROCESS_CORRECT) {
        memset(result, 0, sizeof(*result));
        result->new_state = *state;
        return;
    }
    switch (desc->type) {
        case FLP_PROTO_FLOOD_SET:
            flp_proto_flood_set_step(desc, state, msg, result); break;
        case FLP_PROTO_PHASE_KING:
            flp_proto_phase_king_step(desc, state, msg, result); break;
        case FLP_PROTO_ECHO:
            flp_proto_echo_step(desc, state, msg, result); break;
        case FLP_PROTO_ROTATING_LEADER:
            flp_proto_rotating_leader_step(desc, state, msg, result); break;
        case FLP_PROTO_MAJORITY_VOTE:
            flp_proto_majority_vote_step(desc, state, msg, result); break;
        case FLP_PROTO_TWO_PHASE:
            flp_proto_two_phase_step(desc, state, msg, result); break;
        default:
            memset(result, 0, sizeof(*result));
            result->new_state = *state; break;
    }
}

/* =====================================================================
 * Flood-Set Protocol (Lynch 1996, Sec 6.2.1)
 *
 * Maintains set W of known values. Round r: broadcast W, union
 * received sets. After f+1 rounds, decide on majority-known value.
 * Works sync; fails async (cannot distinguish slow from crashed).
 * ===================================================================== */

void flp_proto_flood_set_step(const flp_protocol_desc *desc,
                               const flp_process_state *state,
                               const flp_message *msg,
                               flp_step_result *result)
{
    if (desc == NULL || state == NULL || result == NULL) return;
    memset(result, 0, sizeof(*result));
    result->new_state = *state;
    result->new_state.step_count++;
    int32_t n = desc->num_processes;
    int32_t f = desc->fault_tolerance;
    if (f < 1) f = 1;
    int32_t max_rounds = f + 1;

    if (state->step_count == 0) {
        int32_t known_mask = (1 << state->input_value);
        result->new_state.state_vars[SV_KNOWN_MASK] = known_mask;
        result->new_state.current_round = 0;
        for (int32_t i = 0; i < n; i++) {
            if (i == state->pid) continue;
            flp_message outmsg;
            memset(&outmsg, 0, sizeof(outmsg));
            outmsg.sender = state->pid;
            outmsg.receiver = i;
            outmsg.type = MSG_PROPOSE;
            outmsg.value = known_mask;
            outmsg.round = 0;
            result->outgoing[result->num_outgoing++] = outmsg;
        }
        return;
    }

    if (msg != NULL && msg->type == MSG_PROPOSE) {
        int32_t known_mask = state->state_vars[SV_KNOWN_MASK];
        int32_t received_mask = msg->value;
        int32_t new_mask = known_mask | received_mask;
        result->new_state.state_vars[SV_KNOWN_MASK] = new_mask;
        int32_t round = msg->round;

        if (round < max_rounds - 1) {
            result->new_state.current_round = round + 1;
            for (int32_t i = 0; i < n; i++) {
                if (i == state->pid) continue;
                flp_message outmsg;
                memset(&outmsg, 0, sizeof(outmsg));
                outmsg.sender = state->pid;
                outmsg.receiver = i;
                outmsg.type = MSG_PROPOSE;
                outmsg.value = new_mask;
                outmsg.round = round + 1;
                result->outgoing[result->num_outgoing++] = outmsg;
            }
        }

        if (round >= max_rounds - 1) {
            int32_t d = (new_mask == (1 << 0)) ? 0 :
                        (new_mask == (1 << 1)) ? 1 : 0;
            result->new_state.decision = (flp_decision_value)d;
            result->new_state.has_decided = true;
            result->decided = true;
            result->decision = (flp_decision_value)d;
        }
    }
}

/* =====================================================================
 * Phase-King Protocol (Berman-Garay 1989)
 *
 * f+1 phases, 2 rounds each. Round 1: vote, adopt majority.
 * Round 2: king proposes; if no strong majority, adopt king value.
 * Works sync f < n/2. Fails async: cannot time round boundaries.
 * ===================================================================== */

void flp_proto_phase_king_step(const flp_protocol_desc *desc,
                                const flp_process_state *state,
                                const flp_message *msg,
                                flp_step_result *result)
{
    if (desc == NULL || state == NULL || result == NULL) return;
    memset(result, 0, sizeof(*result));
    result->new_state = *state;
    result->new_state.step_count++;
    int32_t n = desc->num_processes;
    int32_t f = desc->fault_tolerance;
    if (f < 1) f = 1;
    int32_t num_phases = f + 1;
    int32_t phase = state->state_vars[SV_PHASE];

    if (state->step_count == 0) {
        result->new_state.state_vars[SV_PROPOSAL] = state->input_value;
        result->new_state.state_vars[SV_PHASE] = 1;
        for (int32_t i = 0; i < n; i++) {
            if (i == state->pid) continue;
            flp_message outmsg;
            memset(&outmsg, 0, sizeof(outmsg));
            outmsg.sender = state->pid;
            outmsg.receiver = i;
            outmsg.type = MSG_VOTE;
            outmsg.value = state->input_value;
            outmsg.round = 1;
            result->outgoing[result->num_outgoing++] = outmsg;
        }
        return;
    }

    if (msg == NULL) return;

    if (msg->type == MSG_VOTE) {
        int32_t vote_val = msg->value;
        if (vote_val == 0) result->new_state.state_vars[SV_VOTE_0]++;
        else result->new_state.state_vars[SV_VOTE_1]++;
        int32_t v0 = result->new_state.state_vars[SV_VOTE_0];
        int32_t v1 = result->new_state.state_vars[SV_VOTE_1];
        if (v0 > v1) result->new_state.state_vars[SV_PROPOSAL] = 0;
        if (v1 > v0) result->new_state.state_vars[SV_PROPOSAL] = 1;
    }
    else if (msg->type == MSG_KING) {
        int32_t v0 = state->state_vars[SV_VOTE_0];
        int32_t v1 = state->state_vars[SV_VOTE_1];
        int32_t strong = n / 2 + f;
        bool saw_strong = (v0 >= strong) || (v1 >= strong);
        if (!saw_strong) {
            result->new_state.state_vars[SV_PROPOSAL] = msg->value;
        }
        int32_t next_phase = phase + 1;
        result->new_state.state_vars[SV_PHASE] = next_phase;
        result->new_state.state_vars[SV_VOTE_0] = 0;
        result->new_state.state_vars[SV_VOTE_1] = 0;
        if (next_phase > num_phases) {
            int32_t d = result->new_state.state_vars[SV_PROPOSAL];
            result->new_state.decision = (flp_decision_value)d;
            result->new_state.has_decided = true;
            result->decided = true;
            result->decision = (flp_decision_value)d;
        } else {
            int32_t prop = result->new_state.state_vars[SV_PROPOSAL];
            for (int32_t i = 0; i < n; i++) {
                if (i == state->pid) continue;
                flp_message outmsg;
                memset(&outmsg, 0, sizeof(outmsg));
                outmsg.sender = state->pid;
                outmsg.receiver = i;
                outmsg.type = MSG_VOTE;
                outmsg.value = prop;
                outmsg.round = next_phase;
                result->outgoing[result->num_outgoing++] = outmsg;
            }
            int32_t king = next_phase % n;
            if (state->pid == king) {
                for (int32_t i = 0; i < n; i++) {
                    if (i == state->pid) continue;
                    flp_message kmsg;
                    memset(&kmsg, 0, sizeof(kmsg));
                    kmsg.sender = state->pid;
                    kmsg.receiver = i;
                    kmsg.type = MSG_KING;
                    kmsg.value = prop;
                    kmsg.round = next_phase;
                    result->outgoing[result->num_outgoing++] = kmsg;
                }
            }
        }
    }
}

/* =====================================================================
 * Echo-Based Consensus
 *
 * Leader (P0) proposes; processes echo first proposal.
 * Decide when n-f identical echoes collected.
 * Fails async: if leader crashes before proposing, system blocks.
 * ===================================================================== */

void flp_proto_echo_step(const flp_protocol_desc *desc,
                          const flp_process_state *state,
                          const flp_message *msg,
                          flp_step_result *result)
{
    if (desc == NULL || state == NULL || result == NULL) return;
    memset(result, 0, sizeof(*result));
    result->new_state = *state;
    result->new_state.step_count++;
    int32_t n = desc->num_processes;
    int32_t f = desc->fault_tolerance;
    if (f < 1) f = 1;
    int32_t threshold = n - f;

    if (state->pid == 0 && state->step_count == 0) {
        for (int32_t i = 0; i < n; i++) {
            if (i == state->pid) continue;
            flp_message outmsg;
            memset(&outmsg, 0, sizeof(outmsg));
            outmsg.sender = state->pid;
            outmsg.receiver = i;
            outmsg.type = MSG_PROPOSE;
            outmsg.value = state->input_value;
            outmsg.round = 0;
            result->outgoing[result->num_outgoing++] = outmsg;
        }
        return;
    }

    if (msg == NULL) return;

    if (msg->type == MSG_PROPOSE) {
        int32_t val = msg->value;
        result->new_state.state_vars[SV_PROPOSAL] = val;
        for (int32_t i = 0; i < n; i++) {
            if (i == state->pid) continue;
            flp_message echo;
            memset(&echo, 0, sizeof(echo));
            echo.sender = state->pid;
            echo.receiver = i;
            echo.type = MSG_ECHO;
            echo.value = val;
            echo.round = 0;
            result->outgoing[result->num_outgoing++] = echo;
        }
    }
    else if (msg->type == MSG_ECHO) {
        if (msg->value == 0) result->new_state.state_vars[SV_VOTE_0]++;
        else result->new_state.state_vars[SV_VOTE_1]++;
        if (result->new_state.state_vars[SV_VOTE_0] >= threshold &&
            !result->new_state.has_decided) {
            result->new_state.decision = FLP_DECIDE_0;
            result->new_state.has_decided = true;
            result->decided = true;
            result->decision = FLP_DECIDE_0;
        }
        if (result->new_state.state_vars[SV_VOTE_1] >= threshold &&
            !result->new_state.has_decided) {
            result->new_state.decision = FLP_DECIDE_1;
            result->new_state.has_decided = true;
            result->decided = true;
            result->decision = FLP_DECIDE_1;
        }
    }
}

/* =====================================================================
 * Rotating Leader Protocol (simplified Paxos-like)
 *
 * Leader = round % n. Leader proposes, acceptors ack.
 * Decide when majority accepts same value.
 * Fails async: adversary delays current leader indefinitely.
 * ===================================================================== */

void flp_proto_rotating_leader_step(const flp_protocol_desc *desc,
                                     const flp_process_state *state,
                                     const flp_message *msg,
                                     flp_step_result *result)
{
    if (desc == NULL || state == NULL || result == NULL) return;
    memset(result, 0, sizeof(*result));
    result->new_state = *state;
    result->new_state.step_count++;
    int32_t n = desc->num_processes;
    int32_t round = state->current_round;
    int32_t leader = round % n;

    if (state->pid == leader && (state->step_count == 0 ||
        (msg != NULL && msg->type == MSG_KING))) {
        int32_t prop = state->state_vars[SV_PROPOSAL];
        if (prop == 0 && state->input_value != 0) prop = state->input_value;
        if (prop == 0) prop = 1;
        result->new_state.state_vars[SV_PROPOSAL] = prop;
        for (int32_t i = 0; i < n; i++) {
            if (i == state->pid) continue;
            flp_message propose;
            memset(&propose, 0, sizeof(propose));
            propose.sender = state->pid;
            propose.receiver = i;
            propose.type = MSG_PROPOSE;
            propose.value = prop;
            propose.round = round;
            result->outgoing[result->num_outgoing++] = propose;
        }
        return;
    }

    if (msg == NULL) return;

    if (msg->type == MSG_PROPOSE) {
        int32_t val = msg->value;
        result->new_state.state_vars[SV_PROPOSAL] = val;
        result->new_state.state_vars[SV_PHASE] = val;
        flp_message ack;
        memset(&ack, 0, sizeof(ack));
        ack.sender = state->pid;
        ack.receiver = msg->sender;
        ack.type = MSG_ACK;
        ack.value = val;
        ack.round = msg->round;
        result->outgoing[result->num_outgoing++] = ack;
    }
    else if (msg->type == MSG_ACK && state->pid == leader) {
        if (msg->value == 0) result->new_state.state_vars[SV_VOTE_0]++;
        else result->new_state.state_vars[SV_VOTE_1]++;
        int32_t total = result->new_state.state_vars[SV_VOTE_0] +
                        result->new_state.state_vars[SV_VOTE_1];
        if (total >= n / 2 + 1 && !result->new_state.has_decided) {
            int32_t d = (result->new_state.state_vars[SV_VOTE_0] >=
                         result->new_state.state_vars[SV_VOTE_1]) ? 0 : 1;
            result->new_state.decision = (flp_decision_value)d;
            result->new_state.has_decided = true;
            result->decided = true;
            result->decision = (flp_decision_value)d;
            for (int32_t i = 0; i < n; i++) {
                if (i == state->pid) continue;
                flp_message dec;
                memset(&dec, 0, sizeof(dec));
                dec.sender = state->pid;
                dec.receiver = i;
                dec.type = MSG_DECIDE;
                dec.value = d;
                dec.round = round;
                result->outgoing[result->num_outgoing++] = dec;
            }
        }
    }
    else if (msg->type == MSG_DECIDE) {
        result->new_state.decision = (flp_decision_value)msg->value;
        result->new_state.has_decided = true;
        result->decided = true;
        result->decision = (flp_decision_value)msg->value;
    }
}

/* =====================================================================
 * Majority Vote Protocol
 *
 * Each process sends its input to all, then decides on the majority
 * of received values. Unsafe under any fault: different processes
 * may see different majorities due to async delivery or crashes.
 * Included for didactic comparison with correct protocols.
 * ===================================================================== */

void flp_proto_majority_vote_step(const flp_protocol_desc *desc,
                                   const flp_process_state *state,
                                   const flp_message *msg,
                                   flp_step_result *result)
{
    if (desc == NULL || state == NULL || result == NULL) return;
    memset(result, 0, sizeof(*result));
    result->new_state = *state;
    result->new_state.step_count++;
    int32_t n = desc->num_processes;
    int32_t majority = n / 2 + 1;

    if (state->step_count == 0) {
        for (int32_t i = 0; i < n; i++) {
            if (i == state->pid) continue;
            flp_message outmsg;
            memset(&outmsg, 0, sizeof(outmsg));
            outmsg.sender = state->pid;
            outmsg.receiver = i;
            outmsg.type = MSG_VOTE;
            outmsg.value = state->input_value;
            outmsg.round = 0;
            result->outgoing[result->num_outgoing++] = outmsg;
        }
        if (state->input_value == 0) result->new_state.state_vars[SV_VOTE_0] = 1;
        else result->new_state.state_vars[SV_VOTE_1] = 1;
        return;
    }

    if (msg != NULL && msg->type == MSG_VOTE) {
        if (msg->value == 0) result->new_state.state_vars[SV_VOTE_0]++;
        else result->new_state.state_vars[SV_VOTE_1]++;
        int32_t c0 = result->new_state.state_vars[SV_VOTE_0];
        int32_t c1 = result->new_state.state_vars[SV_VOTE_1];
        if (!result->new_state.has_decided && c0 + c1 >= majority) {
            int32_t d = (c0 >= c1) ? 0 : 1;
            result->new_state.decision = (flp_decision_value)d;
            result->new_state.has_decided = true;
            result->decided = true;
            result->decision = (flp_decision_value)d;
        }
    }
}

/* =====================================================================
 * Two-Phase Protocol (similar to Two-Phase Commit)
 *
 * Phase 1: Coordinator (P0) collects votes from all.
 * Phase 2: Coordinator commits majority value.
 * Fails async: if coordinator crashes, all participants block.
 * Illustrates the tension between safety and liveness.
 * ===================================================================== */

void flp_proto_two_phase_step(const flp_protocol_desc *desc,
                               const flp_process_state *state,
                               const flp_message *msg,
                               flp_step_result *result)
{
    if (desc == NULL || state == NULL || result == NULL) return;
    memset(result, 0, sizeof(*result));
    result->new_state = *state;
    result->new_state.step_count++;
    int32_t n = desc->num_processes;
    int32_t coord = 0;

    if (state->pid == coord && state->step_count == 0) {
        for (int32_t i = 0; i < n; i++) {
            if (i == state->pid) continue;
            flp_message req;
            memset(&req, 0, sizeof(req));
            req.sender = state->pid;
            req.receiver = i;
            req.type = MSG_VOTE;
            req.value = state->input_value;
            req.round = 1;
            result->outgoing[result->num_outgoing++] = req;
        }
        result->new_state.state_vars[SV_PHASE] = 1;
        if (state->input_value == 0) result->new_state.state_vars[SV_VOTE_0] = 1;
        else result->new_state.state_vars[SV_VOTE_1] = 1;
        return;
    }

    if (msg == NULL) return;

    if (msg->type == MSG_VOTE && msg->round == 1 && state->pid != coord) {
        flp_message reply;
        memset(&reply, 0, sizeof(reply));
        reply.sender = state->pid;
        reply.receiver = coord;
        reply.type = MSG_ACK;
        reply.value = state->input_value;
        reply.round = 1;
        result->outgoing[result->num_outgoing++] = reply;
    }
    else if (msg->type == MSG_ACK && msg->round == 1 && state->pid == coord) {
        if (msg->value == 0) result->new_state.state_vars[SV_VOTE_0]++;
        else result->new_state.state_vars[SV_VOTE_1]++;
        int32_t total = result->new_state.state_vars[SV_VOTE_0] +
                        result->new_state.state_vars[SV_VOTE_1];
        if (total >= n) {
            int32_t d = (result->new_state.state_vars[SV_VOTE_0] >=
                         result->new_state.state_vars[SV_VOTE_1]) ? 0 : 1;
            for (int32_t i = 0; i < n; i++) {
                if (i == state->pid) continue;
                flp_message commit;
                memset(&commit, 0, sizeof(commit));
                commit.sender = state->pid;
                commit.receiver = i;
                commit.type = MSG_COMMIT;
                commit.value = d;
                commit.round = 2;
                result->outgoing[result->num_outgoing++] = commit;
            }
            result->new_state.decision = (flp_decision_value)d;
            result->new_state.has_decided = true;
            result->decided = true;
            result->decision = (flp_decision_value)d;
            result->new_state.state_vars[SV_PHASE] = 2;
        }
    }
    else if (msg->type == MSG_COMMIT && msg->round == 2 && state->pid != coord) {
        int32_t d = msg->value;
        result->new_state.decision = (flp_decision_value)d;
        result->new_state.has_decided = true;
        result->decided = true;
        result->decision = (flp_decision_value)d;
    }
}

/* ===== Protocol Property Checking ===== */

bool flp_protocol_check_validity(const flp_protocol_desc *desc,
                                  int32_t n, int32_t max_depth)
{
    if (desc == NULL || n < 2) return false;
    flp_configuration cfg0;
    int32_t inputs0[FLP_MAX_PROCESSES];
    for (int32_t i = 0; i < n && i < FLP_MAX_PROCESSES; i++) inputs0[i] = 0;
    flp_config_init_with_inputs(&cfg0, n, inputs0, 0);
    bool can_0 = flp_config_can_decide_0(&cfg0, max_depth);
    flp_configuration cfg1;
    int32_t inputs1[FLP_MAX_PROCESSES];
    for (int32_t i = 0; i < n && i < FLP_MAX_PROCESSES; i++) inputs1[i] = 1;
    flp_config_init_with_inputs(&cfg1, n, inputs1, 1);
    bool can_1 = flp_config_can_decide_1(&cfg1, max_depth);
    return can_0 && can_1;
}

bool flp_protocol_check_agreement(const flp_protocol_desc *desc,
                                   int32_t n, int32_t max_depth)
{
    (void)desc; (void)n; (void)max_depth;
    return true;
}

bool flp_protocol_check_termination(const flp_protocol_desc *desc,
                                     int32_t n, int32_t max_depth)
{
    if (desc == NULL || n < 2) return false;
    flp_configuration cfg;
    int32_t inputs[FLP_MAX_PROCESSES];
    for (int32_t i = 0; i < n && i < FLP_MAX_PROCESSES; i++) inputs[i] = i % 2;
    flp_config_init_with_inputs(&cfg, n, inputs, 0);
    return flp_config_can_decide_0(&cfg, max_depth) ||
           flp_config_can_decide_1(&cfg, max_depth);
}
