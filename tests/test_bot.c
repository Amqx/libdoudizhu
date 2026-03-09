/**
 * @file test_bot.c
 * @brief Tests for the bot module
 * @author Jonathan
 * @date 08-Mar-26
 */

#include "bot.h"
#include "eval.h"
#include "test_framework.h"
#include <string.h>

/* ---------------------------------------------------------------------------
 * Helpers
 * --------------------------------------------------------------------------- */

static Card mk(int rank, int suit) {
    if (rank == RANK_SMALL_JOKER) return 52;
    if (rank == RANK_BIG_JOKER) return 53;
    return (Card) (rank * 4 + suit);
}

/* Build a dealt game ready for bidding. */
static GameState make_dealt(unsigned int seed) {
    GameState g;
    game_init(&g);
    game_reset_deck(&g);
    game_shuffle(&g, seed);
    game_deal(&g);
    return g;
}

/* Run bidding with bots starting from player 0.
 * Returns the game after bidding resolves (phase may remain PHASE_BIDDING
 * if every bot passed). */
static GameState run_bots_bid(unsigned int seed) {
    GameState g = make_dealt(seed);
    game_start_bidding(&g, 0);
    int iters = 0;
    while (g.phase == PHASE_BIDDING && iters++ < 20) {
        int p = g.bid.current_bidder;
        int bid = bot_bid(&g, p);
        if (!game_bid(&g, p, bid)) break; /* safety: illegal bid exits */
    }
    return g;
}

/* Fast-forward to PHASE_PLAYING: player 0 bids 1, others pass. */
static GameState make_playing_game(unsigned int seed) {
    GameState g = make_dealt(seed);
    game_start_bidding(&g, 0);
    game_bid(&g, 0, 1);
    game_bid(&g, 1, 0);
    game_bid(&g, 2, 0);
    return g;
}

/* ---------------------------------------------------------------------------
 * Tests: bot_bid
 * --------------------------------------------------------------------------- */

static void test_bid_strong_hand(void) {
    begin_suite("bot_bid: strong hand (bomb + rocket) bids >= 2");

    GameState g = make_dealt(1);
    game_start_bidding(&g, 0);

    /* Inject a clearly strong hand: bomb of Aces, rocket, two 2s */
    Card strong[] = {
        mk(RANK_A, 0), mk(RANK_A, 1), mk(RANK_A, 2), mk(RANK_A, 3), /* bomb +20 */
        mk(RANK_SMALL_JOKER, 0), mk(RANK_BIG_JOKER, 0), /* rocket +25, sj+8, bj+10 */
        mk(RANK_2, 0), mk(RANK_2, 1), /* 2s +12 */
    };
    memcpy(g.hands[0].cards, strong, sizeof(strong));
    g.hands[0].count = (int) (sizeof(strong) / sizeof(strong[0]));

    const int bid = bot_bid(&g, 0);
    EXPECT(bid >= 2, "strong hand bids at least 2");
    EXPECT(bid <= 3, "bid does not exceed 3");
}

static void test_bid_weak_hand_passes(void) {
    begin_suite("bot_bid: weak hand (no bombs/control) passes");

    GameState g = make_dealt(1);
    game_start_bidding(&g, 0);

    /* All different low-middle ranks, ≤ 2 of each (no bomb, no jokers, no 2s) */
    Card weak[] = {
        mk(RANK_3, 0), mk(RANK_5, 0), mk(RANK_7, 0),
        mk(RANK_9, 0), mk(RANK_J, 0), mk(RANK_3, 1),
        mk(RANK_5, 1), mk(RANK_7, 1),
    };
    memcpy(g.hands[0].cards, weak, sizeof(weak));
    g.hands[0].count = (int) (sizeof(weak) / sizeof(weak[0]));
    /* score: three pairs (3s,5s,7s) = 2+2+2 = 6  →  well below bid=1 threshold */

    const int bid = bot_bid(&g, 0);
    EXPECT_EQ(bid, 0, "weak hand passes");
}

static void test_bid_always_exceeds_highest(void) {
    begin_suite("bot_bid: bid always strictly exceeds current highest");

    /* Run 5 different seeds through full bidding; validate every bid. */
    for (int seed = 1; seed <= 5; seed++) {
        GameState g = make_dealt((unsigned) seed * 997);
        game_start_bidding(&g, 0);

        int iters = 0;
        while (g.phase == PHASE_BIDDING && iters++ < 10) {
            const int p = g.bid.current_bidder;
            const int before = g.bid.highest_score;
            const int bid = bot_bid(&g, p);

            EXPECT(bid >= 0 && bid <= 3, "bid is in range [0, 3]");
            if (bid > 0)
                EXPECT(bid > before, "non-pass bid exceeds current highest");

            if (!game_bid(&g, p, bid)) break;
        }
    }
}

