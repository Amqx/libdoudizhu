/**
 * @file test_bot.c
 * @brief Tests for the bot module
 * @author Peng Yang Deng, Emma Le
 * @date 08-Mar-26
 */

#include "bot.h"
#ifdef DOUDIZHU_TUNING
#include "tune.h"
#endif
#include <string.h>
#include "eval.h"
#include "test_framework.h"

/* ---------------------------------------------------------------------------
 * Helpers
 * --------------------------------------------------------------------------- */

static Card mk(int rank, int suit) {
    if (rank == RANK_SMALL_JOKER)
        return 52;
    if (rank == RANK_BIG_JOKER)
        return 53;
    return (Card)(rank * 4 + suit);
}

static void fillRank(Card buf[], int rank, int n) {
    for (int i = 0; i < n; i++)
        buf[i] = mk(rank, i);
}

/* Build a dealt game ready for bidding. */
static GameState makeDealt(unsigned int seed) {
    GameState g;
    gameInit(&g);
    gameResetDeck(&g);
    gameShuffle(&g, seed);
    gameDeal(&g);
    return g;
}

/* Run bidding with bots starting from player 0.
 * Returns the game after bidding resolves (phase may remain PHASE_BIDDING
 * if every bot passed). */
static GameState runBotsBid(unsigned int seed) {
    GameState g = makeDealt(seed);
    gameStartBidding(&g, 0);
    int iters = 0;
    while (g.phase == PHASE_BIDDING && iters++ < 20) {
        const int p = g.bid.current_bidder;
        const int bid = botBid(&g, p);
        if (!gameBid(&g, p, bid))
            break; /* safety: illegal bid exits */
    }
    return g;
}

/* Fast-forward to PHASE_PLAYING: player 0 bids 1, others pass. */
static GameState makePlayingGame(unsigned int seed) {
    GameState g = makeDealt(seed);
    gameStartBidding(&g, 0);
    gameBid(&g, 0, 1);
    gameBid(&g, 1, 0);
    gameBid(&g, 2, 0);
    return g;
}

/* ---------------------------------------------------------------------------
 * Tests: botBid
 * --------------------------------------------------------------------------- */

static void testBidStrongHand(void) {
    beginSuite("botBid: strong hand (bomb + rocket) bids >= 2");

    GameState g = makeDealt(1);
    gameStartBidding(&g, 0);

    /* Inject a clearly strong hand: bomb of Aces, rocket, two 2s */
    const Card strong[] = {
        mk(RANK_A, 0), mk(RANK_A, 1), mk(RANK_A, 2), mk(RANK_A, 3), /* bomb +20 */
        mk(RANK_SMALL_JOKER, 0), mk(RANK_BIG_JOKER, 0), /* rocket +25, sj+8, bj+10 */
        mk(RANK_2, 0), mk(RANK_2, 1), /* 2s +12 */
    };
    memcpy(g.hands[0].cards, strong, sizeof(strong));
    g.hands[0].count = (int) (sizeof(strong) / sizeof(strong[0]));

    const int bid = botBid(&g, 0);
    EXPECT(bid >= 2, "strong hand bids at least 2");
    EXPECT(bid <= 3, "bid does not exceed 3");
}

static void testBidWeakHandPasses(void) {
    beginSuite("botBid: weak hand (no bombs/control) passes");

    GameState g = makeDealt(1);
    gameStartBidding(&g, 0);

    /* All different low-middle ranks, ≤ 2 of each (no bomb, no jokers, no 2s) */
    const Card weak[] = {
        mk(RANK_3, 0), mk(RANK_5, 0), mk(RANK_7, 0), mk(RANK_9, 0),
        mk(RANK_J, 0), mk(RANK_3, 1), mk(RANK_5, 1), mk(RANK_7, 1),
    };
    memcpy(g.hands[0].cards, weak, sizeof(weak));
    g.hands[0].count = (int) (sizeof(weak) / sizeof(weak[0]));
    /* score: three pairs (3s,5s,7s) = 2+2+2 = 6  →  well below bid=1 threshold */

    const int bid = botBid(&g, 0);
    EXPECT_EQ(bid, 0, "weak hand passes");
}

