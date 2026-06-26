/*
 * distributed_constants.h -- Constants and Configuration for Distributed Algorithms
 *
 * L1 Definitions:
 *   - Network size n = |V|: total number of nodes
 *   - Maximum degree Delta = max deg(v)
 *   - Node ID space: typically [n^c] for constant c>1
 *   - Unique identifiers: every node has a distinct ID
 *
 * L2 Core Concepts:
 *   - Knowledge: what each node knows initially (n, Delta, own ID)
 *   - KT0 model: nodes know n and Delta
 *   - KT1 model: nodes know their neighbors' IDs
 *   - KTinf model: nodes know the entire graph topology
 *
 * Reference: Peleg (2000), Suomela (2020)
 */

#ifndef DISTRIBUTED_CONSTANTS_H
#define DISTRIBUTED_CONSTANTS_H

#include <stdint.h>

/* Standard distributed computing constants */
#define MAX_NODES              100000
#define MAX_DEGREE             10000
#define MAX_ROUNDS             1000000
#define MAX_COLORS             256
#define MAX_MESSAGE_SIZE       65536
#define MAX_NEIGHBORHOOD_RADIUS 100
#define INVALID_COLOR           (-1)
#define INVALID_NODE            (-1)
#define INVALID_PORT            (-1)

/* Knowledge model for distributed nodes */
typedef enum {
    KT0 = 0,   /* node knows n, Delta, own ID only */
    KT1,       /* node knows neighbors IDs + above */
    KTINF      /* node knows entire graph */
} KnowledgeModel;

/* Termination mode */
typedef enum {
    TERM_LOCAL = 0,   /* node terminates independently */
    TERM_GLOBAL,      /* all nodes terminate together */
    TERM_AFTER_OUTPUT /* terminate after producing output */
} TerminationMode;

/* Port numbering convention */
typedef enum {
    PORT_ARBITRARY = 0,  /* arbitrary but fixed */
    PORT_CONSECUTIVE,    /* ports numbered 0..deg(v)-1 */
    PORT_CONSISTENT,     /* (u,v) same port at both ends */
    PORT_CLOCKWISE       /* cyclic order for planar graphs */
} PortConvention;

#endif /* DISTRIBUTED_CONSTANTS_H */
