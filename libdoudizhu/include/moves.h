/**
 * @file moves.h
 * @brief Move and card logic for libdoudizhu
 * @author Jonathan
 * @date 08-Mar-26
 */

#ifndef LIBDOUDIZHU_MOVES_H
#define LIBDOUDIZHU_MOVES_H

#include <stdint.h> // Required for uint8_t

/** * @defgroup CardReps Card Representations
 * @brief Definitions for card values, ranks, and suits.
 * * A Card is an uint8_t. The deck has 54 cards (52 standard + 2 jokers).
 * For standard cards: value = rank * 4 + suit.
 * @{
 */

/**
 * @brief Extracts the rank (0-14) from a card.
 */
#define CARD_RANK(c) ((c) < 52 ? (c) / 4 : 13 + ((c) - 52))

/** @brief
 * Extracts the suit (0-3) from a card; only meaningful for c < 52.
 */
#define CARD_SUIT(c) ((c) % 4)

#define RANK_3 0 // Lowest standard rank
#define RANK_4 1
#define RANK_5 2
#define RANK_6 3
#define RANK_7 4
#define RANK_8 5
#define RANK_9 6
#define RANK_10 7
#define RANK_J 8
#define RANK_Q 9
#define RANK_K 10
#define RANK_A 11
#define RANK_2 12 // Highest standard rank
#define RANK_SMALL_JOKER 13
#define RANK_BIG_JOKER 14

/**
 * @brief Maximum rank allowed in a straight (2s and jokers are disallowed).
 */
#define RANK_MAX_STRAIGHT RANK_A

/** @brief Type definition for a single card. */
typedef uint8_t Card;
/** @} */

/**
 * @enum MoveType
 * @brief Classification of doudizhu hands.
 */
typedef enum {
    MOVE_PASS = 0, // Player passes (no hand played)
    MOVE_SINGLE, // Single card
    MOVE_PAIR, // Two cards of the same rank
    MOVE_TRIPLE, // Three cards of the same rank
    MOVE_TRIPLE_SINGLE, // Three of a kind + a single kicker
    MOVE_TRIPLE_PAIR, // Three of a kind + a pair of kickers
    MOVE_STRAIGHT, // 5+ consecutive singles
    MOVE_PAIR_STRAIGHT, // 3+ consecutive pairs
    MOVE_TRIPLE_STRAIGHT, // 2+ consecutive triples (plane)
    MOVE_TRIPLE_STRAIGHT_SINGLES, // N consecutive triples + N singles (N >= 2)
    MOVE_TRIPLE_STRAIGHT_PAIRS, // N consecutive triples + N pairs (N >= 2)
    MOVE_FOUR_TWO_SINGLES, // Four of a kind + 2 single kickers
    MOVE_FOUR_TWO_PAIRS, // Four of a kind + 2 pairs
    MOVE_BOMB, // Four cards of the same rank (four of a kind)
    MOVE_ROCKET, // Rocket (big joker + small joker)
    MOVE_INVALID, // Invalid combination of cards
} MoveType;

/**
 * @struct Move
 * @brief Represents a fully classified move played on the table.
 */
#define MOVE_MAX_CARDS 20 // There are at most 20 cards in a hand, so 20 is the max a player can play in one hand.

typedef struct {
    MoveType type; // Category of the move.
    Card cards[MOVE_MAX_CARDS]; // Array of cards in the move.
    int count; // Total number of cards played.
    int rank; // Primary rank for comparison (ex. the rank of the triple in a plane)
    int length; // Length of a chain move (ex. length of straight)
} Move;

/** @brief Size of the rank-count array (0-14). */
#define RANK_COUNT_SIZE 15

/**
 * @brief Populates a rank-count array from a set of cards.
 * @param cards Pointer to the card array.
 * @param n Number of cards.
 * @param cnt Array of size RANK_COUNT_SIZE to be populated.
 */
void movesCountRanks(const Card cards[], int n, int cnt[RANK_COUNT_SIZE]);

/**
 * @brief Classifies an arbitrary set of cards into a Move structure.
 * @param cards Pointer to the cards to classify.
 * @param n Number of cards.
 * @return A Move structure; type is MOVE_INVALID if the combination is illegal.
 */
Move movesClassify(const Card cards[], int n);

/**
 * @brief Classifies cards based on a pre-computed rank-count array.
 * @param cnt The rank-count array.
 * @param total Total number of cards represented in the count array.
 * @return A Move structure.
 */
Move movesClassifyCounts(const int cnt[RANK_COUNT_SIZE], int total);

/**
 * @brief Determines if a new play beats the previous play on the table.
 * @param play The move being attempted.
 * @param prev The current highest move on the table.
 * @return 1 if `play` legally beats `prev`, 0 otherwise.
 */
int movesBeats(const Move* play, const Move* prev);

/**
 * @brief Generates all legal moves from a hand that can beat the current table move.
 * @param hand Pointer to the player's current cards.
 * @param hand_size Number of cards in hand.
 * @param prev The current move to beat (use MOVE_PASS for an empty table).
 * @param out Pointer to an array of Move structures to store results.
 * @param max_out Maximum number of moves to write to the `out` array.
 * @return The total number of legal moves found.
 */
int movesGenerate(const Card hand[], int hand_size, const Move* prev, Move out[], int max_out);

/**
 * @brief Returns a human-readable string for a MoveType.
 * @param type The MoveType enum value.
 * @return A constant string describing the move type.
 */
const char* movesTypeName(MoveType type);

/**
 * @brief Sorts the cards within a Move structure by rank in ascending order.
 * @param m Pointer to the Move to be sorted.
 */
void movesSort(Move* m);

#endif // LIBDOUDIZHU_MOVES_H