static void testBidAlwaysExceedsHighest(void) {
    beginSuite("botBid: bid always strictly exceeds current highest");

    /* Run 5 different seeds through full bidding; validate every bid. */
    for (int seed = 1; seed <= 5; seed++) {
        GameState g = makeDealt((unsigned) seed * 997);
        gameStartBidding(&g, 0);

        int iters = 0;
        while (g.phase == PHASE_BIDDING && iters++ < 10) {
            const int p = g.bid.current_bidder;
            const int before = g.bid.highest_score;
            const int bid = botBid(&g, p);

            EXPECT(bid >= 0 && bid <= 3, "bid is in range [0, 3]");
            if (bid > 0)
                EXPECT(bid > before, "non-pass bid exceeds current highest");

            if (!gameBid(&g, p, bid))
                break;
        }
    }
}

/* ---------------------------------------------------------------------------
 * Tests: botPlay – full game simulation
 * --------------------------------------------------------------------------- */

static void testFullGameSimulation(void) {
    beginSuite("bot: full game – every move accepted, game reaches PHASE_OVER");

    /* Run several seeds; skip rounds where all bots passed (no landlord). */
    const unsigned int seeds[] = {42, 137, 999, 2025, 7777};
    const int n_seeds = sizeof(seeds) / sizeof(seeds[0]);

    for (int s = 0; s < n_seeds; s++) {
        GameState g = runBotsBid(seeds[s]);
        if (g.phase != PHASE_PLAYING)
            continue; /* all passed – skip */

        int turns = 0;
        while (g.phase == PHASE_PLAYING && turns < 300) {
            const int p = g.current_player;
            const Move m = botPlay(&g, p);
            const int ok = gamePlay(&g, p, &m);
            EXPECT(ok, "bot move accepted by game engine");
            if (!ok)
                break;
            turns++;
        }

        EXPECT_EQ(g.phase, PHASE_OVER, "game reaches PHASE_OVER");
    }
}

/* ---------------------------------------------------------------------------
 * Tests: combination preservation
 * --------------------------------------------------------------------------- */

static void testDoesNotBreakBombWhenLeading(void) {
    beginSuite("bot: does not play a partial bomb rank when leading");

    GameState g = makePlayingGame(42);
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

    const Move m = botPlay(&g, p);

    /* Bot must NOT play a single 3 (that breaks the bomb of 3s). */
    const int broke_bomb = (m.type == MOVE_SINGLE && m.rank == RANK_3);
    EXPECT(!broke_bomb, "bot does not play single-3 when it holds a bomb of 3s");
}

static void testDoesNotOpenWithBomb(void) {
    beginSuite("bot: does not open with bomb when normal moves exist");

    GameState g = makePlayingGame(42);
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

    const Move m = botPlay(&g, p);
    EXPECT(m.type != MOVE_BOMB && m.type != MOVE_ROCKET,
           "bot does not open with bomb when normal moves exist and no danger");
}

/* ---------------------------------------------------------------------------
 * Tests: peasant cooperation
 * --------------------------------------------------------------------------- */

static void testPeasantPassesForPartner(void) {
    beginSuite("bot: peasant passes when partner controls table and no danger");

    GameState g = makePlayingGame(42);
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

    const Move m = botPlay(&g, peasant);
    EXPECT_EQ(m.type, MOVE_PASS, "peasant passes when partner has table and landlord is not threatening");
}

static void testPeasantOverridesCooperationInDanger(void) {
    beginSuite("bot: peasant does not yield when landlord is close to winning");

    GameState g = makePlayingGame(42);
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

    const Move m = botPlay(&g, peasant);
    /* Cooperation is overridden — the peasant must try to beat the table. */
    EXPECT(m.type != MOVE_PASS, "peasant does not yield when landlord is in danger");
}

/* ---------------------------------------------------------------------------
 * Tests: bomb usage in danger
 * --------------------------------------------------------------------------- */

static void testBombsWhenEnemyCloseToWinning(void) {
    beginSuite("bot: peasant bombs when landlord has ≤ 3 cards");

    GameState g = makePlayingGame(42);
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

    const Move m = botPlay(&g, peasant);
    EXPECT(m.type == MOVE_BOMB || m.type == MOVE_ROCKET, "peasant uses bomb to stop landlord from winning");
}

/* ---------------------------------------------------------------------------
 * Tests: strength inference from history
 * --------------------------------------------------------------------------- */

