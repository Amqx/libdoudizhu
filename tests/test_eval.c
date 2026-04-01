/**
 * @file test_eval.c
 * @brief Tests for the eval module (hand evaluation utilities)
 * @author Peng Yang Deng (), Emma Le ()
 * @date 08-Mar-26
 */

#include <string.h>
#include "eval.h"
#include "test_framework.h"

/* ---------------------------------------------------------------------------
 * Helpers
 * --------------------------------------------------------------------------- */

static Card card(int rank, int suit) {
    if (rank == RANK_SMALL_JOKER)
        return 52;
    if (rank == RANK_BIG_JOKER)
        return 53;
    return (Card)(rank * 4 + suit);
}

static void fillRank(Card buf[], int rank, int n) {
    for (int i = 0; i < n; i++)
        buf[i] = card(rank, i);
}

/* ---------------------------------------------------------------------------
 * Tests: evalHandScore
 * --------------------------------------------------------------------------- */

static void testScoreStrongBeatsWeak(void) {
    beginSuite("evalHandScore: strong hand scores higher than weak");

    /* Weak hand: low singles only */
    Card weak[] = {
        card(RANK_3, 0), card(RANK_4, 0), card(RANK_5, 0), card(RANK_6, 0), card(RANK_7, 0),
    };

    /* Strong hand: bomb + rocket + 2 */
    Card strong[6];
    fillRank(strong, RANK_K, 4);
    strong[4] = card(RANK_SMALL_JOKER, 0);
    strong[5] = card(RANK_BIG_JOKER, 0);

    int ws = evalHandScore(weak, 5);
    int ss = evalHandScore(strong, 6);
    EXPECT(ss > ws, "strong hand scores higher than weak hand");
}

static void testScoreRocketAddsToScore(void) {
    beginSuite("evalHandScore: rocket is valued");

    Card no_rocket[] = {card(RANK_3, 0), card(RANK_4, 0)};
    Card has_rocket[] = {card(RANK_SMALL_JOKER, 0), card(RANK_BIG_JOKER, 0)};

    int s1 = evalHandScore(no_rocket, 2);
    int s2 = evalHandScore(has_rocket, 2);
    EXPECT(s2 > s1, "hand with rocket scores higher");
}

static void testScoreBombAddsToScore(void) {
    beginSuite("evalHandScore: each bomb raises the score");

    Card no_bomb[4] = {card(RANK_3, 0), card(RANK_4, 0), card(RANK_5, 0), card(RANK_6, 0)};
    Card has_bomb[4];
    fillRank(has_bomb, RANK_A, 4);

    int s1 = evalHandScore(no_bomb, 4);
    int s2 = evalHandScore(has_bomb, 4);
    EXPECT(s2 > s1, "hand with bomb scores higher");
}

static void testScoreStraightPotentialBonus(void) {
    beginSuite("evalHandScore: 5-card straight potential adds bonus");

    /* Five consecutive ranks: 3-4-5-6-7 */
    Card straight[5];
    for (int i = 0; i < 5; i++)
        straight[i] = card(RANK_3 + i, 0);

    /* Five non-consecutive ranks of similar values */
    Card scattered[5] = {
        card(RANK_3, 0), card(RANK_5, 0), card(RANK_7, 0), card(RANK_9, 0), card(RANK_J, 0),
    };

    int s_run = evalHandScore(straight, 5);
    int s_scat = evalHandScore(scattered, 5);
    EXPECT(s_run > s_scat, "consecutive sequence scores higher than scattered");
}

static void testScoreEmptyHand(void) {
    beginSuite("evalHandScore: empty hand scores zero");
    int s = evalHandScore(NULL, 0);
    EXPECT_EQ(s, 0, "empty hand → score 0");
}

/* ---------------------------------------------------------------------------
 * Tests: evalCountBombs
 * --------------------------------------------------------------------------- */

static void testCountBombsNone(void) {
    beginSuite("evalCountBombs: hand with no bombs");
    Card hand[] = {card(RANK_3, 0), card(RANK_5, 0), card(RANK_7, 0)};
    EXPECT_EQ(evalCountBombs(hand, 3), 0, "no bombs counted");
}

static void testCountBombsOneBomb(void) {
    beginSuite("evalCountBombs: one four-of-a-kind");
    Card hand[4];
    fillRank(hand, RANK_9, 4);
    EXPECT_EQ(evalCountBombs(hand, 4), 1, "one bomb counted");
}

