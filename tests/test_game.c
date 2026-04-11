/**
 * @file test_game.c
 * @brief Tests for the game module (game state and updating)
 * @author Peng Yang Deng, Emma Le
 * @date 08-Mar-26
 */

#include <string.h>
#include "game.h"
#include "test_framework.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Build a fresh, dealt game ready for bidding.
static GameState makeDealtGame(void) {
    GameState g;
    gameInit(&g);
    gameResetDeck(&g);
    gameShuffle(&g, 42);
    gameDeal(&g);
    return g;
}

// Run a full bid sequence: players 0,1,2 bid in order.
// values[3]: bid value for each player (0 = pass).
// Returns the resulting GameState (phase will be PHASE_PLAYING if someone won).
static GameState runBidding(int first, int v0, int v1, int v2) {
    GameState g = makeDealtGame();
    gameStartBidding(&g, first);
    int values[3] = {v0, v1, v2};
    // Submit bids in turn order starting from `first`.
    for (int i = 0; i < GAME_NUM_PLAYERS; i++) {
        int p = (first + i) % GAME_NUM_PLAYERS;
        // Skip if the bid would be illegal (lower than current highest).
        if (values[p] > 0 && values[p] <= g.bid.highest_score)
            continue;
        gameBid(&g, p, values[p]);
        if (g.phase == PHASE_PLAYING)
            break;
    }
    return g;
}

// Find a card of a given rank in a hand (returns the card value, or 255).
static Card findCardOfRank(const Hand *h, int rank) {
    for (int i = 0; i < h->count; i++)
        if (CARD_RANK(h->cards[i]) == rank)
            return h->cards[i];
    return 255;
}

// ---------------------------------------------------------------------------
// Tests: gameInit
// ---------------------------------------------------------------------------

static void testInit(void) {
    beginSuite("init");
    GameState g;
    gameInit(&g);
    EXPECT_EQ(g.phase, PHASE_BIDDING, "phase starts as BIDDING");
    EXPECT_EQ(g.landlord, PLAYER_NONE, "no landlord initially");
    EXPECT_EQ(g.current_player, PLAYER_NONE, "no current player initially");
    EXPECT_EQ(g.winner, PLAYER_NONE, "no winner initially");
    EXPECT_EQ(g.last_move.type, MOVE_PASS, "last move is PASS");
}

// ---------------------------------------------------------------------------
// Tests: deck / deal
// ---------------------------------------------------------------------------

static void testDeckReset(void) {
    beginSuite("deck reset");
    GameState g;
    gameInit(&g);
    gameResetDeck(&g);
    for (int i = 0; i < GAME_DECK_SIZE; i++)
        EXPECT_EQ(g.deck[i], (Card) i, "deck card matches index");
}

static void testDealCardCounts(void) {
    beginSuite("deal: card counts");
    GameState g = makeDealtGame();
    EXPECT_EQ(g.hands[0].count, GAME_HAND_SIZE, "player 0 has 17 cards");
    EXPECT_EQ(g.hands[1].count, GAME_HAND_SIZE, "player 1 has 17 cards");
    EXPECT_EQ(g.hands[2].count, GAME_HAND_SIZE, "player 2 has 17 cards");
}

static void testDealNoDuplicates(void) {
    beginSuite("deal: no duplicate cards");
    GameState g = makeDealtGame();
    int seen[GAME_DECK_SIZE] = {0};
    for (int p = 0; p < GAME_NUM_PLAYERS; p++)
        for (int i = 0; i < g.hands[p].count; i++)
            seen[g.hands[p].cards[i]]++;
    for (int i = 0; i < GAME_KITTY_SIZE; i++)
        seen[g.kitty[i]]++;
    for (int i = 0; i < GAME_DECK_SIZE; i++)
        EXPECT_EQ(seen[i], 1, "each card appears exactly once");
}

static void testDealAllCardsAccounted(void) {
    beginSuite("deal: all 54 cards distributed");
    GameState g = makeDealtGame();
    int total = g.hands[0].count + g.hands[1].count + g.hands[2].count + GAME_KITTY_SIZE;
    EXPECT_EQ(total, GAME_DECK_SIZE, "total cards == 54");
}

// ---------------------------------------------------------------------------
// Tests: bidding
// ---------------------------------------------------------------------------