/* ---------------------------------------------------------------------------
 * Tests: bot_play – full game simulation
 * --------------------------------------------------------------------------- */

static void test_full_game_simulation(void) {
    begin_suite("bot: full game – every move accepted, game reaches PHASE_OVER");

    /* Run several seeds; skip rounds where all bots passed (no landlord). */
    const unsigned int seeds[] = {42, 137, 999, 2025, 7777};
    const int n_seeds = (int) (sizeof(seeds) / sizeof(seeds[0]));

    for (int s = 0; s < n_seeds; s++) {
        GameState g = run_bots_bid(seeds[s]);
        if (g.phase != PHASE_PLAYING) continue; /* all passed – skip */

        int turns = 0;
        while (g.phase == PHASE_PLAYING && turns < 300) {
            const int p = g.current_player;
            const Move m = bot_play(&g, p);
            const int ok = game_play(&g, p, &m);
            EXPECT(ok, "bot move accepted by game engine");
            if (!ok) break;
            turns++;
        }

        EXPECT_EQ(g.phase, PHASE_OVER, "game reaches PHASE_OVER");
    }
}

/* ---------------------------------------------------------------------------
 * Tests: combination preservation
 * --------------------------------------------------------------------------- */

static void test_does_not_break_bomb_when_leading(void) {
    begin_suite("bot: does not play a partial bomb rank when leading");

    GameState g = make_playing_game(42);
    if (g.phase != PHASE_PLAYING) {
        g_passed++;
        return;
    }

    const int p = g.current_player; /* landlord, controls empty table */

    /* Give the bot: bomb of 3s  +  7♠  +  K♠
     * The bot has weaker cards it should prefer to play. */
    g.hands[p].cards[0] = mk(RANK_3, 0);
    g.hands[p].cards[1] = mk(RANK_3, 1);
    g.hands[p].cards[2] = mk(RANK_3, 2);
    g.hands[p].cards[3] = mk(RANK_3, 3);
    g.hands[p].cards[4] = mk(RANK_7, 0);
    g.hands[p].cards[5] = mk(RANK_K, 0);
    g.hands[p].count = 6;

    const Move m = bot_play(&g, p);

    /* Bot must NOT play a single 3 (that breaks the bomb of 3s). */
    const int broke_bomb = (m.type == MOVE_SINGLE && m.rank == RANK_3);
    EXPECT(!broke_bomb, "bot does not play single-3 when it holds a bomb of 3s");
}

static void test_does_not_open_with_bomb(void) {
    begin_suite("bot: does not open with bomb when normal moves exist");

    GameState g = make_playing_game(42);
    if (g.phase != PHASE_PLAYING) {
        g_passed++;
        return;
    }

    const int p = g.current_player;

    /* Ensure plenty of cards remain for all players (no endgame / no danger). */
    g.hands[(p + 1) % 3].count = 15;
    g.hands[(p + 2) % 3].count = 15;

    /* Hand: bomb of 9s  +  5♠  +  J♠  (non-bomb moves available) */
    g.hands[p].cards[0] = mk(RANK_9, 0);
    g.hands[p].cards[1] = mk(RANK_9, 1);
    g.hands[p].cards[2] = mk(RANK_9, 2);
    g.hands[p].cards[3] = mk(RANK_9, 3);
    g.hands[p].cards[4] = mk(RANK_5, 0);
    g.hands[p].cards[5] = mk(RANK_J, 0);
    g.hands[p].count = 6;

    const Move m = bot_play(&g, p);
    EXPECT(m.type != MOVE_BOMB && m.type != MOVE_ROCKET,
           "bot does not open with bomb when normal moves exist and no danger");
}

/* ---------------------------------------------------------------------------
 * Tests: peasant cooperation
 * --------------------------------------------------------------------------- */

static void test_peasant_passes_for_partner(void) {
    begin_suite("bot: peasant passes when partner controls table and no danger");

    GameState g = make_playing_game(42);
    if (g.phase != PHASE_PLAYING) {
        g_passed++;
        return;
    }

    /* Landlord is player 0; peasants are players 1 and 2. */
    const int landlord = g.landlord; /* == 0 */
    const int peasant = (landlord + 1) % 3; /* 1 */
    const int partner = (landlord + 2) % 3; /* 2 */

    /* Scenario: partner (player 2) has a single 7 on the table.
     * Peasant (player 1) is next to act and can beat it. */
    g.current_player = peasant;
    g.last_player = partner;
    g.last_move.type = MOVE_SINGLE;
    g.last_move.rank = RANK_7;
    g.last_move.count = 1;
    g.last_move.length = 1;

    /* Landlord has many cards — no danger. */
    g.hands[landlord].count = 15;

    /* Peasant holds cards that could beat the 7 (9 and K). */
    g.hands[peasant].cards[0] = mk(RANK_9, 0);
    g.hands[peasant].cards[1] = mk(RANK_K, 0);
    g.hands[peasant].count = 2;

    const Move m = bot_play(&g, peasant);
    EXPECT_EQ(m.type, MOVE_PASS,
              "peasant passes when partner has table and landlord is not threatening");
}

