/**
 * @file blockchain_consensus.c
 * @brief Blockchain consensus mechanisms analyzed through the lens of
 *        Byzantine agreement theory.
 *
 * Knowledge coverage:
 *   L7: Applications — Proof-of-Work as randomized Byzantine agreement
 *   L7: Applications — Tendermint/PBFT in blockchain (Cosmos, etc.)
 *   L7: Applications — Nakamoto consensus analysis
 *   L8: Advanced — Finality gadgets and hybrid consensus
 *   L8: Advanced — Economic security and the N/3 bound in PoS
 *
 * Reference: Nakamoto (2008) "Bitcoin: A Peer-to-Peer Electronic Cash System"
 *            Garay, Kiayias, Leonardos (2015) "The Bitcoin Backbone Protocol"
 *            Buterin & Griffith (2017) "Casper the Friendly Finality Gadget"
 *            Buchman (2016) "Tendermint: Byzantine Fault Tolerance in the Age
 *            of Blockchains"
 */

#include "byzantine_agreement.h"
#include "consensus_types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ================================================================
 * L7: Nakamoto Consensus as Probabilistic Byzantine Agreement
 *
 * Bitcoin's Proof-of-Work (PoW) consensus is a probabilistic solution
 * to Byzantine agreement in a permissionless, asynchronous setting.
 *
 * Key insight: Instead of requiring deterministic agreement in bounded
 * time (which is impossible by FLP), Nakamoto consensus provides
 * agreement with high probability that increases as more blocks are
 * added on top of a transaction.
 *
 * The "longest chain rule" is an emergent consensus mechanism where
 * the probability of reversal decays exponentially with the number
 * of confirmations.
 *
 * Security assumption: honest miners control > 50% of hash power.
 * This is the f < N/2 bound in a different guise.
 * ================================================================ */

/**
 * @brief Nakamoto consensus parameters.
 */
typedef struct {
    double   honest_hash_fraction;    /**< α, fraction of hash power honest */
    double   adversarial_hash_fraction; /**< β = 1 - α */
    int32_t  target_confirmations;     /**< k, confirmations for "finality" */
    double   safety_threshold;         /**< ε, acceptable error probability */
    int32_t  block_time_seconds;       /**< Average block interval */
} nakamoto_params_t;

/**
 * @brief Compute the probability that an adversary with hash fraction q
 *        can overtake a chain of z blocks mined by honest miners with
 *        hash fraction p.
 *
 * Nakamoto's analysis uses the Gambler's Ruin problem:
 *   P[adversary overtakes z-block lead] = 
 *     Σ_{k=0}^{∞} Poisson(k; z*q/p) * (1 - (q/p)^{z-k})  if q < p
 *     = 1                                                      if q ≥ p
 *
 * Where Poisson(k; λ) = λ^k * e^{-λ} / k!
 *
 * @param q Adversary hash fraction.
 * @param z Number of confirmations (blocks behind).
 * @return Probability of successful double-spend.
 *
 * Complexity: O(z) to compute the truncated sum
 * Reference: Nakamoto (2008) §11 "Calculations"
 */
double nakamoto_attack_success_probability(double q, int32_t z)
{
    /* The classical result from the Bitcoin whitepaper */
    if (q <= 0.0 || q >= 1.0) return (q >= 0.5) ? 1.0 : 0.0;

    double p = 1.0 - q;
    if (q >= p) return 1.0;  /* Adversary has majority: can always overtake */

    double lambda = (double)z * q / p;
    double sum = 1.0;  /* P = 1 - Σ ... */

    /* Poisson tail probability computation */
    double poisson_k = exp(-lambda);  /* k=0 term */
    double qp_ratio = q / p;

    for (int32_t k = 0; k <= z; k++) {
        double term = poisson_k * (1.0 - pow(qp_ratio, (double)(z - k)));
        sum -= term;

        if (k < z) {
            poisson_k *= lambda / (double)(k + 1);
        }
    }

    /* For numerical stability, clamp to [0, 1] */
    if (sum < 0.0) sum = 0.0;
    if (sum > 1.0) sum = 1.0;

    return sum;
}

/**
 * @brief Determine minimum confirmations needed for a given security level.
 *
 * Given adversary hash fraction q and desired safety ε, find the
 * minimum k such that P[attack success] ≤ ε.
 *
 * @param q      Adversary hash fraction.
 * @param epsilon Desired safety level (e.g., 1e-6).
 * @return Minimum confirmations needed.
 */
