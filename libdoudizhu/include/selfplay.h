/**
 * @file selfplay.h
 * @brief Self-play arena and weight tuning helpers for libdoudizhu bots.
 * @author Jonathan
 * @date 09-Mar-26
 */

#ifndef LIBDOUDIZHU_SELFPLAY_H
#define LIBDOUDIZHU_SELFPLAY_H

#include "tune.h"

/**
 * @struct SelfPlayStats
 * @brief Aggregate results from a weighted self-play run.
 */
typedef struct {
    int requested_games; /**< Target number of completed games. */
    int completed_games; /**< Games that reached PHASE_OVER. */
    int skipped_games; /**< Deals skipped because bidding produced no landlord. */
    int seat_wins[GAME_NUM_PLAYERS]; /**< Win count for each seat index. */
    int team_wins[GAME_NUM_PLAYERS]; /**< Team wins credited to each seat's team. */
    int landlord_wins; /**< Number of games won by the landlord seat. */
    int peasant_wins; /**< Number of games won by the peasant team. */
    int total_score; /**< Sum of final doubled scores across completed games. */
} SelfPlayStats;

/**
 * @struct SelfPlayTuneConfig
 * @brief Controls the hill-climbing self-play tuning loop.
 */
typedef struct {
    int generations; /**< Number of candidate mutations to evaluate. */
    int games_per_generation; /**< Games used to score each candidate. */
    /**
     * Initial mutation size as a percentage (0–100) of each field's sensible
     * range (from botWeightsMin / botWeightsMax).  Anneals linearly to 1%
     * over the run.  A value of 20 means the first mutations move each field
     * by ≈20 % of its range, giving coarse exploration early and fine-tuning
     * late.  Because it is relative to per-field ranges, the same value works
     * well for both narrow fields (pos_lead_divisor: 1–10) and wide ones
     * (break_combo_penalty: 0–1000).
     */
    int initial_step;
    unsigned int seed; /**< PRNG seed for mutation and match generation. */
} SelfPlayTuneConfig;

/**
 * @struct SelfPlayTuneResult
 * @brief Outcome of a self-play tuning run.
 */
typedef struct {
    BotWeights initial_weights; /**< Starting point before tuning. */
    BotWeights best_weights; /**< Final incumbent after tuning. */
    SelfPlayStats final_arena; /**< Final incumbent vs default arena result. */
    int generations_attempted; /**< Number of candidate mutations evaluated. */
    int generations_accepted; /**< Number of mutations that replaced incumbent. */
    int last_field_index; /**< Index of the most recently tested field. */
    int last_step; /**< Absolute step used in the most recent mutation. */
} SelfPlayTuneResult;

/**
 * @brief Fills stats with zeros and sets requested_games.
 * @param stats Stats object to reset.
 * @param requested_games Target number of games for the next run.
 */
void selfplayStatsReset(SelfPlayStats* stats, int requested_games);

/**
 * @brief Runs weighted bots against each other until num_games complete.
 * @param num_games Number of completed games to collect.
 * @param base_seed Starting shuffle seed.
 * @param per_player_weights Weight set for each seat; NULL entry uses defaults.
 * @param stats_out Filled with aggregate results.
 * @details The starting bidder rotates by game index to reduce seat bias.
 */
void selfplayRunMatches(int num_games, unsigned int base_seed, const BotWeights* per_player_weights[GAME_NUM_PLAYERS],
                        SelfPlayStats* stats_out);

/**
 * @brief Tunes one BotWeights vector through slow self-play hill climbing.
 * @param initial_weights Starting weights; NULL uses defaults.
 * @param cfg Tuning configuration; NULL uses conservative defaults.
 * @param result_out Filled with the final incumbent and summary stats.
 * @details Each generation mutates one field, runs the candidate in one seat
 *          against two incumbent/default opponents, rotates the candidate seat,
 *          and accepts only strictly better candidates.
 */
void selfplayTune(const BotWeights* initial_weights, const SelfPlayTuneConfig* cfg, SelfPlayTuneResult* result_out);

#endif // LIBDOUDIZHU_SELFPLAY_H