static void testCountBombsRocketCounts(void) {
    beginSuite("evalCountBombs: rocket counts as one bomb");
    Card hand[2] = {card(RANK_SMALL_JOKER, 0), card(RANK_BIG_JOKER, 0)};
    EXPECT_EQ(evalCountBombs(hand, 2), 1, "rocket counts as bomb");
}

static void testCountBombsMultiple(void) {
    beginSuite("evalCountBombs: two bombs + rocket = 3");
    Card hand[10];
    fillRank(hand, RANK_3, 4);
    fillRank(hand + 4, RANK_7, 4);
    hand[8] = card(RANK_SMALL_JOKER, 0);
    hand[9] = card(RANK_BIG_JOKER, 0);
    EXPECT_EQ(evalCountBombs(hand, 10), 3, "two bombs + rocket = 3");
}

/* ---------------------------------------------------------------------------
 * Tests: evalMoveCost
 * --------------------------------------------------------------------------- */

static void testCostOrdering(void) {
    beginSuite("evalMoveCost: ordering (single < pair < triple < bomb < rocket)");

    Move single = {MOVE_SINGLE, {0}, 1, RANK_3, 1};
    Move pair = {MOVE_PAIR, {0}, 2, RANK_3, 1};
    Move triple = {MOVE_TRIPLE, {0}, 3, RANK_3, 1};
    Move bomb = {MOVE_BOMB, {0}, 4, RANK_3, 1};
    Move rocket = {MOVE_ROCKET, {0}, 2, RANK_BIG_JOKER, 1};

    EXPECT(evalMoveCost(&single) < evalMoveCost(&pair), "single < pair");
    EXPECT(evalMoveCost(&pair) < evalMoveCost(&triple), "pair < triple");
    EXPECT(evalMoveCost(&triple) < evalMoveCost(&bomb), "triple < bomb");
    EXPECT(evalMoveCost(&bomb) < evalMoveCost(&rocket), "bomb < rocket");
}

static void testCostHigherRankCostsMore(void) {
    beginSuite("evalMoveCost: higher rank = higher cost within same type");

    Move s_low = {MOVE_SINGLE, {0}, 1, RANK_3, 1};
    Move s_high = {MOVE_SINGLE, {0}, 1, RANK_2, 1};
    Move p_low = {MOVE_PAIR, {0}, 2, RANK_4, 1};
    Move p_high = {MOVE_PAIR, {0}, 2, RANK_K, 1};

    EXPECT(evalMoveCost(&s_low) < evalMoveCost(&s_high), "single 3 < single 2");
    EXPECT(evalMoveCost(&p_low) < evalMoveCost(&p_high), "pair 4 < pair K");
}

static void testCostPassAndInvalid(void) {
    beginSuite("evalMoveCost: PASS and INVALID return 0");
    Move pass = {MOVE_PASS, {0}, 0, 0, 0};
    Move invalid = {MOVE_INVALID, {0}, 0, 0, 0};
    EXPECT_EQ(evalMoveCost(&pass), 0, "PASS cost == 0");
    EXPECT_EQ(evalMoveCost(&invalid), 0, "INVALID cost == 0");
}

/* ---------------------------------------------------------------------------
 * Tests: evalMinPlays
 * --------------------------------------------------------------------------- */

static void testMinPlaysRocketIsOne(void) {
    beginSuite("evalMinPlays: rocket counts as 1 play");
    Card hand[2] = {card(RANK_SMALL_JOKER, 0), card(RANK_BIG_JOKER, 0)};
    EXPECT_EQ(evalMinPlays(hand, 2), 1, "rocket = 1 play");
}

static void testMinPlaysBombIsOne(void) {
    beginSuite("evalMinPlays: four-of-a-kind counts as 1 play");
    Card hand[4];
    fillRank(hand, RANK_7, 4);
    EXPECT_EQ(evalMinPlays(hand, 4), 1, "bomb = 1 play");
}

static void testMinPlaysStraightIsOne(void) {
    beginSuite("evalMinPlays: 5-card straight counts as 1 play");
    Card hand[5];
    for (int i = 0; i < 5; i++)
        hand[i] = card(RANK_3 + i, 0);
    EXPECT_EQ(evalMinPlays(hand, 5), 1, "5-card straight = 1 play");
}

static void testMinPlaysTripleWithKicker(void) {
    beginSuite("evalMinPlays: triple+single consumed as 1 play");
    /* Three 5s + one isolated 3 → triple can absorb 3 as kicker = 1 play */
    Card hand[4] = {card(RANK_5, 0), card(RANK_5, 1), card(RANK_5, 2), card(RANK_3, 0)};
    EXPECT_EQ(evalMinPlays(hand, 4), 1, "triple+kicker = 1 play");
}

