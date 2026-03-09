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
 * @brief Runs a series of complete bot-vs-bot games and tallies winner counts.
 * @param num_games Number of games to simulate.
 * @param base_seed Starting PRNG seed; game i uses seed (base_seed + i).
 * @param wins_out Array of size GAME_NUM_PLAYERS filled with per-player win counts.
 * @details Games where every player passes during bidding (no landlord) are
 *          skipped and do not count toward num_games.
 */
void bot_simulate(int num_games, unsigned int base_seed, int wins_out[GAME_NUM_PLAYERS]);

#endif //LIBDOUDIZHU_BOT_H