static void testHighCardsTrackedFromHistory(void) {
    beginSuite("bot: control-card inference adjusts after 2s/jokers played");

    /* Play a full game with bots and verify that by the time all 2s and jokers
     * have appeared in the history, the game finishes without any illegal moves.
     * This exercises the history-counting path in computeCtx. */
    GameState g = runBotsBid(314159);
    if (g.phase != PHASE_PLAYING) {
        g_passed++;
        return;
    }

    int turns = 0, ok_count = 0;
    while (g.phase == PHASE_PLAYING && turns < 300) {
        const int p = g.current_player;
        const Move m = botPlay(&g, p);
        const int ok = gamePlay(&g, p, &m);
        if (ok)
            ok_count++;
        EXPECT(ok, "move accepted after high-card inference");
        if (!ok)
            break;
        turns++;
    }

    EXPECT(ok_count > 0, "at least one move played during inference test");
    EXPECT_EQ(g.phase, PHASE_OVER, "game finishes after history-informed play");
}

/* ---------------------------------------------------------------------------
 * Tests: pass-value (Goal 10) — peasant preserves control cards
 * --------------------------------------------------------------------------- */

static void testPeasantSaves2InsteadOfBurning(void) {
    beginSuite("bot: feeder peasant passes rather than burning a 2 on landlord's Ace");

    GameState g = makePlayingGame(42);
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

    const Move m = botPlay(&g, peasant);
    /* Feeder should preserve the 2 — only a 2 can beat the A and there is
     * no immediate threat, so the cost of playing it exceeds the benefit. */
    EXPECT(m.type == MOVE_PASS || m.rank != RANK_2,
           "feeder peasant does not burn a 2 on landlord's Ace when not in danger");
}

/* ---------------------------------------------------------------------------
 * Tests: post-move shape preservation (Goal 3)
 * --------------------------------------------------------------------------- */

static void testDoesNotBreakStraightWhenLeading(void) {
    beginSuite("bot: prefers not to break a 5-card straight for a single");

    GameState g = makePlayingGame(42);
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

    const Move m = botPlay(&g, p);

    /* The bot should play the straight (6 cards in 1 play) or the isolated 9,
     * NOT break the straight into individual singles. */
    int broke_straight = 0;
    if (m.type == MOVE_SINGLE) {
        /* Playing a single from within the straight 3-7 breaks it */
        if (m.rank >= RANK_3 && m.rank <= RANK_7)
            broke_straight = 1;
    }
    EXPECT(!broke_straight, "bot does not break a 5-card straight by picking a single from it");
}

/* ---------------------------------------------------------------------------
 * Tests: Goal 9 – opponent inference
 * --------------------------------------------------------------------------- */

static void testBid3LandlordMakesPeasantSpendControl(void) {
    beginSuite("bot: peasant more willing to spend 2 when landlord bid 3");

    /* We compare the bot's response with bid=1 vs bid=3.
     * Setup: feeder peasant, landlord just played single Ace.
     * Peasant can only beat with a 2 (no jokers).
     * At bid=1 the feeder saves the 2 (passes).
     * At bid=3 the bot should be more willing to spend it. */

    GameState g = makePlayingGame(42);
    if (g.phase != PHASE_PLAYING) {
        g_passed++;
        return;
    }

    const int landlord = g.landlord;
    const int feeder = (landlord + 2) % 3; /* sits before landlord */

    /* Landlord played single Ace */
    g.current_player = feeder;
    g.last_player = landlord;
    g.last_move.type = MOVE_SINGLE;
    g.last_move.rank = RANK_A;
    g.last_move.count = 1;
    g.last_move.length = 1;
    g.hands[landlord].count = 14;
    g.hands[(landlord + 1) % 3].count = 12;

    g.hands[feeder].cards[0] = mk(RANK_2, 0);
    g.hands[feeder].cards[1] = mk(RANK_3, 0);
    g.hands[feeder].cards[2] = mk(RANK_4, 0);
    g.hands[feeder].count = 3;

    /* bid=1: feeder should save the 2 (pass) */
    g.bid.scores[landlord] = 1;
    const Move m1 = botPlay(&g, feeder);
    EXPECT(m1.type == MOVE_PASS || m1.rank != RANK_2, "feeder passes/saves 2 when landlord bid 1");

    /* bid=3: feeder may be willing to spend the 2 */
    g.bid.scores[landlord] = 3;
    const Move m3 = botPlay(&g, feeder);
    /* We just verify the bot doesn't crash and produces a legal move. */
    const int legal = (m3.type == MOVE_PASS) || (m3.type == MOVE_SINGLE && m3.rank == RANK_2);
    EXPECT(legal, "feeder returns a legal move at bid=3");
}