static void testMinPlaysScatteredWorseThanChain(void) {
    beginSuite("evalMinPlays: scattered hand needs more plays than chain");
    /* Chain: 3-4-5-6-7-8 straight + 9-10-J pair straight = potentially 2 plays */
    Card chain[8];
    for (int i = 0; i < 6; i++)
        chain[i] = card(RANK_3 + i, 0); /* 3-8 straight */
    chain[6] = card(RANK_9, 0);
    chain[7] = card(RANK_9, 1); /* pair 9s */

    /* Scattered: 8 different isolated ranks, no combos possible */
    Card scattered[8] = {
        card(RANK_3, 0), card(RANK_5, 0), card(RANK_7, 0), card(RANK_9, 0),
        card(RANK_J, 0), card(RANK_2, 0), card(RANK_A, 0), card(RANK_K, 0),
    };

    const int p_chain = evalMinPlays(chain, 8);
    const int p_scat = evalMinPlays(scattered, 8);
    EXPECT(p_chain < p_scat, "chain hand clears in fewer plays than scattered");
}

/* ---------------------------------------------------------------------------
 * Tests: evalBidStrength
 * --------------------------------------------------------------------------- */

static void testBidStrengthRocketHighest(void) {
    beginSuite("evalBidStrength: rocket+bomb hand outscores plain singles");
    Card strong[6];
    fillRank(strong, RANK_A, 4);
    strong[4] = card(RANK_SMALL_JOKER, 0);
    strong[5] = card(RANK_BIG_JOKER, 0);

    Card weak[6];
    for (int i = 0; i < 6; i++)
        weak[i] = card(RANK_3 + i, 0);

    EXPECT(evalBidStrength(strong, 6) > evalBidStrength(weak, 6), "rocket+bomb scores higher than singles for bidding");
}

static void testBidStrengthTwosValued(void) {
    beginSuite("evalBidStrength: 2s contribute significantly");
    Card with_twos[4] = {card(RANK_2, 0), card(RANK_2, 1), card(RANK_2, 2), card(RANK_2, 3)};
    Card without_twos[4] = {card(RANK_3, 0), card(RANK_4, 0), card(RANK_5, 0), card(RANK_6, 0)};
    EXPECT(evalBidStrength(with_twos, 4) > evalBidStrength(without_twos, 4),
           "hand with four 2s bids stronger than low singles");
}

/* ---------------------------------------------------------------------------
 * Tests: evalPlayPosition
 * --------------------------------------------------------------------------- */

static void testPlayPositionFewerPlaysBetter(void) {
    beginSuite("evalPlayPosition: fewer required plays = better position");
    /* Straight: 1 play */
    Card straight[5];
    for (int i = 0; i < 5; i++)
        straight[i] = card(RANK_3 + i, 0);

    /* Scattered singles: 5 plays */
    Card singles[5] = {
        card(RANK_3, 0), card(RANK_5, 0), card(RANK_7, 0), card(RANK_9, 0), card(RANK_J, 0),
    };

    EXPECT(evalPlayPosition(straight, 5) > evalPlayPosition(singles, 5),
           "playable straight has better position than scattered singles");
}

static void testPlayPositionControlCardsBoost(void) {
    beginSuite("evalPlayPosition: control cards improve score");
    Card base[3] = {card(RANK_3, 0), card(RANK_4, 0), card(RANK_5, 0)};
    Card control[3] = {card(RANK_2, 0), card(RANK_A, 0), card(RANK_K, 0)};
    EXPECT(evalPlayPosition(control, 3) > evalPlayPosition(base, 3),
           "hand with 2/A/K has better play position than 3/4/5");
}

/* ---------------------------------------------------------------------------
 * Tests: evalMinPlaysCounts / evalPlayPositionCounts
 * --------------------------------------------------------------------------- */

static void testMinPlaysCountsMatchesCardVariant(void) {
    beginSuite("evalMinPlaysCounts: agrees with evalMinPlays");
    Card hand[6];
    for (int i = 0; i < 5; i++)
        hand[i] = card(RANK_3 + i, 0); /* 3-7 straight */
    hand[5] = card(RANK_9, 0);

    int cnt[RANK_COUNT_SIZE] = {0};
    for (int i = 0; i < 6; i++) {
        const int r = CARD_RANK(hand[i]);
        cnt[r]++;
    }

    int mp_cards = evalMinPlays(hand, 6);
    int mp_counts = evalMinPlaysCounts(cnt);
    EXPECT_EQ(mp_cards, mp_counts, "count-array variant matches card-array variant");
}