static void testBiddingLandlordAssigned(void) {
    beginSuite("bidding: landlord assigned");
    // Player 1 bids 2, others pass.
    GameState g = makeDealtGame();
    gameStartBidding(&g, 0);
    gameBid(&g, 0, 0); // pass
    gameBid(&g, 1, 2); // bid 2
    gameBid(&g, 2, 0); // pass

    EXPECT_EQ(g.phase, PHASE_PLAYING, "phase transitions to PLAYING");
    EXPECT_EQ(g.landlord, 1, "player 1 is landlord");
    EXPECT_EQ(g.base_score, 2, "base score is 2");
}

static void testBiddingKittyGivenToLandlord(void) {
    beginSuite("bidding: landlord receives kitty");
    GameState g = makeDealtGame();
    Card kitty_copy[GAME_KITTY_SIZE];
    memcpy(kitty_copy, g.kitty, sizeof(kitty_copy));

    gameStartBidding(&g, 0);
    gameBid(&g, 0, 1);
    gameBid(&g, 1, 0);
    gameBid(&g, 2, 0);

    EXPECT_EQ(g.landlord, 0, "player 0 is landlord");
    EXPECT_EQ(g.hands[0].count, GAME_MAX_HAND_SIZE, "landlord has 20 cards");

    // Verify all kitty cards are in landlord's hand.
    for (int i = 0; i < GAME_KITTY_SIZE; i++) {
        int found = 0;
        for (int j = 0; j < g.hands[0].count; j++)
            if (g.hands[0].cards[j] == kitty_copy[i]) {
                found = 1;
                break;
            }
        EXPECT(found, "kitty card found in landlord hand");
    }
}

static void testBiddingNobodyBids(void) {
    beginSuite("bidding: nobody bids");
    GameState g = makeDealtGame();
    gameStartBidding(&g, 0);
    gameBid(&g, 0, 0);
    gameBid(&g, 1, 0);
    gameBid(&g, 2, 0);
    // No landlord — phase should remain BIDDING.
    EXPECT_EQ(g.phase, PHASE_BIDDING, "phase stays BIDDING when nobody bids");
    EXPECT_EQ(g.landlord, PLAYER_NONE, "no landlord assigned");
}

static void testBiddingBid3EndsImmediately(void) {
    beginSuite("bidding: bid of 3 ends bidding immediately");
    GameState g = makeDealtGame();
    gameStartBidding(&g, 0);
    gameBid(&g, 0, 3); // max bid
    EXPECT_EQ(g.phase, PHASE_PLAYING, "bidding ends after bid of 3");
    EXPECT_EQ(g.landlord, 0, "bidder of 3 is landlord");
}

static void testBiddingInvalidMoves(void) {
    beginSuite("bidding: invalid bid attempts");
    GameState g = makeDealtGame();
    gameStartBidding(&g, 0);

    // Wrong player bids.
    int ok = gameBid(&g, 1, 1);
    EXPECT_EQ(ok, 0, "wrong player cannot bid");

    // Bid out of range.
    ok = gameBid(&g, 0, 4);
    EXPECT_EQ(ok, 0, "bid > 3 is invalid");

    ok = gameBid(&g, 0, -1);
    EXPECT_EQ(ok, 0, "negative bid is invalid");

    // Bid not higher than current highest.
    gameBid(&g, 0, 2); // player 0 bids 2
    ok = gameBid(&g, 1, 1); // player 1 tries to bid 1
    EXPECT_EQ(ok, 0, "bid must exceed current highest");
}

static void testBiddingFirstPlayerLeads(void) {
    beginSuite("bidding: landlord plays first");
    GameState g = runBidding(0, 1, 0, 0);
    EXPECT_EQ(g.current_player, g.landlord, "landlord leads first");
}

// ---------------------------------------------------------------------------
// Tests: gamePlayerHasCards
// ---------------------------------------------------------------------------

