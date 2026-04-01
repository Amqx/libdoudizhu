/**
 * @file bot.h
 * @brief Rule-based bot player for libdoudizhu
 * @author Peng Yang Deng (), Emma Le ()
 * @date 08-Mar-26
 */

#ifndef LIBDOUDIZHU_BOT_H
#define LIBDOUDIZHU_BOT_H

#include "game.h"

/**
 * @struct BotWeights
 * @brief All tunable scoring constants for the bot
 */
typedef struct {
    // Leading weights
    int clear_per_card; // Base clearing bonus for getting rid of a card
    int endgame_clear; // Extra bonus per card in the endgame
    int chain_length; // Bonus per chain length in straights and airplanes
    int feeder_penalty; // Per-card penalty for leading big combos
    int pos_lead_divisor; // Divide post-move position score for leads

    // Response weights
    int pos_resp_divisor; // Divide post-move position score for responses
    int danger_per_card; // Per-card bonus when an enemy is near winning
    int gatekeeper_bonus; // Gatekeeper blocking the landlord's lead
    int ctrl_2_penalty; // Penalty for using a 2 as a response card
    int ctrl_sj_penalty; // Penalty for using the small joker as a response
    int ctrl_bj_penalty; // Penalty for using the big joker as a response
    int must_stop_multiplier; // Score multiplier for evalMoveCost in must-stop

    // Combo integrity
    int break_combo_penalty; // Penalty for breaking a bomb or rocket
    int kicker_joker; // Joker used as a kicker attachment
    int kicker_2; // 2 used as a kicker attachment
    int kicker_ace; // Ace used as a kicker attachment
    int kicker_king; // King used as a kicker attachment
    int kicker_bomb_break; // Breaking a four of a kind as a kicker
    int kicker_triple_break; // Breaking a triple when using as a kicker
    int kicker_pair_break; // Breaking a pair when using as a kicker

    // Opponent inference
    int bid3_ctrl_reduction; // Reduce control cards saving penalty when the landlord bids 3 (indicates stronger hand)
    int void_threshold;
    // Pass count before inferring a type void (Number of moves a player must skip before the bot can infer that player
    // is unable to play that type)

    // Strategic leading bonuses
    int landlord_weak_bonus; // Bonus for leading a type the landlord is weak in
    int partner_signal_bonus; // Bonus for leading a type the partner signalled
} BotWeights;

/**
 * @brief Returns the default weights used by botBid and botPlay.
 * @return Pointer to the internal default BotWeights.
 */
const BotWeights* botDefaultWeights(void);

/**
 * @brief Decides a bid value for a bot player.
 * @param g Pointer to the current GameState (must be in PHASE_BIDDING).
 * @param player Index of the player whose bid is being decided.
 * @return Bid value: 0 (pass) or 1–3. Always strictly exceeds the current
 *         highest bid, or 0 if the hand is too weak to outbid.
 */
int botBid(const GameState* g, int player);

/**
 * @brief Weight-configurable variant of botBid.
 * @param g Pointer to the current GameState.
 * @param player Index of the player whose bid is being decided.
 * @param weights Tuning constants to use, or NULL for defaults.
 * @return Bid value: 0 (pass) or 1–3.
 */
int botBidWithWeights(const GameState* g, int player, const BotWeights* weights);

/**
 * @brief Selects a move for a bot player during the playing phase.
 * @param g Pointer to the current GameState (must be in PHASE_PLAYING).
 * @param player Index of the player whose turn it is.
 * @return The chosen Move. Will be MOVE_PASS if the bot elects not to play.
 */
Move botPlay(const GameState* g, int player);

/**
 * @brief Weight-configurable variant of botPlay.
 * @param g Pointer to the current GameState.
 * @param player Index of the player whose turn it is.
 * @param weights Tuning constants to use, or NULL for defaults.
 * @return The chosen Move.
 */
Move botPlayWithWeights(const GameState* g, int player, const BotWeights* weights);

#endif // LIBDOUDIZHU_BOT_H
