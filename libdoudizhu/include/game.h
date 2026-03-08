/**
 * @file game.h
 * @brief Core game engine logic for doudizhu
 * @author Jonathan
 * @date 08-Mar-26
 */

#ifndef LIBDOUDIZHU_GAME_H
#define LIBDOUDIZHU_GAME_H

#include "moves.h"

/**
 * @defgroup GameConstants Game Constants
 * @{
 */
#define GAME_NUM_PLAYERS    3   /**< Standard doudizhu is played by 3 players */
#define GAME_DECK_SIZE      54  /**< 52 cards + 2 jokers */
#define GAME_KITTY_SIZE     3   /**< Cards reserved for the landlord */
#define GAME_HAND_SIZE      17  /**< Initial hand size for peasants (54-3)/3 */
#define GAME_MAX_HAND_SIZE  20  /**< Landlord's final hand size (17+3) */

#define PLAYER_0            0
#define PLAYER_1            1
#define PLAYER_2            2
#define PLAYER_NONE        (-1)
/** @} */

/**
 * @enum GamePhase
 * @brief The different stages of a single round.
 */
typedef enum {
    PHASE_BIDDING, /**< Players are bidding to become landlord */
    PHASE_PLAYING, /**< Landlord vs peasants; active card play */
    PHASE_OVER, /**< Round is finished; winner determined */
} GamePhase;

/**
 * @struct BidState
 * @brief tracks the "Call Score" phase to determine the landlord.
 */
typedef struct {
    int scores[GAME_NUM_PLAYERS]; /**< Bid value per player (0=pass, 1-3=multiplier) */
    int current_bidder; /**< Index of the player whose turn it is to bid */
    int highest_bidder; /**< Player index of the current leader (-1 if none) */
    int highest_score; /**< Current highest bid value (0-3) */
    int num_passed; /**< Counter for consecutive passes */
} BidState;

/**
 * @struct Hand
 * @brief Represents a player's private collection of cards.
 */
typedef struct {
    Card cards[GAME_MAX_HAND_SIZE]; /**< Array of card IDs */
    int count; /**< Number of cards currently held */
} Hand;

/**
 * @struct PlayRecord
 * @brief An entry in the game's move history log.
 */
typedef struct {
    int player; /**< Player index who made the move */
    Move move; /**< The move data (type, cards, rank) */
} PlayRecord;

#define GAME_MAX_PLAYS 60 /**< Safety upper bound for plays in a round */

/**
 * @struct GameState
 * @brief The state for an entire doudizhu match.
 */
typedef struct {
    /* --- Deck & Deal --- */
    Card deck[GAME_DECK_SIZE]; /**< The full deck of 54 cards */
    Card kitty[GAME_KITTY_SIZE]; /**< The 3 hidden cards for the landlord */

    /* --- Hands --- */
    Hand hands[GAME_NUM_PLAYERS]; /**< Private hands for each player */

    /* --- Bidding --- */
    BidState bid; /**< Current state of the bidding process */
    int landlord; /**< Index of the landlord (-1 until bidding ends) */
    int base_score; /**< The score multiplier (1, 2, or 3) from bidding */

    /* --- Playing --- */
    GamePhase phase; /**< Current phase (Bidding, Playing, Over) */
    int current_player; /**< Index of player whose turn it is */
    int last_player; /**< Player who last played a non-pass move */
    Move last_move; /**< The current move to beat on the table */
    int passes_in_a_row; /**< Number of consecutive MOVE_PASS plays */

    /* --- History --- */
    PlayRecord history[GAME_MAX_PLAYS]; /**< Log of all moves played */
    int history_count; /**< Total count of records in history */

    /* --- Result --- */
    int winner; /**< Winning player index (valid when phase == PHASE_OVER) */
    int score; /**< Final calculated score including bomb multipliers */
    int bomb_count; /**< Counter for bombs/rockets to double the score */
} GameState;

/* ---------------------------------------------------------------------------
 * Setup Functions
 * --------------------------------------------------------------------------- */

/**
 * @brief Initializes a fresh GameState, zeroing out all memory.
 * @param g Pointer to the GameState.
 */
void game_init(GameState *g);

/**
 * @brief Resets the deck to a standard-ordered state (0-53).
 * @param g Pointer to the GameState.
 */
void game_reset_deck(GameState *g);

/**
 * @brief Shuffles the deck using a seeded PRNG.
 * @param g Pointer to the GameState.
 * @param seed Seed for the shuffle; use 0 to always get the same seed.
 */
void game_shuffle(GameState *g, unsigned int seed);

/**
 * @brief Deals 17 cards to each player and 3 to the kitty.
 * @note Must be called after game_shuffle().
 * @param g Pointer to the GameState.
 */
void game_deal(GameState *g);

/* ---------------------------------------------------------------------------
 * Bidding Phase
 * --------------------------------------------------------------------------- */

/**
 * @brief Switches the game phase to PHASE_BIDDING.
 * @param g Pointer to the GameState.
 * @param first_bidder Index of the player who starts the bid.
 */
void game_start_bidding(GameState *g, int first_bidder);

/**
 * @brief Processes a bid from a player.
 * @param g Pointer to the GameState.
 * @param player Index of the player bidding.
 * @param value Bid value (0 for pass, 1-3 for multiplier).
 * @return 1 on success, 0 if the move is illegal (wrong turn or value too low).
 * @note Transitions to PHASE_PLAYING and awards kitty if bidding concludes.
 */
int game_bid(GameState *g, int player, int value);

/* ---------------------------------------------------------------------------
 * Playing Phase
 * --------------------------------------------------------------------------- */

/**
 * @brief Executes a move for a player.
 * @param g Pointer to the GameState.
 * @param player Index of the player playing.
 * @param move Pointer to the Move structure to be played.
 * @return 1 if the move was legal and applied, 0 otherwise.
 * @details Legality checks include:
 * - Current turn and phase.
 * - Move validity and rank comparison.
 * - Card ownership.
 */
int game_play(GameState *g, int player, const Move *move);

/* ---------------------------------------------------------------------------
 * Queries & Utilities
 * --------------------------------------------------------------------------- */

/**
 * @brief Checks if a player holds the cards required for a move.
 * @param g Pointer to the GameState.
 * @param player Index of the player.
 * @param move Pointer to the move.
 * @return 1 if cards are present, 0 otherwise.
 */
int game_player_has_cards(const GameState *g, int player, const Move *move);

/**
 * @brief Generates all legal moves for a player based on the current table state.
 * @param g Pointer to the GameState.
 * @param player Index of the player.
 * @param out Buffer to store generated moves.
 * @param max_out Size of the out buffer.
 * @return Total number of legal moves found.
 */
int game_legal_moves(const GameState *g, int player, Move *out, int max_out);

/**
 * @brief Checks if the player is a peasant.
 * @param g Pointer to the GameState.
 * @param player Index of the player.
 * @return 1 if peasant, 0 if landlord.
 */
int game_is_peasant(const GameState *g, int player);

/**
 * @brief Calculates the index of the next player in clockwise order.
 * @param g Pointer to the GameState.
 * @param player Current player index.
 * @return Next player index (0, 1, or 2).
 */
int game_next_player(const GameState *g, int player);

#endif // LIBDOUDIZHU_GAME_H