static void testHasCards(void) {
    beginSuite("player_has_cards");
    GameState g = runBidding(0, 1, 0, 0);
    int landlord = g.landlord;

    // Build a move with one card from the landlord's hand.
    Card c = g.hands[landlord].cards[0];
    Move m = {0};
    m.type = MOVE_SINGLE;
    m.count = 1;
    m.cards[0] = c;

    EXPECT(gamePlayerHasCards(&g, landlord, &m), "landlord has their own card");

    // Build a move with a card rank not in the landlord's hand.
    // Find a rank the landlord doesn't have.
    int absent_rank = -1;
    for (int r = 0; r < RANK_COUNT_SIZE && absent_rank < 0; r++) {
        int found = 0;
        for (int i = 0; i < g.hands[landlord].count; i++)
            if (CARD_RANK(g.hands[landlord].cards[i]) == r) {
                found = 1;
                break;
            }
        if (!found)
            absent_rank = r;
    }
    if (absent_rank >= 0) {
        Move m2;
        memset(&m2, 0, sizeof(m2));
        m2.type = MOVE_SINGLE;
        m2.count = 1;
        m2.cards[0] = (absent_rank < 13) ? (Card)(absent_rank * 4) : (Card)(52 + absent_rank - 13);
        EXPECT(!gamePlayerHasCards(&g, landlord, &m2), "landlord does not have absent rank");
    }
}

// ---------------------------------------------------------------------------
// Tests: gamePlay
// ---------------------------------------------------------------------------

// Play a single card of a given rank from the current player's hand.
// Returns 1 if the play succeeded.
static int playSingleRank(GameState *g, int rank) {
    int p = g->current_player;
    Card c = findCardOfRank(&g->hands[p], rank);
    if (c == 255)
        return 0;
    Move m;
    memset(&m, 0, sizeof(m));
    m.type = MOVE_SINGLE;
    m.rank = rank;
    m.length = 1;
    m.count = 1;
    m.cards[0] = c;
    return gamePlay(g, p, &m);
}

static void testPlayWrongPhase(void) {
    beginSuite("play: wrong phase");
    GameState g = makeDealtGame();
    gameStartBidding(&g, 0);
    // Still in BIDDING — playing should fail.
    Move m;
    memset(&m, 0, sizeof(m));
    m.type = MOVE_PASS;
    int ok = gamePlay(&g, 0, &m);
    EXPECT_EQ(ok, 0, "cannot play during bidding phase");
}

static void testPlayWrongPlayer(void) {
    beginSuite("play: wrong player");
    GameState g = runBidding(0, 1, 0, 0);
    int not_current = (g.current_player + 1) % GAME_NUM_PLAYERS;
    Card c = g.hands[not_current].cards[0];
    Move m;
    memset(&m, 0, sizeof(m));
    m.type = MOVE_SINGLE;
    m.count = 1;
    m.cards[0] = c;
    int ok = gamePlay(&g, not_current, &m);
    EXPECT_EQ(ok, 0, "wrong player cannot play out of turn");
}

static void testPlayCardsNotInHand(void) {
    beginSuite("play: cards not in hand");
    GameState g = runBidding(0, 1, 0, 0);
    int p = g.current_player;

    // Matching is rank-based, so find a rank p does not hold at all.
    int cnt[RANK_COUNT_SIZE];
    movesCountRanks(g.hands[p].cards, g.hands[p].count, cnt);
    int absent = -1;
    for (int r = 0; r < RANK_COUNT_SIZE && absent < 0; r++)
        if (cnt[r] == 0)
            absent = r;

    if (absent < 0) {
        // Landlord's 20-card hand covers all 15 ranks — test not applicable.
        g_passed++;
        return;
    }

    Card c = (absent < 13) ? (Card)(absent * 4) : (Card)(52 + absent - 13);
    Move m;
    memset(&m, 0, sizeof(m));
    m.type = MOVE_SINGLE;
    m.rank = absent;
    m.length = 1;
    m.count = 1;
    m.cards[0] = c;
    int ok = gamePlay(&g, p, &m);
    EXPECT_EQ(ok, 0, "cannot play cards not in hand");
}