int32_t nakamoto_min_confirmations(double q, double epsilon)
{
    if (q >= 0.5) return INT32_MAX;  /* Impossible to secure */
    if (epsilon <= 0.0 || epsilon >= 1.0) return -1;

    for (int32_t k = 1; k <= 1000; k++) {
        double prob = nakamoto_attack_success_probability(q, k);
        if (prob <= epsilon) return k;
    }

    return -1;  /* Not found within reasonable range */
}

/**
 * @brief Analyze the relationship between the N/3 Byzantine bound and
 *        Nakamoto consensus's N/2 hash power bound.
 *
 * In PBFT/BFT: f < N/3 is required because faulty processes can
 * equivocate (send conflicting messages to different processes),
 * effectively creating a "split-brain" scenario with only N-2f
 * correct processes able to form a quorum of 2f+1.
 *
 * In Nakamoto consensus: The adversary can equivocate, but PoW
 * introduces an economic cost. The longest chain rule ensures
 * that only one chain survives. The bound becomes f < N/2 because
 * the adversary must outcompete the honest majority in expectation.
 *
 * However, with network delays, the bound degrades to f < N/3
 * in formal analyses (Garay et al. 2015).
 */
typedef struct {
    double   f_over_n;             /**< f/N ratio */
    bool     bft_possible;         /**< Classical BFT possible? */
    bool     nakamoto_possible;    /**< Nakamoto consensus possible? */
    double   nakamoto_safety_6conf; /**< Safety after 6 confirmations */
    bool     hybrid_possible;      /**< Hybrid (Tendermint) possible? */
} consensus_feasibility_t;

/**
 * @brief Evaluate consensus feasibility across different models.
 *
 * @param f                Number of faulty validators.
 * @param N                Total validators.
 * @param network_delay_s  Average network delay in seconds.
 * @param block_time_s     Block time in seconds.
 * @param result           Output feasibility analysis.
 * @return 0 on success.
 */
int consensus_feasibility_analysis(int32_t f, int32_t N,
                                    double network_delay_s,
                                    double block_time_s,
                                    consensus_feasibility_t *result)
{
    if (!result || N < 1) return -1;

    memset(result, 0, sizeof(*result));
    result->f_over_n = (double)f / (double)N;

    /* Classical BFT: f < N/3 */
    result->bft_possible = (3 * f < N);

    /* Nakamoto: adversary fraction < 0.5 for safety */
    double adv_frac = result->f_over_n;
    result->nakamoto_possible = (adv_frac < 0.5);
    result->nakamoto_safety_6conf = nakamoto_attack_success_probability(adv_frac, 6);

    /* Hybrid (Tendermint): f < N/3, but with block production,
     * can tolerate temporary asynchrony */
    result->hybrid_possible = (3 * f < N) && (block_time_s > 3.0 * network_delay_s);

    return 0;
}

/* ================================================================
 * L8: Gasper — the Ethereum 2.0 Finality Gadget
 *
 * Gasper combines:
 *   - LMD-GHOST (Latest Message Driven Greedy Heaviest Observed SubTree)
 *     for chain selection (availability under asynchrony).
 *   - Casper FFG (Friendly Finality Gadget) for finality (consistency).
 *
 * This is a prime example of L8 (Advanced Topics): hybrid consensus
 * that separates liveness from safety.
 * ================================================================ */

typedef struct {
    int32_t epoch;                     /**< Current epoch */
    int32_t justified_epoch;           /**< Highest justified epoch */
    int32_t finalized_epoch;           /**< Highest finalized epoch */
    int32_t validator_count;
    int32_t committee_size;
    double   two_thirds_stake;         /**< 2/3 of total stake */
    /* Voting weights */
    double   votes_for_epoch[1024];    /**< Stake-weighted votes per epoch */
    int32_t  max_epoch;
} gasper_state_t;

/**
 * @brief Initialize a Gasper-like finality gadget.
 *
 * @param gs          State to initialize.
 * @param num_validators Number of validators.
 * @return 0 on success.
 */
int gasper_init(gasper_state_t *gs, int32_t num_validators)
{
    if (!gs || num_validators < 1) return -1;

    memset(gs, 0, sizeof(*gs));
    gs->validator_count = num_validators;
    gs->committee_size = num_validators;  /* Simplified: all validators in committee */
    gs->two_thirds_stake = (2.0 / 3.0) * (double)num_validators;
    gs->epoch = 0;
    gs->justified_epoch = 0;
    gs->finalized_epoch = 0;
    gs->max_epoch = 1024;

    return 0;
}