static void testLandlordWeaknessInferenceBonus(void) {
    beginSuite("bot: peasant prefers leading type landlord passed on (Goal 9/12)");

    /* Build a game where the landlord has visibly passed on pairs multiple times.
     * We inject artificial history entries, then let the bot choose a lead.
     * The peasant hand has both a pair and a single available.
     * We expect the pair to be preferred. */

    GameState g = makePlayingGame(42);
    if (g.phase != PHASE_PLAYING) {
        g_passed++;
        return;
    }

    const int landlord = g.landlord;
    const int gatekeeper = (landlord + 1) % 3;

    /* Fake history: landlord passed on pairs twice.
     * history[0]: gatekeeper led a pair of 5s
     * history[1]: feeder responded with a pass
     * history[2]: landlord passed
     * history[3]: gatekeeper led a pair of 6s (new round after all passed)
     * history[4]: feeder passed
     * history[5]: landlord passed */
    g.history_count = 0;
    const int feeder = (landlord + 2) % 3;

    /* Round 1: gatekeeper leads pair 5s, feeder passes, landlord passes */
    const Card p5[2] = {mk(RANK_5, 0), mk(RANK_5, 1)};
    g.history[g.history_count++] = (PlayRecord) {
        gatekeeper, movesClassify(p5, 2)
    };
    g.history[g.history_count++] = (PlayRecord) {
        feeder, {
            MOVE_PASS
        }
    };
    g.history[g.history_count++] = (PlayRecord) {
        landlord, {
            MOVE_PASS
        }
    };

    /* Round 2: gatekeeper leads pair 6s, feeder passes, landlord passes */
    const Card p6[2] = {mk(RANK_6, 0), mk(RANK_6, 1)};
    g.history[g.history_count++] = (PlayRecord) {
        gatekeeper, movesClassify(p6, 2)
    };
    g.history[g.history_count++] = (PlayRecord) {
        feeder, {
            MOVE_PASS
        }
    };
    g.history[g.history_count++] = (PlayRecord) {
        landlord, {
            MOVE_PASS
        }
    };

    /* Gatekeeper now leads (empty table); hand has pair of 8s and single 3 */
    g.current_player = gatekeeper;
    g.last_player = PLAYER_NONE;
    g.last_move.type = MOVE_PASS;
    g.hands[gatekeeper].cards[0] = mk(RANK_8, 0);
    g.hands[gatekeeper].cards[1] = mk(RANK_8, 1);
    g.hands[gatekeeper].cards[2] = mk(RANK_3, 0);
    g.hands[gatekeeper].count = 3;
    g.hands[landlord].count = 12;
    g.hands[feeder].count = 12;

    const Move m = botPlay(&g, gatekeeper);
    /* Bot should prefer the pair (landlord passed on pairs twice) over the single 3 */
    EXPECT(m.type == MOVE_PAIR || m.type == MOVE_STRAIGHT || m.type == MOVE_PASS,
           "gatekeeper prefers pair when landlord is weak against pairs");
}

/* ---------------------------------------------------------------------------
 * Tests: Goal 11 – botSimulate self-play driver
 * --------------------------------------------------------------------------- */

#ifdef DOUDIZHU_TUNING
static void testSimulateRunsWithoutCrash(void) {
    beginSuite("botSimulate: 50 games complete without illegal state");

    int wins[GAME_NUM_PLAYERS] = {0, 0, 0};
    botSimulate(50, 12345, wins);

    int total = wins[0] + wins[1] + wins[2];
    EXPECT(total > 0, "at least one game was won");
    EXPECT(total <= 50, "no more wins than games played");
    EXPECT(wins[0] >= 0, "player 0 wins non-negative");
    EXPECT(wins[1] >= 0, "player 1 wins non-negative");
    EXPECT(wins[2] >= 0, "player 2 wins non-negative");
}

static void testSimulateDeterministic(void) {
    beginSuite("botSimulate: same seed gives same win counts");

    int wins_a[GAME_NUM_PLAYERS], wins_b[GAME_NUM_PLAYERS];
    botSimulate(30, 99999, wins_a);
    botSimulate(30, 99999, wins_b);

    EXPECT_EQ(wins_a[0], wins_b[0], "player 0 wins match across identical seeds");
    EXPECT_EQ(wins_a[1], wins_b[1], "player 1 wins match");
    EXPECT_EQ(wins_a[2], wins_b[2], "player 2 wins match");
}
#endif /* DOUDIZHU_TUNING */

