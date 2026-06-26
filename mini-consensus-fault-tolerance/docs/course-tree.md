# Course Tree: Consensus and Fault Tolerance

## Prerequisites

Distributed Systems Fundamentals
  --> Fault models (crash, Byzantine)
  --> Message passing
  --> Synchronous vs asynchronous

## Core Consensus

Consensus Problem Definition
  +-- Properties: Agreement, Validity, Termination
  +-- Impossibility: FLP (1985)

## Crash Fault Tolerance (n > 2f)

  +-- Paxos (Lamport 1998)
  |     +-- Multi-Paxos
  |     +-- Leader election optimization
  +-- Raft (Ongaro 2014)
        +-- Leader election
        +-- Log replication
        +-- Membership changes

## Byzantine Fault Tolerance (n > 3f)

  +-- Oral Messages OM(m)
  +-- Signed Messages SM(m)
  +-- PBFT (Castro and Liskov 1999)
        +-- Three-phase protocol
        +-- View change
        +-- Checkpointing

## Quorum Theory

  +-- Majority quorums
  +-- Grid quorums
  +-- B-Masking quorums
  +-- FPP quorums

## Blockchain Consensus

  +-- Nakamoto consensus (PoW)
  +-- Longest chain rule
  +-- Probabilistic finality
  +-- Selfish mining

## Reading Order

1. consensus_types.h/c - Foundation
2. consensus_model.h/c - System models, FLP
3. quorum.h/c - Quorum theory
4. paxos.h/c - Crash fault consensus
5. raft.h/c - Understandable consensus
6. byzantine_agreement.c - Byzantine Generals
7. pbft.h/c - Practical BFT
8. nakamoto.h/c - Blockchain consensus

## Key Papers

1. Lamport, Shostak, Pease (1982) - Byzantine Generals Problem
2. Fischer, Lynch, Paterson (1985) - FLP Impossibility
3. Dwork, Lynch, Stockmeyer (1988) - Partial Synchrony
4. Lamport (1998) - The Part-Time Parliament
5. Lamport (2001) - Paxos Made Simple
6. Castro and Liskov (1999) - PBFT
7. Ongaro and Ousterhout (2014) - Raft
8. Nakamoto (2008) - Bitcoin
9. Malkhi and Reiter (1998) - Byzantine Quorum Systems
10. Eyal and Sirer (2014) - Selfish Mining
