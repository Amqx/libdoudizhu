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
 */
Move bot_play(const GameState *g, int player);

#endif //LIBDOUDIZHU_BOT_H