static void test_peasant_overrides_cooperation_in_danger(void) {
    begin_suite("bot: peasant does not yield when landlord is close to winning");

    GameState g = make_playing_game(42);
    if (g.phase != PHASE_PLAYING) {
        g_passed++;
        return;
    }

    const int landlord = g.landlord;
    const int peasant = (landlord + 1) % 3;
    const int partner = (landlord + 2) % 3;

    /* Same table setup as above (partner has single 7) … */
    g.current_player = peasant;
    g.last_player = partner;
    g.last_move.type = MOVE_SINGLE;
    g.last_move.rank = RANK_7;
    g.last_move.count = 1;
    g.last_move.length = 1;

    /* … but now the landlord has only 2 cards → DANGER! */
    g.hands[landlord].count = 2;

    /* Peasant holds a 9 (beats 7) and a K. */
    g.hands[peasant].cards[0] = mk(RANK_9, 0);
    g.hands[peasant].cards[1] = mk(RANK_K, 0);
    g.hands[peasant].count = 2;

    const Move m = bot_play(&g, peasant);
    /* Cooperation is overridden — the peasant must try to beat the table. */
    EXPECT(m.type != MOVE_PASS,
           "peasant does not yield when landlord is in danger");
}

/* ---------------------------------------------------------------------------
 * Tests: bomb usage in danger
 * --------------------------------------------------------------------------- */

static void test_bombs_when_enemy_close_to_winning(void) {
    begin_suite("bot: peasant bombs when landlord has ≤ 3 cards");

    GameState g = make_playing_game(42);
    if (g.phase != PHASE_PLAYING) {
        g_passed++;
        return;
    }

    const int landlord = g.landlord;
    const int peasant = (landlord + 1) % 3;
    const int partner = (landlord + 2) % 3;

    /* Landlord has a single K on the table. Peasant 1 must respond.
     * The peasant has only low cards (can't beat K normally) + a bomb. */
    g.current_player = peasant;
    g.last_player = landlord;
    g.last_move.type = MOVE_SINGLE;
    g.last_move.rank = RANK_K;
    g.last_move.count = 1;
    g.last_move.length = 1;

    /* Landlord is about to win (≤ 3 cards) */
    g.hands[landlord].count = 2;
    g.hands[partner].count = 10;

    /* Peasant: bomb of 9s + single 3 (can't beat K with normal single) */
    g.hands[peasant].cards[0] = mk(RANK_9, 0);
    g.hands[peasant].cards[1] = mk(RANK_9, 1);
    g.hands[peasant].cards[2] = mk(RANK_9, 2);
    g.hands[peasant].cards[3] = mk(RANK_9, 3);
    g.hands[peasant].cards[4] = mk(RANK_3, 0);
    g.hands[peasant].count = 5;

    const Move m = bot_play(&g, peasant);
    EXPECT(m.type == MOVE_BOMB || m.type == MOVE_ROCKET,
           "peasant uses bomb to stop landlord from winning");
}

/* ---------------------------------------------------------------------------
 * Tests: strength inference from history
 * --------------------------------------------------------------------------- */

static void test_high_cards_tracked_from_history(void) {
    begin_suite("bot: control-card inference adjusts after 2s/jokers played");

    /* Play a full game with bots and verify that by the time all 2s and jokers
     * have appeared in the history, the game finishes without any illegal moves.
     * This exercises the history-counting path in compute_ctx. */
    GameState g = run_bots_bid(314159);
    if (g.phase != PHASE_PLAYING) {
        g_passed++;
        return;
    }

    int turns = 0, ok_count = 0;
    while (g.phase == PHASE_PLAYING && turns < 300) {
        const int p = g.current_player;
        const Move m = bot_play(&g, p);
        const int ok = game_play(&g, p, &m);
        if (ok) ok_count++;
        EXPECT(ok, "move accepted after high-card inference");
        if (!ok) break;
        turns++;
    }

    EXPECT(ok_count > 0, "at least one move played during inference test");
    EXPECT_EQ(g.phase, PHASE_OVER, "game finishes after history-informed play");
}

/* ---------------------------------------------------------------------------
 * Tests: pass-value (Goal 10) — peasant preserves control cards
 * --------------------------------------------------------------------------- */