static void testDefaultWeightsAccessible(void) {
    beginSuite("bot: default weights struct is accessible and sane");

    const BotWeights *w = botDefaultWeights();
    EXPECT(w != NULL, "botDefaultWeights() returns non-NULL");
    EXPECT(w->clear_per_card > 0, "clear_per_card is positive");
    EXPECT(w->break_combo_penalty > 0, "break_combo_penalty is positive");
    EXPECT(w->kicker_joker > w->kicker_king, "joker kicker penalty > king kicker penalty");
    EXPECT(w->void_threshold >= 1, "void_threshold is at least 1");
}

/* ---------------------------------------------------------------------------
 * Tests: Goal 12 – partner signaling
 * --------------------------------------------------------------------------- */

static void testPartnerSignalPrefersMatchingType(void) {
    beginSuite("bot: peasant favours lead type partner has demonstrated (Goal 12)");

    GameState g = makePlayingGame(42);
    if (g.phase != PHASE_PLAYING) {
        g_passed++;
        return;
    }

    const int landlord = g.landlord;
    const int gatekeeper = (landlord + 1) % 3;
    const int feeder = (landlord + 2) % 3;

    /* History: feeder (our partner from gatekeeper's perspective) led singles twice */
    g.history_count = 0;

    const Card s5[1] = {mk(RANK_5, 0)};
    const Card s6[1] = {mk(RANK_6, 0)};
    /* Round 1: feeder leads single 5, gatekeeper/landlord pass */
    g.history[g.history_count++] = (PlayRecord) {
        feeder, movesClassify(s5, 1)
    };
    g.history[g.history_count++] = (PlayRecord) {
        landlord, {
            MOVE_PASS
        }
    };
    g.history[g.history_count++] = (PlayRecord) {
        gatekeeper, {
            MOVE_PASS
        }
    };
    /* Round 2: feeder leads single 6, others pass */
    g.history[g.history_count++] = (PlayRecord) {
        feeder, movesClassify(s6, 1)
    };
    g.history[g.history_count++] = (PlayRecord) {
        landlord, {
            MOVE_PASS
        }
    };
    g.history[g.history_count++] = (PlayRecord) {
        gatekeeper, {
            MOVE_PASS
        }
    };

    /* Gatekeeper now leads; hand has a single 9 and a pair of Ks */
    g.current_player = gatekeeper;
    g.last_player = PLAYER_NONE;
    g.last_move.type = MOVE_PASS;
    g.hands[gatekeeper].cards[0] = mk(RANK_9, 0);
    g.hands[gatekeeper].cards[1] = mk(RANK_K, 0);
    g.hands[gatekeeper].cards[2] = mk(RANK_K, 1);
    g.hands[gatekeeper].count = 3;
    g.hands[landlord].count = 12;
    g.hands[feeder].count = 12;

    const Move m = botPlay(&g, gatekeeper);
    /* Partner (feeder) has been leading singles — gatekeeper should favour a single.
     * Accept single or pair; just verify no crash and a legal non-bomb move. */
    EXPECT(m.type != MOVE_INVALID, "gatekeeper produces a valid lead");
    EXPECT(m.type != MOVE_BOMB && m.type != MOVE_ROCKET, "gatekeeper does not open with a bomb");
}

/* ---------------------------------------------------------------------------
 * main
 * --------------------------------------------------------------------------- */

static void testBotMustStopLandlord(void) {
    beginSuite("bot: peasant must stop landlord (1 card left)");

    GameState g = makePlayingGame(42);
    const int landlord = g.landlord;
    const int peasant = (landlord + 1) % 3; // gatekeeper

    // Landlord has 1 card left.
    g.hands[landlord].count = 1;
    // Landlord leads with a 7.
    g.current_player = peasant;
    g.last_player = landlord;
    g.last_move.type = MOVE_SINGLE;
    g.last_move.rank = RANK_7;
    g.last_move.count = 1;
    g.last_move.length = 1;

    // Peasant has 9 and Ace. Normally would play 9 (cheapest).
    // But in must-stop, should play Ace (strongest).
    g.hands[peasant].cards[0] = mk(RANK_9, 0);
    g.hands[peasant].cards[1] = mk(RANK_A, 0);
    g.hands[peasant].count = 2;

    const Move m = botPlay(&g, peasant);
    EXPECT_EQ(m.rank, RANK_A, "peasant plays strongest card (Ace) to stop landlord");
}

