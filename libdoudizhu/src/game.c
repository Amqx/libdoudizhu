/**
 * @file game.c
 * @brief Implementation of doudizhu game engine logic.
 * @author Peng Yang Deng (), Emma Le ()
 * @date 08-Mar-26
 */

#include "game.h"
#include <stdlib.h>
#include "utils.h"

//Game setup - reset everything for setup
void gameInit(GameState *g) {
    lddzMemset(g, 0, sizeof(GameState));
    g->landlord = PLAYER_NONE;
    g->bid.current_bidder = PLAYER_NONE;
    g->bid.highest_bidder = PLAYER_NONE;
    g->phase = PHASE_BIDDING;
    g->current_player = PLAYER_NONE;
    g->last_player = PLAYER_NONE;
    g->last_move.type = MOVE_PASS;
    g->winner = PLAYER_NONE;
}

void gameResetDeck(GameState *g) {
    for (int i = 0; i < GAME_DECK_SIZE; i++)
        g->deck[i] = (Card) i;
}

void gameShuffle(GameState *g, unsigned int seed) {
    srand(seed);
    for (int i = GAME_DECK_SIZE - 1; i > 0; i--) {
        const int j = rand() % (i + 1);
        const Card tmp = g->deck[i];
        g->deck[i] = g->deck[j];
        g->deck[j] = tmp;
    }
}

void gameDeal(GameState *g) {
    // Deal 17 cards to each player sequentially from the shuffled deck
    for (int p = 0; p < GAME_NUM_PLAYERS; p++) {
        for (int i = 0; i < GAME_HAND_SIZE; i++)
            g->hands[p].cards[i] = g->deck[p * GAME_HAND_SIZE + i];
        g->hands[p].count = GAME_HAND_SIZE;
    }
    // The remaining 3 cards from the kitty
    for (int i = 0; i < GAME_KITTY_SIZE; i++)
        g->kitty[i] = g->deck[GAME_NUM_PLAYERS * GAME_HAND_SIZE + i];
}

void gameStartBidding(GameState *g, const int first_bidder) {
    g->phase = PHASE_BIDDING;
    g->bid.current_bidder = first_bidder;
    g->bid.highest_bidder = PLAYER_NONE;
    g->bid.highest_score = 0;
    g->bid.num_passed = 0;
    for (int i = 0; i < GAME_NUM_PLAYERS; i++)
        g->bid.scores[i] = 0;
    g->landlord = PLAYER_NONE;
    g->base_score = 0;
}

/**
 * Internal helper to award kitty to the landlord and sort their hand.
 * @param g Currently playing game
 */
static void assignKitty(GameState *g) {
    Hand *h = &g->hands[g->landlord];
    for (int i = 0; i < GAME_KITTY_SIZE; i++)
        h->cards[h->count++] = g->kitty[i];

    // Perform a simple insertion sort to keep the hand readable.
    for (int i = 1; i < h->count; i++) {
        const Card key = h->cards[i];
        int j = i - 1;
        while (j >= 0 && CARD_RANK(h->cards[j]) > CARD_RANK(key)) {
            h->cards[j + 1] = h->cards[j];
            j--;
        }
        h->cards[j + 1] = key;
    }
}

/**
 * Transitions the game from bidding to the active playing phase.
 * @param g Currently playing game
 */
static void startPlaying(GameState *g) {
    assignKitty(g);
    g->base_score = g->bid.highest_score;
    g->phase = PHASE_PLAYING;
    g->current_player = g->landlord; // Landlord always leads the first round.
    g->last_player = PLAYER_NONE;
    g->last_move.type = MOVE_PASS;
    g->passes_in_a_row = 0;
    g->bomb_count = 0;
    g->history_count = 0;
}

int gameBid(GameState *g, const int player, const int value) {
    if (g->phase != PHASE_BIDDING || player != g->bid.current_bidder)
        return 0;
    if (value < 0 || value > 3)
        return 0;
    if (value > 0 && value <= g->bid.highest_score)
        return 0;

    g->bid.scores[player] = value;

    if (value > 0) {
        g->bid.highest_bidder = player;
        g->bid.highest_score = value;
    } else {
        g->bid.num_passed++;
    }

    int total_bids = 0;
    for (int i = 0; i < GAME_NUM_PLAYERS; i++)
        if (g->bid.scores[i] > 0)
            total_bids++;

    const int bidding_done = (value == 3) || (g->bid.num_passed == GAME_NUM_PLAYERS) ||
                             (total_bids + g->bid.num_passed == GAME_NUM_PLAYERS);

    if (bidding_done) {
        if (g->bid.highest_bidder != PLAYER_NONE) {
            g->landlord = g->bid.highest_bidder;
            startPlaying(g);
        }
    } else {
        g->bid.current_bidder = (player + 1) % GAME_NUM_PLAYERS;
    }

    return 1;
}

