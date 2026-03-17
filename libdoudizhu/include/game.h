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
#define GAME_NUM_PLAYERS 3 // Standard games have 3 players
#define GAME_DECK_SIZE 54 // 52 cards + 2 jokers
#define GAME_KITTY_SIZE 3 // Kitty reserves 3 cards for the landlord
#define GAME_HAND_SIZE 17 // (54 - 3)/3 = 17 cards for all players initially
#define GAME_MAX_HAND_SIZE 20 // Landlord's final hand size

#define PLAYER_0 0
#define PLAYER_1 1
#define PLAYER_2 2
#define PLAYER_NONE (-1)
/** @} */

/**
 * @enum GamePhase
 * @brief The different stages of a single round.
 */
typedef enum {
    PHASE_BIDDING, // Players are bidding to become the landlord
    PHASE_PLAYING, // Active card playing phase
    PHASE_OVER, // Round is finished and the winner has been determined
} GamePhase;

/**
 * @struct BidState
 * @brief Tracks the bidding phase to determine the landlord.
 */
typedef struct {
    int scores[GAME_NUM_PLAYERS]; // Bids from players (0 = pass, 1-3 = multipliers)
    int current_bidder; // Currently bidding player
    int highest_bidder; // Currently leading bidder
    int highest_score; // Currently leading bid
    int num_passed; // Number of consecutive passes
} BidState;

/**
 * @struct Hand
 * @brief Represents a player's cards.
 */
typedef struct {
    Card cards[GAME_MAX_HAND_SIZE]; // Array of card IDs
    int count; // Number of cards in hand
} Hand;

/**
 * @struct PlayRecord
 * @brief An entry in the game's move history log.
 */
typedef struct {
    int player; // Player who made the move
    Move move; // Move data (type, rank, cards)
} PlayRecord;

#define GAME_MAX_PLAYS 60 // Upper bound for number of plays in a game.

/**
 * @struct GameState
 * @brief The state for an entire match.
 */
typedef struct {
    // Deck and dealing
    Card deck[GAME_DECK_SIZE]; // The full deck
    Card kitty[GAME_KITTY_SIZE]; // The three kitty cards

    // Hands
    Hand hands[GAME_NUM_PLAYERS]; // Each player's hand

    // Bidding
    BidState bid; // Current bidding state
    int landlord; // Landlord player (PLAYER_NONE until bidding finishes)
    int base_score; // Score multiplier from bidding

    // Playing
    GamePhase phase; // Current game state
    int current_player; // Current player
    int last_player; // Last player who played a non-pass move
    Move last_move; // Current move to beat
    int passes_in_a_row; // Number of consecutive passes

    // Playing history
    PlayRecord history[GAME_MAX_PLAYS]; // Log of all moves played thus far
    int history_count; // Total count of records in the log

    // Results
    int winner; // Winning player index (only valid once phase == PHASE_OVER)
    int score; // Final calculated score including bomb multipliers
    int bomb_count; // Bomb and rockets counter
} GameState;

/* --- Setup functions --- */
/**
 * @brief Initializes a fresh GameState. All memory is zeroed out.
 * @param g Pointer to the GameState.
 */
void gameInit(GameState* g);

/**
 * @brief Resets the deck to a standard-ordered state (0-53).
 * @param g Pointer to the GameState.
 */
void gameResetDeck(GameState* g);

/**
 * @brief Shuffles the deck using a seed.
 * @param g Pointer to the GameState.
 * @param seed Seed for the shuffle.
 */
void gameShuffle(GameState* g, unsigned int seed);

/**
 * @brief Deals 17 cards to each player and 3 to the kitty.
 * @param g Pointer to the GameState.
 * @note To get randomized playing hands, use gameShuffle() first.
 */
void gameDeal(GameState* g);

/* --- Bidding --- */
/**
 * @brief Switches the game phase to PHASE_BIDDING.
 * @param g Pointer to the GameState.
 * @param first_bidder Index of the player who starts the bid.
 */
void gameStartBidding(GameState* g, int first_bidder);

/**
 * @brief Processes a bid from a player.
 * @param g Pointer to the GameState.
 * @param player Index of the player bidding.
 * @param value Bid value (0 for pass, 1-3 for multiplier).
 * @return 1 on success, 0 if the move is illegal (wrong turn or value too low).
 * @note Transitions to PHASE_PLAYING and awards kitty if bidding concludes.
 */
int gameBid(GameState* g, int player, int value);

/* --- Playing --- */
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
int gamePlay(GameState* g, int player, const Move* move);

/* --- Querying and Utilities --- */
/**
 * @brief Checks if a player holds the cards required for a move.
 * @param g Pointer to the GameState.
 * @param player Index of the player.
 * @param move Pointer to the move.
 * @return 1 if cards are present, 0 otherwise.
 */
int gamePlayerHasCards(const GameState* g, int player, const Move* move);

/**
 * @brief Generates all legal moves for a player based on the current table state.
 * @param g Pointer to the GameState.
 * @param player Index of the player.
 * @param out Buffer to store generated moves.
 * @param max_out Size of the out buffer.
 * @return Total number of legal moves found.
 */
int gameLegalMoves(const GameState* g, int player, Move out[], int max_out);

/**
 * @brief Checks if the player is a peasant.
 * @param g Pointer to the GameState.
 * @param player Index of the player.
 * @return 1 if peasant, 0 if landlord.
 */
int gameIsPeasant(const GameState* g, int player);

/**
 * @brief Calculates the index of the next player in clockwise order.
 * @param g Pointer to the GameState.
 * @param player Current player index.
 * @return Next player index (0, 1, or 2).
 */
int gameNextPlayer(const GameState* g, int player);

#endif // LIBDOUDIZHU_GAME_H