static void testPlayPositionCountsMatchesCardVariant(void) {
    beginSuite("evalPlayPositionCounts: agrees with evalPlayPosition");
    Card hand[4];
    fillRank(hand, RANK_A, 4); /* bomb of Aces */

    int cnt[RANK_COUNT_SIZE] = {0};
    cnt[RANK_A] = 4;

    const int pos_cards = evalPlayPosition(hand, 4);
    const int pos_counts = evalPlayPositionCounts(cnt, 4);
    EXPECT_EQ(pos_cards, pos_counts, "count-array variant matches card-array variant");
}

/* ---------------------------------------------------------------------------
 * main
 * --------------------------------------------------------------------------- */

static void testMinPlaysComplexAirplane(void) {
    beginSuite("evalMinPlays: airplane with kickers");
    // 333 444 + 5 + 6 (2 sets + 2 kickers = 1 play)
    Card hand[8];
    fillRank(hand, RANK_3, 3);
    fillRank(hand + 3, RANK_4, 3);
    hand[6] = card(RANK_5, 0);
    hand[7] = card(RANK_6, 0);
    EXPECT_EQ(evalMinPlays(hand, 8), 1, "airplane with 2 kickers = 1 play");
}

static void testMinPlaysNestedStraights(void) {
    beginSuite("evalMinPlays: nested/overlapping straights");
    // 3-4-5-6-7-8-9 (7-card straight)
    Card hand[7];
    for (int i = 0; i < 7; i++)
        hand[i] = card(RANK_3 + i, 0);
    EXPECT_EQ(evalMinPlays(hand, 7), 1, "7-card straight = 1 play");
}

static void testBidStrengthVsHandScore(void) {
    beginSuite("evalBidStrength: different from evalHandScore");
    Card hand[4];
    fillRank(hand, RANK_2, 4); // bomb of 2s
    int s1 = evalHandScore(hand, 4);
    int s2 = evalBidStrength(hand, 4);
    // Both should be high, but they are calculated differently.
    EXPECT(s1 > 0 && s2 > 0, "both give positive scores for bomb of 2s");
}

static void testPlayPositionExtreme(void) {
    beginSuite("evalPlayPosition: best and worst cases");
    Card best[2] = {card(RANK_SMALL_JOKER, 0), card(RANK_BIG_JOKER, 0)};
    Card worst[1] = {card(RANK_3, 0)};

    int p_best = evalPlayPosition(best, 2);
    int p_worst = evalPlayPosition(worst, 1);
    EXPECT(p_best > p_worst, "rocket has better play position than single 3");
}

static void testMinPlaysAirplaneNoKickers(void) {
    beginSuite("evalMinPlays: airplane without kickers");
    Card hand[6];
    fillRank(hand, RANK_5, 3);
    fillRank(hand + 3, RANK_6, 3);
    EXPECT_EQ(evalMinPlays(hand, 6), 1, "333 444 = 1 play");
}

int main(void) {
    printf("--- Test file: %s ---\n", __FILE__);
    testScoreStrongBeatsWeak();
    testScoreRocketAddsToScore();
    testScoreBombAddsToScore();
    testScoreStraightPotentialBonus();
    testScoreEmptyHand();

    testCountBombsNone();
    testCountBombsOneBomb();
    testCountBombsRocketCounts();
    testCountBombsMultiple();

    testCostOrdering();
    testCostHigherRankCostsMore();
    testCostPassAndInvalid();

    testMinPlaysRocketIsOne();
    testMinPlaysBombIsOne();
    testMinPlaysStraightIsOne();
    testMinPlaysTripleWithKicker();
    testMinPlaysComplexAirplane();
    testMinPlaysAirplaneNoKickers();
    testMinPlaysNestedStraights();
    testMinPlaysScatteredWorseThanChain();

    testBidStrengthRocketHighest();
    testBidStrengthTwosValued();
    testBidStrengthVsHandScore();

    testPlayPositionFewerPlaysBetter();
    testPlayPositionControlCardsBoost();
    testPlayPositionExtreme();

    testMinPlaysCountsMatchesCardVariant();
    testPlayPositionCountsMatchesCardVariant();

    PRINT_RESULTS();
    RETURN_TEST_RESULT();
}
