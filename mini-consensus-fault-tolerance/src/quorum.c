/*
 * quorum.c - Quorum System Implementations
 * L3: Mathematical structures for quorum systems.
 * L5: Quorum construction algorithms.
 * L4: Intersection properties for consensus safety.
 * Reference: Malkhi & Reiter (1998) "Byzantine Quorum Systems"
 *            Maekawa (1985) "A sqrt(n) Algorithm for Mutual Exclusion"
 */
#include "quorum.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

quorum_system_t *quorum_create_majority(int n)
{
    if (n < 1) return NULL;
    quorum_system_t *qs = (quorum_system_t *)calloc(1, sizeof(quorum_system_t));
    if (!qs) return NULL;
    qs->total_processes = n;
    qs->quorum_size = n / 2 + 1;
    qs->has_intersection = true;
    qs->num_quorums = n;
    qs->quorums = (quorum_t *)calloc((size_t)n, sizeof(quorum_t));
    if (!qs->quorums) { free(qs); return NULL; }
    for (int i = 0; i < n; i++) {
        qs->quorums[i].quorum_id = i;
        qs->quorums[i].num_members = qs->quorum_size;
        qs->quorums[i].weight = 1;
        for (int j = 0; j < qs->quorum_size; j++)
            qs->quorums[i].members[j] = (i + j) % n;
    }
    return qs;
}

quorum_system_t *quorum_create_grid(int grid_side)
{
    if (grid_side < 2) return NULL;
    int n = grid_side * grid_side;
    quorum_system_t *qs = (quorum_system_t *)calloc(1, sizeof(quorum_system_t));
    if (!qs) return NULL;
    qs->total_processes = n;
    qs->num_quorums = n;
    qs->quorum_size = 2 * grid_side - 1;
    qs->has_intersection = true;
    qs->quorums = (quorum_t *)calloc((size_t)n, sizeof(quorum_t));
    if (!qs->quorums) { free(qs); return NULL; }
    for (int i = 0; i < n; i++) {
        int row = i / grid_side, col = i % grid_side;
        qs->quorums[i].quorum_id = i;
        qs->quorums[i].weight = 1;
        int m = 0;
        for (int c = 0; c < grid_side; c++)
            qs->quorums[i].members[m++] = row * grid_side + c;
        for (int r = 0; r < grid_side; r++) {
            int pid = r * grid_side + col;
            if (pid != i) qs->quorums[i].members[m++] = pid;
        }
        qs->quorums[i].num_members = m;
    }
    return qs;
}

quorum_system_t *quorum_create_b_masking(int n, int f)
{
    if (n < 3 * f + 1) return NULL;
    quorum_system_t *qs = (quorum_system_t *)calloc(1, sizeof(quorum_system_t));
    if (!qs) return NULL;
    int qsize = n / 2 + f + 1;
    qs->total_processes = n;
    qs->num_quorums = n;
    qs->quorum_size = qsize;
    qs->has_intersection = true;
    qs->quorums = (quorum_t *)calloc((size_t)n, sizeof(quorum_t));
    if (!qs->quorums) { free(qs); return NULL; }
    for (int i = 0; i < n; i++) {
        qs->quorums[i].quorum_id = i;
        qs->quorums[i].num_members = qsize;
        qs->quorums[i].weight = 1;
        for (int j = 0; j < qsize; j++)
            qs->quorums[i].members[j] = (i + j) % n;
    }
    return qs;
}

quorum_system_t *quorum_create_fpp(int q)
{
    if (q < 2) return NULL;
    int n = q * q + q + 1;
    if (n > 64) return NULL;
    quorum_system_t *qs = (quorum_system_t *)calloc(1, sizeof(quorum_system_t));
    if (!qs) return NULL;
    qs->total_processes = n;
    qs->num_quorums = n;
    qs->quorum_size = q + 1;
    qs->has_intersection = true;
    qs->quorums = (quorum_t *)calloc((size_t)n, sizeof(quorum_t));
    if (!qs->quorums) { free(qs); return NULL; }
    int line_idx = 0;
    for (int mm = 0; mm < q && line_idx < n; mm++) {
        for (int b = 0; b < q && line_idx < n; b++) {
            qs->quorums[line_idx].quorum_id = line_idx;
            qs->quorums[line_idx].weight = 1;
            int cnt = 0;
            for (int x = 0; x < q; x++) {
                int y = (mm * x + b) % q;
                qs->quorums[line_idx].members[cnt++] = x * q + y;
            }
            qs->quorums[line_idx].members[cnt++] = q * q + mm;
            qs->quorums[line_idx].num_members = cnt;
            line_idx++;
        }
    }
    for (int a = 0; a < q && line_idx < n; line_idx++, a++) {
        qs->quorums[line_idx].quorum_id = line_idx;
        qs->quorums[line_idx].weight = 1;
        int cnt = 0;
        for (int y = 0; y < q; y++)
            qs->quorums[line_idx].members[cnt++] = a * q + y;
        qs->quorums[line_idx].members[cnt++] = q * q + q;
        qs->quorums[line_idx].num_members = cnt;
    }
    if (line_idx < n) {
        qs->quorums[line_idx].quorum_id = line_idx;
        qs->quorums[line_idx].weight = 1;
        int cnt = 0;
        for (int i = 0; i <= q; i++)
            qs->quorums[line_idx].members[cnt++] = q * q + i;
        qs->quorums[line_idx].num_members = cnt;
    }
    return qs;
}

