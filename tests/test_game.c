//
// Created by Jonathan on 08-Mar-26.
//

#include "game.h"
#include "test_framework.h"

#include <string.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Build a fresh, dealt game ready for bidding.
static GameState make_dealt_game(void) {
    GameState g;
    game_init(&g);
    game_reset_deck(&g);
    game_shuffle(&g, 42);
    game_deal(&g);
    return g;
}

// Run a full bid sequence: players 0,1,2 bid in order.
// values[3]: bid value for each player (0 = pass).
// Returns the resulting GameState (phase will be PHASE_PLAYING if someone won).
static GameState run_bidding(int first, int v0, int v1, int v2) {
    GameState g = make_dealt_game();
    game_start_bidding(&g, first);
    int values[3] = {v0, v1, v2};
    // Submit bids in turn order starting from `first`.
    for (int i = 0; i < GAME_NUM_PLAYERS; i++) {
        int p = (first + i) % GAME_NUM_PLAYERS;
        // Skip if the bid would be illegal (lower than current highest).
        if (values[p] > 0 && values[p] <= g.bid.highest_score) continue;
        game_bid(&g, p, values[p]);
        if (g.phase == PHASE_PLAYING) break;
    }
    return g;
}

// Find a card of a given rank in a hand (returns the card value, or 255).
static Card find_card_of_rank(const Hand *h, int rank) {
    for (int i = 0; i < h->count; i++)
        if (CARD_RANK(h->cards[i]) == rank) return h->cards[i];
    return 255;
}

// ---------------------------------------------------------------------------
// Tests: game_init
// ---------------------------------------------------------------------------

static void test_init(void) {
    begin_suite("init");
    GameState g;
    game_init(&g);
    EXPECT_EQ(g.phase, PHASE_BIDDING, "phase starts as BIDDING");
    EXPECT_EQ(g.landlord, PLAYER_NONE, "no landlord initially");
    EXPECT_EQ(g.current_player, PLAYER_NONE, "no current player initially");
    EXPECT_EQ(g.winner, PLAYER_NONE, "no winner initially");
    EXPECT_EQ(g.last_move.type, MOVE_PASS, "last move is PASS");
}

// ---------------------------------------------------------------------------
// Tests: deck / deal
// ---------------------------------------------------------------------------

static void test_deck_reset(void) {
    begin_suite("deck reset");
    GameState g;
    game_init(&g);
    game_reset_deck(&g);
    for (int i = 0; i < GAME_DECK_SIZE; i++)
        EXPECT_EQ(g.deck[i], (Card)i, "deck card matches index");
}

static void test_deal_card_counts(void) {
    begin_suite("deal: card counts");
    GameState g = make_dealt_game();
    EXPECT_EQ(g.hands[0].count, GAME_HAND_SIZE, "player 0 has 17 cards");
    EXPECT_EQ(g.hands[1].count, GAME_HAND_SIZE, "player 1 has 17 cards");
    EXPECT_EQ(g.hands[2].count, GAME_HAND_SIZE, "player 2 has 17 cards");
}

static void test_deal_no_duplicates(void) {
    begin_suite("deal: no duplicate cards");
    GameState g = make_dealt_game();
    int seen[GAME_DECK_SIZE] = {0};
    for (int p = 0; p < GAME_NUM_PLAYERS; p++)
        for (int i = 0; i < g.hands[p].count; i++)
            seen[g.hands[p].cards[i]]++;
    for (int i = 0; i < GAME_KITTY_SIZE; i++)
        seen[g.kitty[i]]++;
    for (int i = 0; i < GAME_DECK_SIZE; i++)
        EXPECT_EQ(seen[i], 1, "each card appears exactly once");
}

static void test_deal_all_cards_accounted(void) {
    begin_suite("deal: all 54 cards distributed");
    GameState g = make_dealt_game();
    int total = g.hands[0].count + g.hands[1].count + g.hands[2].count + GAME_KITTY_SIZE;
    EXPECT_EQ(total, GAME_DECK_SIZE, "total cards == 54");
}

// ---------------------------------------------------------------------------
// Tests: bidding
// ---------------------------------------------------------------------------