/* --- Internal Helpers --- */
/**
 * Removes played cards from a player's hand via shifting.
 * @param g Currently playing game
 * @param player Player index for the move
 * @param move Move played by player
 */
static void removeCards(GameState *g, const int player, const Move *move) {
    Hand *h = &g->hands[player];
    for (int i = 0; i < move->count; i++) {
        const int target_rank = CARD_RANK(move->cards[i]);
        for (int j = 0; j < h->count; j++) {
            if (CARD_RANK(h->cards[j]) == target_rank) {
                h->cards[j] = h->cards[--h->count];
                break;
            }
        }
    }
}

/**
 * Adds a move to the game's history log.
 * @param g Currently playing game
 * @param player Player index for the move
 * @param move Move played by player
 */
static void recordPlay(GameState *g, const int player, const Move *move) {
    if (g->history_count < GAME_MAX_PLAYS) {
        g->history[g->history_count].player = player;
        g->history[g->history_count].move = *move;
        g->history_count++;
    }
}

/**
 * Checks for victory conditions and calculates the final score.
 * @param g Currently playing game
 * @param player Last player to move
 */
static void checkGameOver(GameState *g, const int player) {
    if (g->hands[player].count == 0) {
        g->phase = PHASE_OVER;
        g->winner = player;

        // Calculate score: multiplier = base_score * 2^(number of bombs)
        g->score = g->base_score;
        for (int i = 0; i < g->bomb_count; i++)
            g->score *= 2;
    }
}

int gamePlayerHasCards(const GameState *g, const int player, const Move *move) {
    int have[RANK_COUNT_SIZE], need[RANK_COUNT_SIZE];
    movesCountRanks(g->hands[player].cards, g->hands[player].count, have);
    movesCountRanks(move->cards, move->count, need);

    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        if (have[r] < need[r])
            return 0;
    }
    return 1;
}

int gameLegalMoves(const GameState *g, const int player, Move out[], const int max_out) {
    const Move *prev = (g->last_player == PLAYER_NONE || g->last_player == player)
                           ? &(const Move){.type = MOVE_PASS}
                           : &g->last_move;
    return movesGenerate(g->hands[player].cards, g->hands[player].count, prev, out, max_out);
}

int gameIsPeasant(const GameState *g, const int player) { return player != g->landlord; }

int gameNextPlayer(const GameState *g, const int player) {
    (void) g;
    return (player + 1) % GAME_NUM_PLAYERS;
}

int gamePlay(GameState *g, const int player, const Move *move) {
    if (g->phase != PHASE_PLAYING || player != g->current_player || !move)
        return 0;

    if (move->type == MOVE_PASS) {
        // Cannot pass if the player is leading the turn.
        if (g->last_player == PLAYER_NONE || g->last_player == player)
            return 0;

        recordPlay(g, player, move);
        g->passes_in_a_row++;
        g->current_player = gameNextPlayer(g, player);

        // Everyone else passed; the table is cleared.
        if (g->passes_in_a_row >= GAME_NUM_PLAYERS - 1) {
            g->last_player = PLAYER_NONE;
            g->last_move.type = MOVE_PASS;
            g->passes_in_a_row = 0;
        }
        return 1;
    }

    if (move->type == MOVE_INVALID || !gamePlayerHasCards(g, player, move))
        return 0;

    // Validate if the move beats the current move on the table.
    const int leads_turn = (g->last_player == PLAYER_NONE || g->last_player == player);
    if (!leads_turn && !movesBeats(move, &g->last_move))
        return 0;

    if (move->type == MOVE_BOMB || move->type == MOVE_ROCKET)
        g->bomb_count++;

    removeCards(g, player, move);
    recordPlay(g, player, move);

    g->last_player = player;
    g->last_move = *move;
    g->passes_in_a_row = 0;
    g->current_player = gameNextPlayer(g, player);

    checkGameOver(g, player);
    return 1;
}
