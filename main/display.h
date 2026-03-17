/**
 * @file display.h
 * @brief Display utilities for the Doudizhu game.
 *
 * Provides functions for rendering cards, hands, moves, and game status
 * to the terminal with support for Unicode suits and structured move display.
 */

#ifndef LIBDOUDIZHU_DISPLAY_H
#define LIBDOUDIZHU_DISPLAY_H

#include "game.h"
#include "moves.h"

/**
 * @brief Prints a single card's rank and suit to stdout.
 * @param c The card to print.
 */
void printCard(Card c);

/**
 * @brief Sorts an array of cards by rank into a destination buffer.
 * @param dst Destination buffer (must be at least size n).
 * @param src Source card array.
 * @param n Number of cards to sort.
 */
void sortCardsInto(Card dst[], const Card src[], int n);

/**
 * @brief Prints a Hand's cards to stdout, sorted by rank.
 * @param h Pointer to the hand to print.
 */
void printHandSorted(const Hand* h);

/**
 * @brief Helper to print a card with an optional preceding space.
 * @param c Card to print.
 * @param need_gap Pointer to a flag; if 1, a space is printed before the card.
 *                 Set to 1 after printing.
 */
void printCardWithGap(Card c, int* need_gap);

/**
 * @brief Prints a specific number of cards of a certain rank from a sorted list.
 * @details Used to group cards by their role in a move (e.g., printing the triplet before the kicker).
 * @param sorted Pointer to a sorted array of cards.
 * @param n Total number of cards in the array.
 * @param rank The rank to filter for.
 * @param copies Number of cards of this rank to print.
 * @param used Array tracking how many cards of each rank have already been printed.
 * @param need_gap Pointer to the gap flag for spacing.
 */
void printRankGroup(const Card sorted[], int n, int rank, int copies, int used[RANK_COUNT_SIZE], int* need_gap);


/**
 * @brief Prints the cards in a move, structured by the move type's logic.
 * @details For example, in a Triple-Single, the triple is printed first regardless of rank order.
 * @param m Pointer to the move.
 */
void printMoveCardsByStructure(const Move* m);

/**
 * @brief Prints a move's type and its cards on a single line.
 * @param m Pointer to the move.
 */
void printMoveInline(const Move* m);

/**
 * @brief Prints a horizontal rule to stdout.
 */
void hr();

/**
 * @brief Prints a decorated banner with the provided title.
 * @param s The title string.
 */
void banner(const char* s);

/**
 * @brief Returns a display name for a player index.
 * @param p Player index (0, 1, or 2).
 * @return String representation ("You", "Bot 1", or "Bot 2").
 */
const char* pname(int p);

/**
 * @brief Prints the current game status, including player card counts and the last move on the table.
 * @param g Pointer to the current game state.
 */
void printStatus(const GameState* g);

/**
 * Prints all legal moves for a player.
 * @param moves An array containing all possible moves for a player.
 * @param n Number of moves in the array.
 * @param is_leading Whether the player is following or leading the hand.
 */
void printLegalMoves(const Move moves[], int n, int is_leading);

/**
 * Prints the scoreboard for all players.
 * @param scores List of scores.
 */
void printScoreboard(const int scores[GAME_NUM_PLAYERS]);

#endif // LIBDOUDIZHU_DISPLAY_H