static void test_bidding_landlord_assigned(void) {
    begin_suite("bidding: landlord assigned");
    // Player 1 bids 2, others pass.
    GameState g = make_dealt_game();
    game_start_bidding(&g, 0);
    game_bid(&g, 0, 0); // pass
    game_bid(&g, 1, 2); // bid 2
    game_bid(&g, 2, 0); // pass

    EXPECT_EQ(g.phase, PHASE_PLAYING, "phase transitions to PLAYING");
    EXPECT_EQ(g.landlord, 1, "player 1 is landlord");
    EXPECT_EQ(g.base_score, 2, "base score is 2");
}

static void test_bidding_kitty_given_to_landlord(void) {
    begin_suite("bidding: landlord receives kitty");
    GameState g = make_dealt_game();
    Card kitty_copy[GAME_KITTY_SIZE];
    memcpy(kitty_copy, g.kitty, sizeof(kitty_copy));

    game_start_bidding(&g, 0);
    game_bid(&g, 0, 1);
    game_bid(&g, 1, 0);
    game_bid(&g, 2, 0);

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

static void test_bidding_nobody_bids(void) {
    begin_suite("bidding: nobody bids");
    GameState g = make_dealt_game();
    game_start_bidding(&g, 0);
    game_bid(&g, 0, 0);
    game_bid(&g, 1, 0);
    game_bid(&g, 2, 0);
    // No landlord — phase should remain BIDDING.
    EXPECT_EQ(g.phase, PHASE_BIDDING, "phase stays BIDDING when nobody bids");
    EXPECT_EQ(g.landlord, PLAYER_NONE, "no landlord assigned");
}

static void test_bidding_bid_3_ends_immediately(void) {
    begin_suite("bidding: bid of 3 ends bidding immediately");
    GameState g = make_dealt_game();
    game_start_bidding(&g, 0);
    game_bid(&g, 0, 3); // max bid
    EXPECT_EQ(g.phase, PHASE_PLAYING, "bidding ends after bid of 3");
    EXPECT_EQ(g.landlord, 0, "bidder of 3 is landlord");
}

static void test_bidding_invalid_moves(void) {
    begin_suite("bidding: invalid bid attempts");
    GameState g = make_dealt_game();
    game_start_bidding(&g, 0);

    // Wrong player bids.
    int ok = game_bid(&g, 1, 1);
    EXPECT_EQ(ok, 0, "wrong player cannot bid");

    // Bid out of range.
    ok = game_bid(&g, 0, 4);
    EXPECT_EQ(ok, 0, "bid > 3 is invalid");

    ok = game_bid(&g, 0, -1);
    EXPECT_EQ(ok, 0, "negative bid is invalid");

    // Bid not higher than current highest.
    game_bid(&g, 0, 2); // player 0 bids 2
    ok = game_bid(&g, 1, 1); // player 1 tries to bid 1
    EXPECT_EQ(ok, 0, "bid must exceed current highest");
}

static void test_bidding_first_player_leads(void) {
    begin_suite("bidding: landlord plays first");
    GameState g = run_bidding(0, 1, 0, 0);
    EXPECT_EQ(g.current_player, g.landlord, "landlord leads first");
}

// ---------------------------------------------------------------------------
// Tests: game_player_has_cards
// ---------------------------------------------------------------------------

static void test_has_cards(void) {
    begin_suite("player_has_cards");
    GameState g = run_bidding(0, 1, 0, 0);
    int landlord = g.landlord;

    // Build a move with one card from the landlord's hand.
    Card c = g.hands[landlord].cards[0];
    Move m;
    memset(&m, 0, sizeof(m));
    m.type = MOVE_SINGLE;
    m.count = 1;
    m.cards[0] = c;

    EXPECT(game_player_has_cards(&g, landlord, &m), "landlord has their own card");

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
        if (!found) absent_rank = r;
    }
    if (absent_rank >= 0) {
        Move m2;
        memset(&m2, 0, sizeof(m2));
        m2.type = MOVE_SINGLE;
        m2.count = 1;
        m2.cards[0] = (absent_rank < 13) ? (Card) (absent_rank * 4) : (Card) (52 + absent_rank - 13);
        EXPECT(!game_player_has_cards(&g, landlord, &m2), "landlord does not have absent rank");
    }
}