/**
 * @brief Process votes for a checkpoint in an epoch.
 *
 * In Casper FFG, validators vote for checkpoints (source, target).
 * If a supermajority (≥ 2/3) votes for (source, target), then target
 * becomes justified. If target is justified and its child is justified,
 * then target becomes finalized.
 *
 * @param gs          Gasper state.
 * @param target_epoch The epoch being voted on as target.
 * @param votes_for   Number of validators voting for this target.
 * @return 0 if nothing new, 1 if new justification, 2 if new finalization.
 */
int gasper_process_votes(gasper_state_t *gs, int32_t target_epoch,
                          double votes_for)
{
    if (!gs) return -1;

    /* Check for supermajority */
    if (votes_for >= gs->two_thirds_stake) {
        /* Justify this epoch */
        if (target_epoch > gs->justified_epoch) {
            gs->justified_epoch = target_epoch;
            gs->votes_for_epoch[target_epoch % gs->max_epoch] = votes_for;

            /* Finalization rule: if we have consecutive justified epochs,
             * the earlier one is finalized */
            if (target_epoch == gs->justified_epoch &&
                gs->justified_epoch > gs->finalized_epoch + 1) {
                gs->finalized_epoch = gs->justified_epoch - 1;
                return 2;  /* New finalization */
            }
            return 1;  /* New justification */
        }
    }

    return 0;
}

/**
 * @brief Check if an epoch is finalized (irreversible).
 *
 * @param gs    Gasper state.
 * @param epoch Epoch to check.
 * @return true if the epoch is finalized.
 */
bool gasper_is_finalized(const gasper_state_t *gs, int32_t epoch)
{
    if (!gs) return false;
    return epoch <= gs->finalized_epoch;
}

/**
 * @brief The accountable safety property of Casper FFG:
 *        If two conflicting checkpoints are finalized, then at least
 *        1/3 of validators violated the protocol and can be identified
 *        (their signatures are evidence).
 *
 * This is the key insight: BFT consensus under f < N/3, but with
 * accountability — if more than f are faulty, we can PROVE it
 * (unlike classical BFT where > f faults simply breaks the protocol
 * silently).
 *
 * @param total_stake Total staked amount.
 * @return Minimum stake that must be slashed if safety is violated.
 */
double gasper_accountable_safety_bound(double total_stake)
{
    /* If safety fails, at least 1/3 of stake is provably malicious */
    return total_stake / 3.0;
}

/* ================================================================
 * L7: Tendermint Consensus Analysis
 *
 * Tendermint (Buchman 2016) is a BFT consensus engine used by Cosmos.
 * It operates in rounds with a proposer selection mechanism.
 *
 * Each round:
 *   Propose:   Proposer broadcasts a block.
 *   Pre-vote:  Validators broadcast prevotes.
 *              Wait for 2/3+ prevotes or timeout.
 *   Pre-commit: If 2/3+ prevotes received, broadcast precommit.
 *              Wait for 2/3+ precommits or timeout.
 *   Commit:    If 2/3+ precommits received, commit the block.
 *
 * Safety: If a block is committed at height h, no other block can be
 * committed at height h (requires > 1/3 Byzantine to violate).
 *
 * Liveness: The protocol may need to move to a new round if the
 * proposer is faulty. Timeouts increase exponentially.
 * ================================================================ */

typedef enum {
    TENDERMINT_PROPOSE,
    TENDERMINT_PREVOTE,
    TENDERMINT_PRECOMMIT,
    TENDERMINT_COMMIT
} tendermint_step_t;

typedef struct {
    int32_t           height;
    int32_t           round;
    tendermint_step_t step;
    int32_t           N;              /**< Validators */
    int32_t           f;              /**< Max Byzantine */
    int32_t           quorum;         /**< 2f+1 */
    int32_t           proposer_id;
    int32_t           proposed_block_hash;
    int32_t           prevotes_received;
    int32_t           precommits_received;
    bool              block_committed;
    int32_t           committed_block_hash;
    double            timeout;        /**< Current round timeout */
} tendermint_state_t;

/**
 * @brief Initialize Tendermint consensus state.
 * @param tm  State to initialize.
 * @param N   Number of validators.
 * @param f   Max Byzantine faults.
 * @return 0 on success.
 */