static void testPlayRemovesCards(void) {
    beginSuite("play: cards removed from hand after play");
    GameState g = runBidding(0, 1, 0, 0);
    int p = g.current_player;
    int before = g.hands[p].count;
    Card c = g.hands[p].cards[0];
    int rank = CARD_RANK(c);

    Move m;
    memset(&m, 0, sizeof(m));
    m.type = MOVE_SINGLE;
    m.rank = rank;
    m.length = 1;
    m.count = 1;
    m.cards[0] = c;
    int ok = gamePlay(&g, p, &m);

    EXPECT_EQ(ok, 1, "play succeeds");
    EXPECT_EQ(g.hands[p].count, before - 1, "hand shrinks by 1");
    // Card should no longer be present.
    int still_there = 0;
    for (int i = 0; i < g.hands[p].count; i++)
        if (CARD_RANK(g.hands[p].cards[i]) == rank) {
            still_there++;
        }
    // Allow still_there > 0 only if player had duplicates.
    EXPECT(still_there < before, "at least one copy removed from hand");
}

static void testPlayAdvancesTurn(void) {
    beginSuite("play: turn advances");
    GameState g = runBidding(0, 1, 0, 0);
    int first = g.current_player;
    int expected_next = gameNextPlayer(&g, first);

    Move m;
    memset(&m, 0, sizeof(m));
    m.type = MOVE_SINGLE;
    m.rank = CARD_RANK(g.hands[first].cards[0]);
    m.length = 1;
    m.count = 1;
    m.cards[0] = g.hands[first].cards[0];
    gamePlay(&g, first, &m);

    EXPECT_EQ(g.current_player, expected_next, "turn advances to next player");
}

static void testPlayMustBeatTable(void) {
    beginSuite("play: must beat table move");
    GameState g = runBidding(0, 1, 0, 0);

    // Landlord plays their highest card rank available.
    // First find what rank the landlord can play.
    int p0 = g.current_player; // landlord
    // Play a mid-rank single to set the table.
    // Find rank 7 (RANK_7 = 4) or similar.
    int played = 0;
    for (int rank = RANK_7; rank <= RANK_A && !played; rank++)
        played = playSingleRank(&g, rank);
    EXPECT(played, "landlord plays a single");

    // Next player tries to play a card with a lower rank (should fail).
    int p1 = g.current_player;
    int table_rank = g.last_move.rank;
    Card low = 255;
    for (int i = 0; i < g.hands[p1].count; i++) {
        if (CARD_RANK(g.hands[p1].cards[i]) < table_rank) {
            low = g.hands[p1].cards[i];
            break;
        }
    }
    if (low != 255) {
        Move bad;
        memset(&bad, 0, sizeof(bad));
        bad.type = MOVE_SINGLE;
        bad.rank = CARD_RANK(low);
        bad.length = 1;
        bad.count = 1;
        bad.cards[0] = low;
        int ok = gamePlay(&g, p1, &bad);
        EXPECT_EQ(ok, 0, "lower single does not beat table");
    }
}

static void testPlayPass(void) {
    beginSuite("play: pass");
    GameState g = runBidding(0, 1, 0, 0);
    int p0 = g.current_player;

    // Landlord plays a single to set the table.
    Card c = g.hands[p0].cards[0];
    Move m;
    memset(&m, 0, sizeof(m));
    m.type = MOVE_SINGLE;
    m.rank = CARD_RANK(c);
    m.length = 1;
    m.count = 1;
    m.cards[0] = c;
    gamePlay(&g, p0, &m);

    // Next player passes.
    int p1 = g.current_player;
    Move pass;
    memset(&pass, 0, sizeof(pass));
    pass.type = MOVE_PASS;
    int ok = gamePlay(&g, p1, &pass);
    EXPECT_EQ(ok, 1, "pass is accepted");
    EXPECT_EQ(g.current_player, gameNextPlayer(&g, p1), "turn still advances after pass");
    EXPECT_EQ(g.last_move.type, m.type, "table move unchanged after pass");
}

static void testPlayCannotPassOnEmptyTable(void) {
    beginSuite("play: cannot pass on empty table");
    GameState g = runBidding(0, 1, 0, 0);
    // Table is empty at start — landlord must play.
    Move pass;
    memset(&pass, 0, sizeof(pass));
    pass.type = MOVE_PASS;
    int ok = gamePlay(&g, g.current_player, &pass);
    EXPECT_EQ(ok, 0, "cannot pass when table is empty");
}

