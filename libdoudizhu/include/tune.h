/**
 * @file tune.h
 * @brief Weight introspection, mutation, and self-play simulation for bot tuning.
 * @author Jonathan
 * @date 08-Mar-26
 */

#ifndef LIBDOUDIZHU_TUNE_H
#define LIBDOUDIZHU_TUNE_H

#include "bot.h"

/**
 * @brief Initializes a BotWeights struct with the default values.
 * @param out Destination to fill; must not be NULL.
 * @details Always call this (or copy from bot_default_weights()) before
 *          using bot_weights_set() to tweak individual fields.
 */
void bot_weights_init(BotWeights * out);

/**
 * @brief Returns the number of integer fields in BotWeights.
 * @return Count of tunable weight entries.
 * @details BotWeights is intentionally all-integer, so generic tuning code can
 *          index into it without bespoke per-field logic.
 */
int bot_weights_count(void);

/**
 * @brief Returns a stable field name for a BotWeights index.
 * @param index Zero-based field index in [0, bot_weights_count()).
 * @return Pointer to a static string, or NULL for an invalid index.
 */
const char *bot_weights_name(int index);

/**
 * @brief Copies a BotWeights field out by index.
 * @param weights Source weights.
 * @param index Zero-based field index.
 * @return Field value, or 0 if the index is invalid.
 */
int bot_weights_get(const BotWeights *weights, int index);

/**
 * @brief Writes a BotWeights field by index.
 * @param weights Destination weights.
 * @param index Zero-based field index.
 * @param value New field value.
 * @return 1 on success, 0 on invalid index or NULL weights.
 */
int bot_weights_set(BotWeights *weights, int index, int value);

/**
 * @brief Returns the minimum sensible value for a BotWeights field.
 * @param index Zero-based field index in [0, bot_weights_count()).
 * @return Minimum value, or 0 for an invalid index.
 * @details Use these bounds in tuning/arena code instead of a flat [0, N]
 *          range.  Fields such as pos_lead_divisor have a minimum of 1 to
 *          avoid divide-by-zero; count fields like void_threshold are capped
 *          well below 1000.
 */
int bot_weights_min(int index);

/**
 * @brief Returns the maximum sensible value for a BotWeights field.
 * @param index Zero-based field index in [0, bot_weights_count()).
 * @return Maximum value, or 0 for an invalid index.
 */
int bot_weights_max(int index);

/**
 * @brief Runs a series of complete bot-vs-bot games and tallies winner counts.
 * @param num_games Number of games to simulate.
 * @param base_seed Starting PRNG seed; game i uses seed (base_seed + i).
 * @param wins_out Array of size GAME_NUM_PLAYERS filled with per-player win counts.
 * @details Games where every player passes during bidding (no landlord) are
 *          skipped and do not count toward num_games.
 */
void bot_simulate(int num_games, unsigned int base_seed, int wins_out[GAME_NUM_PLAYERS]);

/**
 * @brief Evaluates candidate weights against default-weight opponents.
 *
 * Runs two independent series of num_games games each:
 *  - Series A: candidate in seat 0 (landlord seat), default in seats 1 & 2.
 *  - Series B: default in seat 0, candidate in seats 1 & 2 (peasant team).
 *
 * @param candidate        Weights being evaluated; NULL uses defaults.
 * @param num_games        Games per series (total games run = 2 * num_games).
 * @param base_seed        PRNG seed for series A; series B uses base_seed + num_games.
 * @param wins_as_landlord Filled with candidate win count from series A (out of num_games).
 * @param wins_as_peasant  Filled with peasant-team win count from series B (out of num_games).
 */
void bot_evaluate_weights(const BotWeights *candidate, int num_games, unsigned int base_seed,
                          int *wins_as_landlord, int *wins_as_peasant);

/**
 * @brief Weight-configurable variant of bot_simulate.
 * @param num_games Number of games to simulate.
 * @param base_seed Starting PRNG seed; game i uses seed (base_seed + i).
 * @param per_player_weights Weight set for each seat; NULL entry uses defaults.
 * @param wins_out Array of size GAME_NUM_PLAYERS filled with per-player win counts.
 */
void bot_simulate_with_weights(int num_games, unsigned int base_seed,
                               const BotWeights *per_player_weights[GAME_NUM_PLAYERS],
                               int wins_out[GAME_NUM_PLAYERS]);

#endif //LIBDOUDIZHU_TUNE_H
