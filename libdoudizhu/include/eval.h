/**
 * @file eval.h
 * @brief Hand evaluation utilities for libdoudizhu
 * @author Peng Yang Deng, Emma Le
 * @date 08-Mar-26
 */

#ifndef LIBDOUDIZHU_EVAL_H
#define LIBDOUDIZHU_EVAL_H

#include "moves.h"

/**
 * @brief Computes a numeric strength score for a hand.
 * @param cards Pointer to the card array.
 * @param n Number of cards.
 * @return An integer score; higher means stronger. Roughly in the 0–100 range.
 * @details Accounts for bombs, rockets, high singles (2s, jokers, aces),
 *          pairs, triples, and sequential potential.
 */
int evalHandScore(const Card cards[], int n);

/**
 * @brief Counts the number of bombs (four of a kind) and rockets in a hand.
 * @param cards Pointer to the card array.
 * @param n Number of cards.
 * @return Total number of bomb/rocket combinations.
 */
int evalCountBombs(const Card cards[], int n);

/**
 * @brief Returns a cost score for a move; lower means weaker/cheaper to play.
 * @param m Pointer to the Move.
 * @return Integer cost. Used by the bot to prefer playing weaker moves first.
 * @details The scale is roughly the following: singles (0–14) < pairs (15–29) < triples (30–44)
 *          < chain moves (45–79) < bombs (80–94) < rocket (100).
 */
int evalMoveCost(const Move* m);

/**
 * @brief Estimates the minimum number of plays to empty a hand, assuming the player leads every turn.
 * @param cards Pointer to the cards in a player's hand.
 * @param n Number of cards.
 * @return Estimated minimum play count. Lower = stronger position.
 * @details Greedily extracts rockets, bombs, airplanes (with kickers),
 *          pair straights, single straights, triples, pairs, then singles.
 */
int evalMinPlays(const Card cards[], int n);

/**
 * @brief Rank-count-array variant of evalMinPlays.
 * @param cnt Pre-computed rank-count array.
 * @return Estimated minimum play count.
 */
int evalMinPlaysCounts(const int cnt[RANK_COUNT_SIZE]);

/**
 * @brief Rank-count-array variant of evalPlayPosition.
 * @param cnt Pre-computed rank-count array.
 * @param n   Total number of cards.
 * @return Play-phase position score (higher = better).
 */
int evalPlayPositionCounts(const int cnt[RANK_COUNT_SIZE], int n);

/**
 * @brief Computes a bidding strength score on raw power.
 * @param cards Pointer to the card array.
 * @param n Number of cards.
 * @return Integer score; higher means a stronger bidding hand.
 * @details Prioritises bombs, rockets, 2s, and jokers. Used for bid decisions.
 */
int evalBidStrength(const Card cards[], int n);

/**
 * @brief Computes a play-phase position score on control and tempo.
 * @param cards Pointer to the card array.
 * @param n Number of cards.
 * @return Integer score; higher means better in-play position.
 * @details Combines hand clearing ability (min plays) with control card count.
 */
int evalPlayPosition(const Card cards[], int n);

#endif // LIBDOUDIZHU_EVAL_H