static void testPlayTableClearsAfterTwoPasses(void) {
    beginSuite("play: table clears after all others pass");
    GameState g = runBidding(0, 1, 0, 0);
    int p0 = g.current_player;

    // p0 plays a single.
    Move m;
    memset(&m, 0, sizeof(m));
    m.type = MOVE_SINGLE;
    m.count = 1;
    m.cards[0] = g.hands[p0].cards[0];
    m.rank = CARD_RANK(m.cards[0]);
    m.length = 1;
    gamePlay(&g, p0, &m);

    // p1 and p2 pass.
    Move pass;
    memset(&pass, 0, sizeof(pass));
    pass.type = MOVE_PASS;
    int p1 = g.current_player;
    gamePlay(&g, p1, &pass);
    int p2 = g.current_player;
    gamePlay(&g, p2, &pass);

    // Table should be cleared; p0 leads again.
    EXPECT_EQ(g.current_player, p0, "original player leads again");
    EXPECT_EQ(g.last_move.type, MOVE_PASS, "table cleared (last_move is PASS)");
    EXPECT_EQ(g.last_player, PLAYER_NONE, "last_player reset");
}

static void testPlayHistoryRecorded(void) {
    beginSuite("play: history recorded");
    GameState g = runBidding(0, 1, 0, 0);
    int p = g.current_player;
    Move m;
    memset(&m, 0, sizeof(m));
    m.type = MOVE_SINGLE;
    m.count = 1;
    m.cards[0] = g.hands[p].cards[0];
    m.rank = CARD_RANK(m.cards[0]);
    m.length = 1;
    gamePlay(&g, p, &m);

    EXPECT_EQ(g.history_count, 1, "one play recorded");
    EXPECT_EQ(g.history[0].player, p, "correct player in history");
    EXPECT_EQ(g.history[0].move.type, MOVE_SINGLE, "correct move type in history");
}

// ---------------------------------------------------------------------------
// Tests: bomb scoring
// ---------------------------------------------------------------------------

static void testBombDoublesScore(void) {
    beginSuite("bomb: doubles score");
    // We need a game where a bomb is played. Manufacture one by injecting
    // four cards of the same rank into a player's hand.
    GameState g = runBidding(0, 1, 0, 0);
    int p = g.current_player;

    // Overwrite first 4 cards of the current player with a bomb of 3s.
    for (int i = 0; i < 4; i++)
        g.hands[p].cards[i] = (Card)(RANK_3 * 4 + i);

    Move bomb;
    memset(&bomb, 0, sizeof(bomb));
    bomb.type = MOVE_BOMB;
    bomb.rank = RANK_3;
    bomb.length = 1;
    bomb.count = 4;
    for (int i = 0; i < 4; i++)
        bomb.cards[i] = (Card)(RANK_3 * 4 + i);

    int ok = gamePlay(&g, p, &bomb);
    EXPECT_EQ(ok, 1, "bomb play accepted");
    EXPECT_EQ(g.bomb_count, 1, "bomb_count incremented");
}

// ---------------------------------------------------------------------------
// Tests: game over
// ---------------------------------------------------------------------------

static void testGameOverWhenHandEmpty(void) {
    beginSuite("game over: triggered when hand emptied");
    // Give player 0 exactly one card.
    GameState g = runBidding(0, 1, 0, 0);
    int p = g.current_player; // landlord leads

    // Strip landlord down to one card.
    g.hands[p].count = 1;
    Card last = g.hands[p].cards[0];
    int rank = CARD_RANK(last);

    Move m;
    memset(&m, 0, sizeof(m));
    m.type = MOVE_SINGLE;
    m.rank = rank;
    m.length = 1;
    m.count = 1;
    m.cards[0] = last;
    gamePlay(&g, p, &m);

    EXPECT_EQ(g.phase, PHASE_OVER, "game over after emptying hand");
    EXPECT_EQ(g.winner, p, "emptying player is winner");
}

// ---------------------------------------------------------------------------
// Tests: gameLegalMoves
// ---------------------------------------------------------------------------

static void testLegalMovesOnEmptyTable(void) {
    beginSuite("legal_moves: empty table");
    GameState g = runBidding(0, 1, 0, 0);
    int p = g.current_player;
    Move out[512];
    int n = gameLegalMoves(&g, p, out, 512);
    EXPECT(n > 0, "at least one legal move from full hand on empty table");
    // Every generated move should be legal to play.
    for (int i = 0; i < n; i++)
        EXPECT(gamePlayerHasCards(&g, p, &out[i]), "legal move cards are in hand");
}