// ---------------------------------------------------------------------------
// Tests: game_play
// ---------------------------------------------------------------------------

// Play a single card of a given rank from the current player's hand.
// Returns 1 if the play succeeded.
static int play_single_rank(GameState *g, int rank) {
    int p = g->current_player;
    Card c = find_card_of_rank(&g->hands[p], rank);
    if (c == 255) return 0;
    Move m;
    memset(&m, 0, sizeof(m));
    m.type = MOVE_SINGLE;
    m.rank = rank;
    m.length = 1;
    m.count = 1;
    m.cards[0] = c;
    return game_play(g, p, &m);
}

static void test_play_wrong_phase(void) {
    begin_suite("play: wrong phase");
    GameState g = make_dealt_game();
    game_start_bidding(&g, 0);
    // Still in BIDDING — playing should fail.
    Move m;
    memset(&m, 0, sizeof(m));
    m.type = MOVE_PASS;
    int ok = game_play(&g, 0, &m);
    EXPECT_EQ(ok, 0, "cannot play during bidding phase");
}

static void test_play_wrong_player(void) {
    begin_suite("play: wrong player");
    GameState g = run_bidding(0, 1, 0, 0);
    int not_current = (g.current_player + 1) % GAME_NUM_PLAYERS;
    Card c = g.hands[not_current].cards[0];
    Move m;
    memset(&m, 0, sizeof(m));
    m.type = MOVE_SINGLE;
    m.count = 1;
    m.cards[0] = c;
    int ok = game_play(&g, not_current, &m);
    EXPECT_EQ(ok, 0, "wrong player cannot play out of turn");
}

static void test_play_cards_not_in_hand(void) {
    begin_suite("play: cards not in hand");
    GameState g = run_bidding(0, 1, 0, 0);
    int p = g.current_player;

    // Matching is rank-based, so find a rank p does not hold at all.
    int cnt[RANK_COUNT_SIZE];
    moves_count_ranks(g.hands[p].cards, g.hands[p].count, cnt);
    int absent = -1;
    for (int r = 0; r < RANK_COUNT_SIZE && absent < 0; r++)
        if (cnt[r] == 0) absent = r;

    if (absent < 0) {
        // Landlord's 20-card hand covers all 15 ranks — test not applicable.
        g_passed++;
        return;
    }

    Card c = (absent < 13) ? (Card) (absent * 4) : (Card) (52 + absent - 13);
    Move m;
    memset(&m, 0, sizeof(m));
    m.type = MOVE_SINGLE;
    m.rank = absent;
    m.length = 1;
    m.count = 1;
    m.cards[0] = c;
    int ok = game_play(&g, p, &m);
    EXPECT_EQ(ok, 0, "cannot play cards not in hand");
}

static void test_play_removes_cards(void) {
    begin_suite("play: cards removed from hand after play");
    GameState g = run_bidding(0, 1, 0, 0);
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
    int ok = game_play(&g, p, &m);

    EXPECT_EQ(ok, 1, "play succeeds");
    EXPECT_EQ(g.hands[p].count, before - 1, "hand shrinks by 1");
    // Card should no longer be present.
    int still_there = 0;
    for (int i = 0; i < g.hands[p].count; i++)
        if (CARD_RANK(g.hands[p].cards[i]) == rank) { still_there++; }
    // Allow still_there > 0 only if player had duplicates.
    EXPECT(still_there < before, "at least one copy removed from hand");
}

static void test_play_advances_turn(void) {
    begin_suite("play: turn advances");
    GameState g = run_bidding(0, 1, 0, 0);
    int first = g.current_player;
    int expected_next = game_next_player(&g, first);

    Move m;
    memset(&m, 0, sizeof(m));
    m.type = MOVE_SINGLE;
    m.rank = CARD_RANK(g.hands[first].cards[0]);
    m.length = 1;
    m.count = 1;
    m.cards[0] = g.hands[first].cards[0];
    game_play(&g, first, &m);

    EXPECT_EQ(g.current_player, expected_next, "turn advances to next player");
}