int tendermint_init(tendermint_state_t *tm, int32_t N, int32_t f)
{
    if (!tm || N < 1 || 3 * f >= N) return -1;

    memset(tm, 0, sizeof(*tm));
    tm->N = N;
    tm->f = f;
    tm->quorum = 2 * f + 1;
    tm->height = 1;
    tm->round = 0;
    tm->step = TENDERMINT_PROPOSE;
    tm->proposer_id = 0;
    tm->timeout = 1.0;  /* Base timeout */
    tm->block_committed = false;
    tm->committed_block_hash = -1;

    return 0;
}

/**
 * @brief Advance Tendermint by one step.
 *
 * @param tm            Tendermint state.
 * @param proposer_block Proposed block hash (-1 if proposer is faulty).
 * @param honest_prevotes Number of honest validators prevoting.
 * @param honest_precommits Number of honest validators precommitting.
 * @return 0 if advancing, 1 if block committed, -1 on error.
 */
int tendermint_step(tendermint_state_t *tm,
                    int32_t proposer_block,
                    int32_t honest_prevotes,
                    int32_t honest_precommits)
{
    if (!tm) return -1;

    switch (tm->step) {
    case TENDERMINT_PROPOSE:
        tm->proposed_block_hash = proposer_block;
        tm->step = TENDERMINT_PREVOTE;
        break;

    case TENDERMINT_PREVOTE:
        tm->prevotes_received = honest_prevotes;
        if (tm->prevotes_received >= tm->quorum) {
            tm->step = TENDERMINT_PRECOMMIT;
        } else {
            /* Timeout: move to next round */
            tm->round++;
            tm->proposer_id = tm->round % tm->N;
            tm->timeout *= 2.0;  /* Exponential backoff */
            tm->step = TENDERMINT_PROPOSE;
        }
        break;

    case TENDERMINT_PRECOMMIT:
        tm->precommits_received = honest_precommits;
        if (tm->precommits_received >= tm->quorum) {
            tm->step = TENDERMINT_COMMIT;
        } else {
            tm->round++;
            tm->proposer_id = tm->round % tm->N;
            tm->timeout *= 2.0;
            tm->step = TENDERMINT_PROPOSE;
        }
        break;

    case TENDERMINT_COMMIT:
        tm->block_committed = true;
        tm->committed_block_hash = tm->proposed_block_hash;
        /* Advance to next height */
        tm->height++;
        tm->round = 0;
        tm->step = TENDERMINT_PROPOSE;
        tm->proposer_id = 0;
        tm->timeout = 1.0;
        return 1;  /* Block committed */
    }

    return 0;
}

/* ================================================================
 * L8: Economic Finality vs. Mathematical Finality
 *
 * A key distinction in blockchain consensus:
 *
 * Mathematical (absolute) finality: Once a block is finalized, no
 *   amount of adversary power can revert it. This is what BFT
 *   protocols (PBFT, Tendermint) provide under f < N/3.
 *
 * Economic finality: Reverting a block is economically irrational
 *   because it would cost more than the attacker could gain. This
 *   is what Nakamoto consensus provides with sufficient confirmations.
 *
 * Probabilistic finality: The probability of reversion decays
 *   exponentially with confirmations, but never reaches zero.
 * ================================================================ */

typedef enum {
    FINALITY_MATHEMATICAL,     /**< Impossible to revert (BFT) */
    FINALITY_ECONOMIC,         /**< Irrational to revert (Nakamoto) */
    FINALITY_PROBABILISTIC     /**< Exponentially unlikely to revert */
} finality_type_t;

typedef struct {
    finality_type_t type;
    double          safety_margin;     /**< For economic: cost/profit ratio */
    int32_t         confirmations;    /**< For probabilistic: block depth */
    double          reversion_prob;   /**< For probabilistic: P[reversion] */
} finality_analysis_t;

/**
 * @brief Analyze the finality guarantees of a blockchain protocol.
 *
 * @param f                   Byzantine validators.
 * @param N                   Total validators.
 * @param block_confirmations  Block depth.
 * @param attacker_hash_frac   Attacker's hash fraction (for PoW).
 * @param result               Output analysis.
 * @return 0 on success.
 */