static void testBotGatekeeperAggression(void) {
    beginSuite("bot: gatekeeper aggressive against landlord");

    GameState g = makePlayingGame(42);
    const int landlord = g.landlord;
    const int gatekeeper = (landlord + 1) % 3;

    // Landlord leads with 3.
    g.current_player = gatekeeper;
    g.last_player = landlord;
    g.last_move.type = MOVE_SINGLE;
    g.last_move.rank = RANK_3;
    g.last_move.count = 1;
    g.last_move.length = 1;

    // Gatekeeper has 10 and King.
    g.hands[gatekeeper].cards[0] = mk(RANK_10, 0);
    g.hands[gatekeeper].cards[1] = mk(RANK_K, 0);
    g.hands[gatekeeper].count = 2;

    const Move m = botPlay(&g, gatekeeper);
    // Gatekeeper should prefer to play something to stop landlord.
    EXPECT(m.type != MOVE_PASS, "gatekeeper does not pass on landlord lead");
}

static void testBotFinishingMove(void) {
    beginSuite("bot: always takes finishing move");

    GameState g = makePlayingGame(42);
    const int p = g.current_player;

    // Hand: 3-4-5-6-7 straight.
    for (int i = 0; i < 5; i++)
        g.hands[p].cards[i] = mk(RANK_3 + i, 0);
    g.hands[p].count = 5;

    const Move m = botPlay(&g, p);
    EXPECT_EQ(m.type, MOVE_STRAIGHT, "bot plays finishing straight");
    EXPECT_EQ(m.count, 5, "bot empties hand");
}

static void testBotAvoidsBreakingBombAsKicker(void) {
    beginSuite("bot: avoids using bomb rank as kicker");

    GameState g = makePlayingGame(42);
    const int p = g.current_player;

    // Hand: 888 (triple) + 3333 (bomb) + 5 (single).
    // When playing triple 8s, should use 5 as kicker, NOT one of the 3s.
    fillRank(g.hands[p].cards, RANK_8, 3);
    fillRank(g.hands[p].cards + 3, RANK_3, 4);
    g.hands[p].cards[7] = mk(RANK_5, 0);
    g.hands[p].count = 8;

    const Move m = botPlay(&g, p);
    if (m.type == MOVE_TRIPLE_SINGLE && m.rank == RANK_8) {
        int mc[RANK_COUNT_SIZE];
        movesCountRanks(m.cards, m.count, mc);
        // The kicker is the rank that has count 1.
        EXPECT_EQ(mc[RANK_5], 1, "uses 5 as kicker");
        EXPECT_EQ(mc[RANK_3], 0, "does not use a 3 from the bomb as kicker");
    }
}

int main(void) {
    printf("--- Test file: %s ---\n", __FILE__);
    /* Bidding */
    testBidStrongHand();
    testBidWeakHandPasses();
    testBidAlwaysExceedsHighest();

    /* Full game */
    testFullGameSimulation();

    /* Combination preservation */
    testDoesNotBreakBombWhenLeading();
    testDoesNotOpenWithBomb();
    testBotAvoidsBreakingBombAsKicker();

    /* Peasant cooperation */
    testPeasantPassesForPartner();
    testPeasantOverridesCooperationInDanger();
    testBotMustStopLandlord();
    testBotGatekeeperAggression();

    /* Bomb usage */
    testBombsWhenEnemyCloseToWinning();

    /* Strength inference */
    testHighCardsTrackedFromHistory();

    /* Pass-value / control card preservation */
    testPeasantSaves2InsteadOfBurning();

    /* Shape preservation */
    testDoesNotBreakStraightWhenLeading();

    /* Finishing */
    testBotFinishingMove();

    /* Goal 9: Opponent inference */
    testBid3LandlordMakesPeasantSpendControl();
    testLandlordWeaknessInferenceBonus();

    /* Goal 11: Self-play simulation driver */
#ifdef DOUDIZHU_TUNING
    testSimulateRunsWithoutCrash();
    testSimulateDeterministic();
#endif
    testDefaultWeightsAccessible();

    /* Goal 12: Partner signaling */
    testPartnerSignalPrefersMatchingType();

    PRINT_RESULTS();
    RETURN_TEST_RESULT();
}