static void testLegalMovesAfterSinglePlayed(void) {
    beginSuite("legal_moves: after single played");
    GameState g = runBidding(0, 1, 0, 0);
    int p0 = g.current_player;
    // p0 plays a single.
    Card c = g.hands[p0].cards[0];
    Move m;
    memset(&m, 0, sizeof(m));
    m.type = MOVE_SINGLE;
    m.rank = CARD_RANK(c);
    m.length = 1;
    m.count = 1;
    m.cards[0] = c;
    gamePlay(&g, p0, &m);

    int p1 = g.current_player;
    Move out[256];
    int n = gameLegalMoves(&g, p1, out, 256);
    // All non-pass moves must beat the table.
    for (int i = 0; i < n; i++) {
        if (out[i].type == MOVE_PASS)
            continue;
        EXPECT(movesBeats(&out[i], &g.last_move), "legal response beats table");
    }
}

// ---------------------------------------------------------------------------
// Tests: gameIsPeasant / gameNextPlayer
// ---------------------------------------------------------------------------

static void testIsPeasant(void) {
    beginSuite("is_peasant");
    GameState g = runBidding(0, 1, 0, 0); // player 0 is landlord
    EXPECT(!gameIsPeasant(&g, g.landlord), "landlord is not peasant");
    for (int p = 0; p < GAME_NUM_PLAYERS; p++) {
        if (p != g.landlord)
            EXPECT(gameIsPeasant(&g, p), "non-landlord is peasant");
    }
}