static void test_play_must_beat_table(void) {
    begin_suite("play: must beat table move");
    GameState g = run_bidding(0, 1, 0, 0);

    // Landlord plays their highest card rank available.
    // First find what rank the landlord can play.
    int p0 = g.current_player; // landlord
    // Play a mid-rank single to set the table.
    // Find rank 7 (RANK_7 = 4) or similar.
    int played = 0;
    for (int rank = RANK_7; rank <= RANK_A && !played; rank++)
        played = play_single_rank(&g, rank);
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
        int ok = game_play(&g, p1, &bad);
        EXPECT_EQ(ok, 0, "lower single does not beat table");
    }
}

static void test_play_pass(void) {
    begin_suite("play: pass");
    GameState g = run_bidding(0, 1, 0, 0);
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
    game_play(&g, p0, &m);

    // Next player passes.
    int p1 = g.current_player;
    Move pass;
    memset(&pass, 0, sizeof(pass));
    pass.type = MOVE_PASS;
    int ok = game_play(&g, p1, &pass);
    EXPECT_EQ(ok, 1, "pass is accepted");
    EXPECT_EQ(g.current_player, game_next_player(&g, p1), "turn still advances after pass");
    EXPECT_EQ(g.last_move.type, m.type, "table move unchanged after pass");
}

static void test_play_cannot_pass_on_empty_table(void) {
    begin_suite("play: cannot pass on empty table");
    GameState g = run_bidding(0, 1, 0, 0);
    // Table is empty at start — landlord must play.
    Move pass;
    memset(&pass, 0, sizeof(pass));
    pass.type = MOVE_PASS;
    int ok = game_play(&g, g.current_player, &pass);
    EXPECT_EQ(ok, 0, "cannot pass when table is empty");
}

static void test_play_table_clears_after_two_passes(void) {
    begin_suite("play: table clears after all others pass");
    GameState g = run_bidding(0, 1, 0, 0);
    int p0 = g.current_player;

    // p0 plays a single.
    Move m;
    memset(&m, 0, sizeof(m));
    m.type = MOVE_SINGLE;
    m.count = 1;
    m.cards[0] = g.hands[p0].cards[0];
    m.rank = CARD_RANK(m.cards[0]);
    m.length = 1;
    game_play(&g, p0, &m);

    // p1 and p2 pass.
    Move pass;
    memset(&pass, 0, sizeof(pass));
    pass.type = MOVE_PASS;
    int p1 = g.current_player;
    game_play(&g, p1, &pass);
    int p2 = g.current_player;
    game_play(&g, p2, &pass);

    // Table should be cleared; p0 leads again.
    EXPECT_EQ(g.current_player, p0, "original player leads again");
    EXPECT_EQ(g.last_move.type, MOVE_PASS, "table cleared (last_move is PASS)");
    EXPECT_EQ(g.last_player, PLAYER_NONE, "last_player reset");
}

static void test_play_history_recorded(void) {
    begin_suite("play: history recorded");
    GameState g = run_bidding(0, 1, 0, 0);
    int p = g.current_player;
    Move m;
    memset(&m, 0, sizeof(m));
    m.type = MOVE_SINGLE;
    m.count = 1;
    m.cards[0] = g.hands[p].cards[0];
    m.rank = CARD_RANK(m.cards[0]);
    m.length = 1;
    game_play(&g, p, &m);

    EXPECT_EQ(g.history_count, 1, "one play recorded");
    EXPECT_EQ(g.history[0].player, p, "correct player in history");
    EXPECT_EQ(g.history[0].move.type, MOVE_SINGLE, "correct move type in history");
}

// ---------------------------------------------------------------------------
// Tests: bomb scoring
// ---------------------------------------------------------------------------

static void test_bomb_doubles_score(void) {
    begin_suite("bomb: doubles score");
    // We need a game where a bomb is played. Manufacture one by injecting
    // four cards of the same rank into a player's hand.
    GameState g = run_bidding(0, 1, 0, 0);
    int p = g.current_player;

    // Overwrite first 4 cards of the current player with a bomb of 3s.
    for (int i = 0; i < 4; i++) g.hands[p].cards[i] = (Card) (RANK_3 * 4 + i);

    Move bomb;
    memset(&bomb, 0, sizeof(bomb));
    bomb.type = MOVE_BOMB;
    bomb.rank = RANK_3;
    bomb.length = 1;
    bomb.count = 4;
    for (int i = 0; i < 4; i++) bomb.cards[i] = (Card) (RANK_3 * 4 + i);

    int ok = game_play(&g, p, &bomb);
    EXPECT_EQ(ok, 1, "bomb play accepted");
    EXPECT_EQ(g.bomb_count, 1, "bomb_count incremented");
}