bool quorum_has_intersection(quorum_system_t *qs)
{
    if (!qs || !qs->quorums) return false;
    for (int i = 0; i < qs->num_quorums; i++) {
        for (int j = i + 1; j < qs->num_quorums; j++) {
            bool intersects = false;
            for (int a = 0; a < qs->quorums[i].num_members && !intersects; a++) {
                for (int b = 0; b < qs->quorums[j].num_members; b++) {
                    if (qs->quorums[i].members[a] == qs->quorums[j].members[b]) {
                        intersects = true; break;
                    }
                }
            }
            if (!intersects) return false;
        }
    }
    return true;
}

bool quorum_has_byzantine_intersection(quorum_system_t *qs, int f)
{
    if (!qs || !qs->quorums || f < 0) return false;
    for (int i = 0; i < qs->num_quorums; i++) {
        for (int j = i + 1; j < qs->num_quorums; j++) {
            int common = 0;
            for (int a = 0; a < qs->quorums[i].num_members; a++) {
                for (int b = 0; b < qs->quorums[j].num_members; b++) {
                    if (qs->quorums[i].members[a] == qs->quorums[j].members[b]) {
                        common++; break;
                    }
                }
            }
            if (common < 2 * f + 1) return false;
        }
    }
    return true;
}

double quorum_compute_load(quorum_system_t *qs, const double *access_probs)
{
    if (!qs || !qs->quorums) return -1.0;
    double *server_load = (double *)calloc((size_t)qs->total_processes, sizeof(double));
    if (!server_load) return -1.0;
    double uniform_prob = access_probs ? 0.0 : (1.0 / qs->num_quorums);
    for (int qi = 0; qi < qs->num_quorums; qi++) {
        double prob = access_probs ? access_probs[qi] : uniform_prob;
        for (int m = 0; m < qs->quorums[qi].num_members; m++)
            server_load[qs->quorums[qi].members[m]] += prob;
    }
    double max_load = 0.0;
    for (int i = 0; i < qs->total_processes; i++)
        if (server_load[i] > max_load) max_load = server_load[i];
    free(server_load);
    return max_load;
}

double quorum_compute_availability(quorum_system_t *qs, double failure_prob)
{
    if (!qs || failure_prob < 0.0 || failure_prob > 1.0) return -1.0;
    double prob_alive = 1.0 - failure_prob;
    double prob_all_dead = 1.0;
    for (int i = 0; i < qs->num_quorums; i++) {
        double prob_quorum_alive = 1.0;
        for (int m = 0; m < qs->quorums[i].num_members; m++)
            prob_quorum_alive *= prob_alive;
        prob_all_dead *= (1.0 - prob_quorum_alive);
    }
    return 1.0 - prob_all_dead;
}

double quorum_compute_capacity(quorum_system_t *qs)
{
    if (!qs) return -1.0;
    double load = quorum_compute_load(qs, NULL);
    if (load <= 0.0) return -1.0;
    return 1.0 / load;
}

int quorum_read(quorum_system_t *qs, int *server_values, int *server_clocks)
{
    if (!qs || !server_values) return -1;
    int best_value = -1, best_clock = -1;
    quorum_t *q = &qs->quorums[0];
    for (int i = 0; i < q->num_members; i++) {
        int sid = q->members[i];
        if (server_clocks && server_clocks[sid] > best_clock) {
            best_clock = server_clocks[sid];
            best_value = server_values[sid];
        }
    }
    return best_value;
}

int quorum_write(quorum_system_t *qs, int *server_values, int *server_clocks, int value_to_write)
{
    if (!qs || !server_values) return -1;
    int max_clock = 0;
    quorum_t *q = &qs->quorums[0];
    for (int i = 0; i < q->num_members; i++) {
        int sid = q->members[i];
        if (server_clocks && server_clocks[sid] > max_clock) max_clock = server_clocks[sid];
    }
    int new_clock = max_clock + 1;
    for (int i = 0; i < q->num_members; i++) {
        int sid = q->members[i];
        server_values[sid] = value_to_write;
        if (server_clocks) server_clocks[sid] = new_clock;
    }
    return new_clock;
}

void quorum_system_free(quorum_system_t *qs)
{
    if (!qs) return;
    if (qs->quorums) free(qs->quorums);
    free(qs);
}