int finality_analyze(int32_t f, int32_t N,
                      int32_t block_confirmations,
                      double attacker_hash_frac,
                      finality_analysis_t *result)
{
    if (!result || N < 1) return -1;

    memset(result, 0, sizeof(*result));

    if (3 * f < N && block_confirmations >= 1) {
        /* BFT: Mathematical finality after 1 block (2/3+ precommits) */
        result->type = FINALITY_MATHEMATICAL;
        result->safety_margin = (double)(N - 3 * f) / (double)N;
        result->reversion_prob = 0.0;
    } else if (attacker_hash_frac < 0.5) {
        /* Nakamoto: Probabilistic finality */
        result->type = FINALITY_PROBABILISTIC;
        result->confirmations = block_confirmations;
        result->reversion_prob = nakamoto_attack_success_probability(
            attacker_hash_frac, block_confirmations);
        if (result->reversion_prob < 1e-9) {
            result->type = FINALITY_ECONOMIC;
            result->safety_margin = -log10(result->reversion_prob);
        }
    } else {
        /* No finality: attacker has majority */
        result->type = FINALITY_PROBABILISTIC;
        result->reversion_prob = 1.0;
        result->safety_margin = 0.0;
    }

    return 0;
}

/* ================================================================
 * L8: Asynchronous Byzantine Agreement with Random Oracles
 *
 * Recent research (e.g., Algorand by Micali 2017) shows that
 * cryptographic sortition (VRF-based leader election) can achieve
 * Byzantine agreement in the asynchronous setting with high
 * probability, circumventing the FLP impossibility via a form
 * of common coin implemented through VRFs.
 *
 * This is an L8 advanced topic: the interplay between cryptography
 * and distributed consensus.
 * ================================================================ */

/**
 * @brief Simulate VRF-based leader election for Algorand-style consensus.
 *
 * Each validator computes VRF_sk(seed) and is selected as a leader if
 * the output falls below a threshold τ. The expected number of leaders
 * is τ * N.
 *
 * @param N            Number of validators.
 * @param threshold_tau Desired fraction of leaders.
 * @param seed         Random seed for the round.
 * @param selected     Output: bitmap of selected validators.
 * @param num_selected Output: number of selected validators.
 * @return 0 on success.
 */
int vrf_leader_election(int32_t N, double threshold_tau,
                         uint64_t seed, uint64_t *selected,
                         int32_t *num_selected)
{
    if (!selected || !num_selected || N < 1) return -1;
    if (threshold_tau <= 0.0 || threshold_tau > 1.0) return -1;

    *num_selected = 0;
    uint64_t threshold_val = (uint64_t)(threshold_tau * (double)UINT64_MAX);

    /* Simple LCG-based VRF simulation */
    for (int32_t i = 0; i < N; i++) {
        uint64_t hash = (seed + (uint64_t)i * 6364136223846793005ULL) ^
                        ((uint64_t)i << 32);
        hash = hash * 0x9E3779B97F4A7C15ULL;
        hash = hash ^ (hash >> 30);
        hash = hash * 0xBF58476D1CE4E5B9ULL;

        if (hash < threshold_val) {
            selected[(*num_selected)++] = (uint64_t)i;
        }
    }

    return 0;
}

/**
 * @brief Verify that the number of honest leaders is sufficient for
 *        the Algorand BA★ protocol to reach agreement.
 *
 * BA★ requires that the honest majority of the selected committee
 * can outvote any Byzantine minority. If the expected committee size
 * is τ*N, then we need:
 *   honest_selected > 2 * byzantine_selected
 *
 * Which holds with high probability when honest stake > 2/3
 * (the f < N/3 bound appears again in a new guise).
 *
 * @param N                    Total users.
 * @param honest_fraction      Fraction of honest stake.
 * @param threshold_tau        Selection threshold.
 * @param safety_prob          Output: probability that the condition holds.
 * @return 0 on success.
 */
int algorand_safety_check(int32_t N, double honest_fraction,
                           double threshold_tau, double *safety_prob)
{
    if (!safety_prob || N < 1) return -1;

    /* Expected committee size */
    double expected_committee = threshold_tau * (double)N;

    /* Expected honest and Byzantine in committee */
    double expected_honest = honest_fraction * expected_committee;

    /* Safety: need honest_selected > 2 * byzantine_selected
     * Using Chernoff bounds, the failure probability is approximately
     * exp(-expected_honest * (1 - 2*(1-honest_fraction)/honest_fraction)^2 / 3)
     * when honest_fraction > 2/3. */

    if (honest_fraction <= 2.0 / 3.0) {
        *safety_prob = 0.0;  /* Cannot guarantee safety */
        return 0;
    }

    double mu = expected_honest;
    double delta = 1.0 - (2.0 * (1.0 - honest_fraction) / honest_fraction);
    if (delta <= 0.0) {
        *safety_prob = 0.0;
        return 0;
    }

    /* Chernoff bound: P[X ≤ (1-δ)μ] ≤ exp(-μ*δ²/2) */
    double chernoff = exp(-mu * delta * delta / 2.0);
    *safety_prob = 1.0 - chernoff;

    if (*safety_prob > 1.0) *safety_prob = 1.0;
    if (*safety_prob < 0.0) *safety_prob = 0.0;

    return 0;
}