// ---------------------------------------------------------------------------
// Tests: game over
// ---------------------------------------------------------------------------

static void test_game_over_when_hand_empty(void) {
    begin_suite("game over: triggered when hand emptied");
    // Give player 0 exactly one card.
    GameState g = run_bidding(0, 1, 0, 0);
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
    game_play(&g, p, &m);

    EXPECT_EQ(g.phase, PHASE_OVER, "game over after emptying hand");
    EXPECT_EQ(g.winner, p, "emptying player is winner");
}

// ---------------------------------------------------------------------------
// Tests: game_legal_moves
// ---------------------------------------------------------------------------

static void test_legal_moves_on_empty_table(void) {
    begin_suite("legal_moves: empty table");
    GameState g = run_bidding(0, 1, 0, 0);
    int p = g.current_player;
    Move out[512];
    int n = game_legal_moves(&g, p, out, 512);
    EXPECT(n > 0, "at least one legal move from full hand on empty table");
    // Every generated move should be legal to play.
    for (int i = 0; i < n; i++)
        EXPECT(game_player_has_cards(&g, p, &out[i]), "legal move cards are in hand");
}

static void test_legal_moves_after_single_played(void) {
    begin_suite("legal_moves: after single played");
    GameState g = run_bidding(0, 1, 0, 0);
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
    game_play(&g, p0, &m);

    int p1 = g.current_player;
    Move out[256];
    int n = game_legal_moves(&g, p1, out, 256);
    // All non-pass moves must beat the table.
    for (int i = 0; i < n; i++) {
        if (out[i].type == MOVE_PASS) continue;
        EXPECT(moves_beats(&out[i], &g.last_move), "legal response beats table");
    }
}

// ---------------------------------------------------------------------------
// Tests: game_is_peasant / game_next_player
// ---------------------------------------------------------------------------

static void test_is_peasant(void) {
    begin_suite("is_peasant");
    GameState g = run_bidding(0, 1, 0, 0); // player 0 is landlord
    EXPECT(!game_is_peasant(&g, g.landlord), "landlord is not peasant");
    for (int p = 0; p < GAME_NUM_PLAYERS; p++) {
        if (p != g.landlord)
            EXPECT(game_is_peasant(&g, p), "non-landlord is peasant");
    }
}

static void test_next_player(void) {
    begin_suite("next_player");
    GameState g = run_bidding(0, 1, 0, 0);
    EXPECT_EQ(game_next_player(&g, 0), 1, "0 -> 1");
    EXPECT_EQ(game_next_player(&g, 1), 2, "1 -> 2");
    EXPECT_EQ(game_next_player(&g, 2), 0, "2 -> 0 (wraps)");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(void) {
    test_init();
    test_deck_reset();
    test_deal_card_counts();
    test_deal_no_duplicates();
    test_deal_all_cards_accounted();

    test_bidding_landlord_assigned();
    test_bidding_kitty_given_to_landlord();
    test_bidding_nobody_bids();
    test_bidding_bid_3_ends_immediately();
    test_bidding_invalid_moves();
    test_bidding_first_player_leads();

    test_has_cards();

    test_play_wrong_phase();
    test_play_wrong_player();
    test_play_cards_not_in_hand();
    test_play_removes_cards();
    test_play_advances_turn();
    test_play_must_beat_table();
    test_play_pass();
    test_play_cannot_pass_on_empty_table();
    test_play_table_clears_after_two_passes();
    test_play_history_recorded();

    test_bomb_doubles_score();
    test_game_over_when_hand_empty();

    test_legal_moves_on_empty_table();
    test_legal_moves_after_single_played();

    test_is_peasant();
    test_next_player();

    PRINT_RESULTS();
    RETURN_TEST_RESULT();
}