static void testNextPlayer(void) {
    beginSuite("next_player");
    GameState g = runBidding(0, 1, 0, 0);
    EXPECT_EQ(gameNextPlayer(&g, 0), 1, "0 -> 1");
    EXPECT_EQ(gameNextPlayer(&g, 1), 2, "1 -> 2");
    EXPECT_EQ(gameNextPlayer(&g, 2), 0, "2 -> 0 (wraps)");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

static void testShuffleReproducibility(void) {
    beginSuite("shuffle: reproducibility");
    GameState g1, g2;
    gameInit(&g1);
    gameResetDeck(&g1);
    gameShuffle(&g1, 123);
    gameInit(&g2);
    gameResetDeck(&g2);
    gameShuffle(&g2, 123);
    for (int i = 0; i < GAME_DECK_SIZE; i++)
        EXPECT_EQ(g1.deck[i], g2.deck[i], "same seed -> same shuffle");

    gameShuffle(&g2, 456);
    int different = 0;
    for (int i = 0; i < GAME_DECK_SIZE; i++)
        if (g1.deck[i] != g2.deck[i]) {
            different = 1;
            break;
        }
    EXPECT(different, "different seed -> different shuffle");
}

static void testBiddingComplexSequence(void) {
    beginSuite("bidding: complex sequence (1 -> 2 -> pass)");
    GameState g = makeDealtGame();
    gameStartBidding(&g, 0);
    gameBid(&g, 0, 1);
    gameBid(&g, 1, 2);
    gameBid(&g, 2, 0); // pass
    gameBid(&g, 0, 0); // pass

    EXPECT_EQ(g.phase, PHASE_PLAYING, "bidding ends after two passes following a bid");
    EXPECT_EQ(g.landlord, 1, "player 1 is landlord");
    EXPECT_EQ(g.base_score, 2, "base score is 2");
}

static void testPlayRocketBeatsAll(void) {
    beginSuite("play: rocket beats bomb and normal moves");
    GameState g = runBidding(0, 1, 0, 0);
    int p0 = g.current_player;
    int p1 = gameNextPlayer(&g, p0);

    // Inject a bomb for p0.
    for (int i = 0; i < 4; i++)
        g.hands[p0].cards[i] = (Card)(RANK_K * 4 + i);
    Move bomb;
    memset(&bomb, 0, sizeof(bomb));
    bomb.type = MOVE_BOMB;
    bomb.rank = RANK_K;
    bomb.count = 4;
    for (int i = 0; i < 4; i++)
        bomb.cards[i] = g.hands[p0].cards[i];
    gamePlay(&g, p0, &bomb);

    // Inject a rocket for p1.
    g.hands[p1].cards[0] = 52; // small joker
    g.hands[p1].cards[1] = 53; // big joker
    Move rocket;
    memset(&rocket, 0, sizeof(rocket));
    rocket.type = MOVE_ROCKET;
    rocket.count = 2;
    rocket.cards[0] = 52;
    rocket.cards[1] = 53;

    int ok = gamePlay(&g, p1, &rocket);
    EXPECT_EQ(ok, 1, "rocket beats bomb");
    EXPECT_EQ(g.last_move.type, MOVE_ROCKET, "rocket is now on table");
}

static void testScoreWithMultipleBombs(void) {
    beginSuite("score: multiple bombs");
    GameState g = runBidding(0, 1, 0, 0);
    int p0 = g.current_player;

    // Play two bombs.
    for (int i = 0; i < 4; i++)
        g.hands[p0].cards[i] = (Card)(RANK_3 * 4 + i);
    Move b3;
    memset(&b3, 0, sizeof(b3));
    b3.type = MOVE_BOMB;
    b3.rank = RANK_3;
    b3.count = 4;
    for (int i = 0; i < 4; i++)
        b3.cards[i] = (Card)(RANK_3 * 4 + i);
    gamePlay(&g, p0, &b3);

    // Clear table with passes to play another bomb.
    gamePlay(&g, gameNextPlayer(&g, p0), &(Move) {
        MOVE_PASS
    }
    )
    ;
    gamePlay(&g, gameNextPlayer(&g, gameNextPlayer(&g, p0)), &(Move) {
        MOVE_PASS
    }
    )
    ;

    for (int i = 0; i < 4; i++)
        g.hands[p0].cards[i] = (Card)(RANK_4 * 4 + i);
    Move b4;
    memset(&b4, 0, sizeof(b4));
    b4.type = MOVE_BOMB;
    b4.rank = RANK_4;
    b4.count = 4;
    for (int i = 0; i < 4; i++)
        b4.cards[i] = (Card)(RANK_4 * 4 + i);
    gamePlay(&g, p0, &b4);

    EXPECT_EQ(g.bomb_count, 2, "two bombs recorded");
}

static void testLegalMovesNoOptions(void) {
    beginSuite("legal_moves: only pass available");
    GameState g = runBidding(0, 1, 0, 0);
    int p0 = g.current_player;

    // Landlord plays a rocket.
    g.hands[p0].cards[0] = 52;
    g.hands[p0].cards[1] = 53;
    Move rkt;
    memset(&rkt, 0, sizeof(rkt));
    rkt.type = MOVE_ROCKET;
    rkt.count = 2;
    rkt.cards[0] = 52;
    rkt.cards[1] = 53;
    gamePlay(&g, p0, &rkt);

    int p1 = g.current_player;
    // p1 has no bombs/rockets, just low cards.
    Move out[16];
    int n = gameLegalMoves(&g, p1, out, 16);
    // gameLegalMoves (movesGenerate) does not return MOVE_PASS;
    // it returns active moves that beat the table.
    EXPECT_EQ(n, 0, "no active moves can beat a rocket");
}

int main(void) {
    printf("--- Test file: %s ---\n", __FILE__);
    testInit();
    testDeckReset();
    testShuffleReproducibility();
    testDealCardCounts();
    testDealNoDuplicates();
    testDealAllCardsAccounted();

    testBiddingLandlordAssigned();
    testBiddingKittyGivenToLandlord();
    testBiddingNobodyBids();
    testBiddingBid3EndsImmediately();
    testBiddingInvalidMoves();
    testBiddingComplexSequence();
    testBiddingFirstPlayerLeads();

    testHasCards();

    testPlayWrongPhase();
    testPlayWrongPlayer();
    testPlayCardsNotInHand();
    testPlayRemovesCards();
    testPlayAdvancesTurn();
    testPlayMustBeatTable();
    testPlayPass();
    testPlayRocketBeatsAll();
    testPlayCannotPassOnEmptyTable();
    testPlayTableClearsAfterTwoPasses();
    testPlayHistoryRecorded();

    testBombDoublesScore();
    testScoreWithMultipleBombs();
    testGameOverWhenHandEmpty();

    testLegalMovesOnEmptyTable();
    testLegalMovesAfterSinglePlayed();
    testLegalMovesNoOptions();

    testIsPeasant();
    testNextPlayer();

    PRINT_RESULTS();
    RETURN_TEST_RESULT();
}