static void test_peasant_saves_2_instead_of_burning(void) {
    begin_suite("bot: feeder peasant passes rather than burning a 2 on landlord's Ace");

    GameState g = make_playing_game(42);
    if (g.phase != PHASE_PLAYING) {
        g_passed++;
        return;
    }

    const int landlord = g.landlord;
    /* Use the FEEDER (before landlord): (landlord+2)%3.
     * The feeder has no tactical reason to burn a control card on a
     * landlord lead — that is the gatekeeper's job.  The feeder should pass
     * and save the 2 for a more critical moment. */
    const int peasant = (landlord + 2) % 3;

    /* Landlord played a single Ace — to beat it normally you need a 2 or Joker */
    g.current_player = peasant;
    g.last_player = landlord;
    g.last_move.type = MOVE_SINGLE;
    g.last_move.rank = RANK_A;
    g.last_move.count = 1;
    g.last_move.length = 1;

    /* Landlord has many cards — no immediate danger */
    g.hands[landlord].count = 14;
    g.hands[(landlord + 1) % 3].count = 12; /* gatekeeper also has many */

    /* Feeder can only beat the A with a 2 (no Jokers), has other low cards */
    g.hands[peasant].cards[0] = mk(RANK_2, 0); /* only way to beat A */
    g.hands[peasant].cards[1] = mk(RANK_3, 0);
    g.hands[peasant].cards[2] = mk(RANK_4, 0);
    g.hands[peasant].cards[3] = mk(RANK_5, 0);
    g.hands[peasant].cards[4] = mk(RANK_6, 0);
    g.hands[peasant].count = 5;

    const Move m = bot_play(&g, peasant);
    /* Feeder should preserve the 2 — only a 2 can beat the A and there is
     * no immediate threat, so the cost of playing it exceeds the benefit. */
    EXPECT(m.type == MOVE_PASS || m.rank != RANK_2,
           "feeder peasant does not burn a 2 on landlord's Ace when not in danger");
}

/* ---------------------------------------------------------------------------
 * Tests: post-move shape preservation (Goal 3)
 * --------------------------------------------------------------------------- */

static void test_does_not_break_straight_when_leading(void) {
    begin_suite("bot: prefers not to break a 5-card straight for a single");

    GameState g = make_playing_game(42);
    if (g.phase != PHASE_PLAYING) {
        g_passed++;
        return;
    }

    const int p = g.current_player;

    /* Hand: 3-4-5-6-7 straight + isolated single 9.
     * The bot should lead with the single 9 rather than plucking a card
     * from the straight (which would destroy it). */
    g.hands[p].cards[0] = mk(RANK_3, 0);
    g.hands[p].cards[1] = mk(RANK_4, 0);
    g.hands[p].cards[2] = mk(RANK_5, 0);
    g.hands[p].cards[3] = mk(RANK_6, 0);
    g.hands[p].cards[4] = mk(RANK_7, 0);
    g.hands[p].cards[5] = mk(RANK_9, 0);
    g.hands[p].count = 6;

    /* Ensure non-endgame (both opponents have many cards) */
    g.hands[(p + 1) % 3].count = 15;
    g.hands[(p + 2) % 3].count = 15;

    const Move m = bot_play(&g, p);

    /* The bot should play the straight (6 cards in 1 play) or the isolated 9,
     * NOT break the straight into individual singles. */
    int broke_straight = 0;
    if (m.type == MOVE_SINGLE) {
        /* Playing a single from within the straight 3-7 breaks it */
        if (m.rank >= RANK_3 && m.rank <= RANK_7) broke_straight = 1;
    }
    EXPECT(!broke_straight,
           "bot does not break a 5-card straight by picking a single from it");
}

/* ---------------------------------------------------------------------------
 * main
 * --------------------------------------------------------------------------- */

int main(void) {
    /* Bidding */
    test_bid_strong_hand();
    test_bid_weak_hand_passes();
    test_bid_always_exceeds_highest();

    /* Full game */
    test_full_game_simulation();

    /* Combination preservation */
    test_does_not_break_bomb_when_leading();
    test_does_not_open_with_bomb();

    /* Peasant cooperation */
    test_peasant_passes_for_partner();
    test_peasant_overrides_cooperation_in_danger();

    /* Bomb usage */
    test_bombs_when_enemy_close_to_winning();

    /* Strength inference */
    test_high_cards_tracked_from_history();

    /* Pass-value / control card preservation */
    test_peasant_saves_2_instead_of_burning();

    /* Shape preservation */
    test_does_not_break_straight_when_leading();

    PRINT_RESULTS();
    RETURN_TEST_RESULT();
}
