/**
 * @file bot.h
 * @brief Rule-based AI player for libdoudizhu
 * @author Jonathan
 * @date 08-Mar-26
 */

#ifndef LIBDOUDIZHU_BOT_H
#define LIBDOUDIZHU_BOT_H

#include "game.h"

/**
 * @struct BotWeights
 * @brief All tunable scoring constants for the bot (Goal 11).
 *
 * Consolidating every magic number into this struct makes it straightforward
 * to run self-play experiments: swap in a different BotWeights and compare
 * win-rates without touching any logic.
 */
typedef struct {
    /* --- Leading weights --- */
    int clear_per_card; /**< Base clearing bonus per card played (15) */
    int endgame_clear; /**< Extra clearing bonus per card in endgame (10) */
    int chain_length; /**< Bonus per unit of chain length for straights/airplanes (4) */
    int feeder_penalty; /**< Feeder: per-card penalty for leading big combos (3) */
    int pos_lead_divisor; /**< Divide post-move position score for leads (3) */

    /* --- Response weights --- */
    int pos_resp_divisor; /**< Divide post-move position score for responses (4) */
    int danger_per_card; /**< Per-card bonus when an enemy is near winning (10) */
    int gatekeeper_bonus; /**< Gatekeeper blocking the landlord's lead (20) */
    int ctrl_2_penalty; /**< Penalty for using a 2 as a response card (35) */
    int ctrl_sj_penalty; /**< Penalty for using the small joker as a response (50) */
    int ctrl_bj_penalty; /**< Penalty for using the big joker as a response (60) */
    int must_stop_multiplier; /**< Score multiplier for eval_move_cost in must-stop (2) */

    /* --- Combination integrity --- */
    int break_combo_penalty; /**< Penalty for breaking a latent bomb or rocket (300) */
    int kicker_joker; /**< Joker used as kicker attachment (80) */
    int kicker_2; /**< 2 used as kicker attachment (50) */
    int kicker_ace; /**< Ace used as kicker attachment (30) */
    int kicker_king; /**< King used as kicker attachment (15) */
    int kicker_bomb_break; /**< Extra: breaking a four-of-a-kind as a kicker (100) */
    int kicker_triple_break; /**< Extra: breaking a triple when using as kicker (25) */
    int kicker_pair_break; /**< Extra: breaking a pair when using as kicker (10) */

    /* --- Goal 9: Opponent inference --- */
    int bid3_ctrl_reduction; /**< Reduce ctrl-card save penalty when landlord bid 3 (15) */
    int void_threshold; /**< Pass count before inferring a type void (2) */

    /* --- Goal 12: Strategic lead categories --- */
    int landlord_weak_bonus; /**< Bonus for leading a type the landlord is weak in (15) */
    int partner_signal_bonus; /**< Bonus for leading a type the partner signalled (10) */
} BotWeights;

/**
 * @brief Returns the default weights used by bot_bid and bot_play.
 * @return Pointer to the internal default BotWeights.
 */
const BotWeights *bot_default_weights(void);

/**
 * @brief Initialises a BotWeights struct with the default values.
 * @param out Destination to fill; must not be NULL.
 * @details Always call this (or copy from bot_default_weights()) before
 *          using bot_weights_set() to tweak individual fields.  A
 *          zero-initialised BotWeights will cause a divide-by-zero crash
 *          because pos_lead_divisor and pos_resp_divisor would both be 0.
 */
void bot_weights_init(BotWeights *out);

/**
 * @brief Returns the number of integer fields in BotWeights.
 * @return Count of tunable weight entries.
 * @details BotWeights is intentionally all-integer so generic tuning code can
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
 * @return Field value, or 0 if index is invalid.
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
 * @brief Decides a bid value for a bot player.
 * @param g Pointer to the current GameState (must be in PHASE_BIDDING).
 * @param player Index of the player whose bid is being decided.
 * @return Bid value: 0 (pass) or 1–3. Always strictly exceeds the current
 *         highest bid, or 0 if the hand is too weak to outbid.
 * @details Uses eval_hand_score to assess hand quality. Thresholds are
 *          calibrated so that hands with bombs and control cards bid higher.
 */
int bot_bid(const GameState *g, int player);

/**
 * @brief Weight-configurable variant of bot_bid.
 * @param g Pointer to the current GameState.
 * @param player Index of the player whose bid is being decided.
 * @param weights Tuning constants to use, or NULL for defaults.
 * @return Bid value: 0 (pass) or 1–3.
 */
int bot_bid_with_weights(const GameState *g, int player, const BotWeights *weights);

/**
 * @brief Selects a move for a bot player during the playing phase.
 * @param g Pointer to the current GameState (must be in PHASE_PLAYING).
 * @param player Index of the player whose turn it is.
 * @return The chosen Move. Will be MOVE_PASS if the bot elects not to play.
 * @details The bot is aware of:
 *  - **Role**: Landlord plays aggressively; peasants cooperate with their partner.
 *  - **Endgame positioning**: Prefers larger combos when any player has ≤ 5 cards.
 *  - **Combination preservation**: Avoids partially using a bomb rank as a kicker
 *    when safer alternatives exist.
 *  - **Bomb discipline**: Only uses bombs when an opponent is close to winning or
 *    the stakes justify it (high landlord bid + endgame).
 *  - **Peasant cooperation**: Passes when the peasant partner controls the table,
 *    unless the landlord is an immediate threat (≤ 3 cards remaining).
 *  - **Card counting**: Tracks remaining hand sizes for all players (public info).
 *  - **Strength inference**: Infers unplayed control cards (2s, jokers) from the
 *    play history and own hand to adjust aggression.
 *  - **Opponent inference (Goal 9)**: Uses bid level and pass patterns to infer
 *    opponent voids and adjusts control-card spending accordingly.
 *  - **Strategic leads (Goal 12)**: Prefers leading types the landlord is weak in
 *    or types the peasant partner has demonstrated strength in.
 */
Move bot_play(const GameState *g, int player);

/**
 * @brief Weight-configurable variant of bot_play.
 * @param g Pointer to the current GameState.
 * @param player Index of the player whose turn it is.
 * @param weights Tuning constants to use, or NULL for defaults.
 * @return The chosen Move.
 */
Move bot_play_with_weights(const GameState *g, int player, const BotWeights *weights);

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
 * This avoids the self-play degeneracy problem where all three bots use the
 * same weights and the total win count is always == num_games regardless of
 * weight quality.  By fixing the opponent weights, the fitness signal
 * reflects genuine improvement over the baseline.
 *
 * Because the landlord role (1 vs 2) and peasant role (2 vs 1) are
 * fundamentally different objectives, they are reported separately so
 * the caller can blend them as appropriate for their optimiser.
 *
 * @param candidate        Weights being evaluated; NULL uses defaults.
 * @param num_games        Games per series (total games run = 2 * num_games).
 * @param base_seed        PRNG seed for series A; series B uses base_seed + num_games.
 * @param wins_as_landlord Filled with candidate win count from series A (out of num_games).
 * @param wins_as_peasant  Filled with peasant-team win count from series B (out of num_games).
 *                         A peasant team win is any game where the winner is not seat 0.
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

#endif //LIBDOUDIZHU_BOT_H