/* ================================================================
 * L8: Cross-chain Atomic Swaps and Byzantine Agreement
 *
 * HTLC (Hashed TimeLock Contracts) enable cross-chain atomic swaps
 * without a trusted third party. This is a practical application of
 * the impossibility results: since two chains cannot directly
 * coordinate (asynchrony), they use cryptographic commitments
 * and timeouts to achieve atomicity.
 *
 * This connects to L7 (Applications) and L8 (Advanced Topics).
 * ================================================================ */

typedef struct {
    uint64_t hash_lock;          /**< H = hash(preimage) */
    uint64_t preimage;           /**< Secret that unlocks both sides */
    int32_t  timeout_chain_a;    /**< Block height deadline on chain A */
    int32_t  timeout_chain_b;    /**< Block height deadline on chain B */
    bool     chain_a_claimed;    /**< Has Alice claimed on chain A? */
    bool     chain_b_claimed;    /**< Has Bob claimed on chain B? */
    bool     refunded_a;         /**< Has Alice refunded on chain A? */
    bool     refunded_b;         /**< Has Bob refunded on chain B? */
} htlc_atomic_swap_t;

/**
 * @brief Simulate the HTLC atomic swap protocol.
 *
 * Protocol:
 *   1. Alice generates preimage s, computes H = hash(s).
 *   2. Alice locks funds on chain A with H, timeout T_A.
 *   3. Bob locks funds on chain B with H, timeout T_B (T_B < T_A).
 *   4. Alice claims on chain B, revealing s.
 *   5. Bob sees s on chain B, claims on chain A using s.
 *
 * Safety: Either both claims succeed (atomicity) or both refund.
 *
 * @param swap         Swap state.
 * @param alice_claims  Alice reveals preimage on chain B.
 * @param bob_claims    Bob reveals preimage on chain A.
 * @return true if the swap is atomically correct.
 */
bool htlc_atomic_swap_execute(htlc_atomic_swap_t *swap,
                               bool alice_claims, bool bob_claims)
{
    if (!swap) return false;

    /* Reset state */
    swap->chain_a_claimed = false;
    swap->chain_b_claimed = false;
    swap->refunded_a = false;
    swap->refunded_b = false;

    /* Alice claims on chain B (reveals preimage) */
    if (alice_claims) {
        swap->chain_b_claimed = true;
        /* Bob learns preimage from chain B and claims on chain A */
        if (bob_claims) {
            swap->chain_a_claimed = true;
        }
    }

    /* Check timeouts: if timeout exceeded without claim, refund */
    if (!swap->chain_b_claimed) {
        swap->refunded_b = true;  /* Bob refunds on chain B after T_B */
    }
    if (!swap->chain_a_claimed) {
        swap->refunded_a = true;  /* Alice refunds on chain A after T_A */
    }

    /* Atomicity: both claimed OR both refunded */
    bool atomic = (swap->chain_a_claimed && swap->chain_b_claimed) ||
                  (swap->refunded_a && swap->refunded_b);

    return atomic;
}

/**
 * @brief Demonstrate the connection between HTLC safety and the
 *        byzantine agreement lower bound.
 *
 * The HTLC protocol achieves atomicity without a consensus protocol
 * between the two chains. This works because:
 *   - Cryptographic commitments replace communication (signature model)
 *   - Timeouts provide a "failure detector" (partial synchrony)
 *   - Only 2 parties participate (not N > 3f)
 *
 * If we try to extend this to N parties, we need consensus, and
 * the lower bound f < N/3 reappears.
 *
 * @return Explanation string explaining the connection.
 */
const char *htlc_connection_to_byzantine_bound(void)
{
    return "HTLC achieves cross-chain atomicity for 2 parties without "
           "consensus. For N-party cross-chain swaps, a consensus protocol "
           "is needed between the chains. This requires satisfying the "
           "Byzantine agreement lower bound (f < N/3 for oral, f < N/2 for "
           "signed). The HTLC hash lock effectively provides unforgeable "
           "signatures (signed model), making the bound f < N/2. "
           "When extending to N parties with hash locks only (no signatures), "
           "the oral message bound f < N/3 applies, explaining why "
           "decentralized cross-chain protocols become complex.";
}
