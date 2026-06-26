/*
 * quorum.h - Quorum Systems for Fault-Tolerant Consensus
 * L3: Quorum theory (Malkhi & Reiter 1998).
 * L4: Intersection property for consensus safety.
 * Reference: Malkhi & Reiter (1998) "Byzantine Quorum Systems"
 */
#ifndef QUORUM_H
#define QUORUM_H
#include "consensus_types.h"
#include <stdint.h>
#include <stdbool.h>

quorum_system_t *quorum_create_majority(int n);
quorum_system_t *quorum_create_grid(int grid_side);
quorum_system_t *quorum_create_b_masking(int n, int f);
quorum_system_t *quorum_create_fpp(int q);
bool quorum_has_intersection(quorum_system_t *qs);
bool quorum_has_byzantine_intersection(quorum_system_t *qs, int f);
double quorum_compute_load(quorum_system_t *qs, const double *access_probs);
double quorum_compute_availability(quorum_system_t *qs, double failure_prob);
double quorum_compute_capacity(quorum_system_t *qs);
int  quorum_read(quorum_system_t *qs, int *server_values, int *server_clocks);
int  quorum_write(quorum_system_t *qs, int *server_values, int *server_clocks, int value_to_write);
void quorum_system_free(quorum_system_t *qs);

#endif /* QUORUM_H */
