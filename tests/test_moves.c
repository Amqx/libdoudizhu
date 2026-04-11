/**
 * @file test_moves.c
 * @brief Tests for the moves module (valid and legal move detection/ generation)
 * @author Peng Yang Deng, Emma Le
 * @date 08-Mar-26
 */

#include <string.h>
#include "moves.h"
#include "test_framework.h"

// ---------------------------------------------------------------------------
// Card construction helpers
// ---------------------------------------------------------------------------

// Make a card from rank and suit (suit 0–3).
static Card card(int rank, int suit) {
    if (rank == RANK_SMALL_JOKER)
        return 52;
    if (rank == RANK_BIG_JOKER)
        return 53;
    return (Card)(rank * 4 + suit);
}

// Make N copies of the same rank (suits 0, 1, 2, 3).
static void fillRank(Card buf[], int rank, int n) {
    for (int i = 0; i < n; i++)
        buf[i] = card(rank, i);
}

// ---------------------------------------------------------------------------
// Tests: movesClassify
// ---------------------------------------------------------------------------

static void testClassifySingle(void) {
    beginSuite("classify: single");
    Card c = card(RANK_7, 0);
    Move m = movesClassify(&c, 1);
    EXPECT_EQ(m.type, MOVE_SINGLE, "type == MOVE_SINGLE");
    EXPECT_EQ(m.rank, RANK_7, "rank == RANK_7");
    EXPECT_EQ(m.count, 1, "count == 1");

    // Jokers are valid singles.
    Card sj = card(RANK_SMALL_JOKER, 0);
    Move msj = movesClassify(&sj, 1);
    EXPECT_EQ(msj.type, MOVE_SINGLE, "small joker single type");
    EXPECT_EQ(msj.rank, RANK_SMALL_JOKER, "small joker rank");

    Card bj = card(RANK_BIG_JOKER, 0);
    Move mbj = movesClassify(&bj, 1);
    EXPECT_EQ(mbj.type, MOVE_SINGLE, "big joker single type");
    EXPECT_EQ(mbj.rank, RANK_BIG_JOKER, "big joker rank");
}

static void testClassifyPair(void) {
    beginSuite("classify: pair");
    Card cs[2];
    fillRank(cs, RANK_K, 2);
    Move m = movesClassify(cs, 2);
    EXPECT_EQ(m.type, MOVE_PAIR, "type == MOVE_PAIR");
    EXPECT_EQ(m.rank, RANK_K, "rank == RANK_K");
    EXPECT_EQ(m.count, 2, "count == 2");
}

static void testClassifyTriple(void) {
    beginSuite("classify: triple");
    Card cs[3];
    fillRank(cs, RANK_A, 3);
    Move m = movesClassify(cs, 3);
    EXPECT_EQ(m.type, MOVE_TRIPLE, "type == MOVE_TRIPLE");
    EXPECT_EQ(m.rank, RANK_A, "rank");
    EXPECT_EQ(m.count, 3, "count");
}

static void testClassifyTripleSingle(void) {
    beginSuite("classify: triple+single");
    Card cs[4];
    fillRank(cs, RANK_9, 3);
    cs[3] = card(RANK_2, 0);
    Move m = movesClassify(cs, 4);
    EXPECT_EQ(m.type, MOVE_TRIPLE_SINGLE, "type");
    EXPECT_EQ(m.rank, RANK_9, "rank == RANK_9");
    EXPECT_EQ(m.count, 4, "count");
}

static void testClassifyTriplePair(void) {
    beginSuite("classify: triple+pair");
    Card cs[5];
    fillRank(cs, RANK_8, 3);
    fillRank(cs + 3, RANK_Q, 2);
    Move m = movesClassify(cs, 5);
    EXPECT_EQ(m.type, MOVE_TRIPLE_PAIR, "type");
    EXPECT_EQ(m.rank, RANK_8, "rank == RANK_8");
    EXPECT_EQ(m.count, 5, "count");
}

static void testClassifyBomb(void) {
    beginSuite("classify: bomb");
    Card cs[4];
    fillRank(cs, RANK_2, 4);
    Move m = movesClassify(cs, 4);
    EXPECT_EQ(m.type, MOVE_BOMB, "type == MOVE_BOMB");
    EXPECT_EQ(m.rank, RANK_2, "rank == RANK_2");
}

static void testClassifyRocket(void) {
    beginSuite("classify: rocket");
    Card cs[2] = {card(RANK_SMALL_JOKER, 0), card(RANK_BIG_JOKER, 0)};
    Move m = movesClassify(cs, 2);
    EXPECT_EQ(m.type, MOVE_ROCKET, "type == MOVE_ROCKET");
}

static void testClassifyStraight(void) {
    beginSuite("classify: straight");
    // 3-4-5-6-7 (ranks 0-4)
    Card cs[5];
    for (int i = 0; i < 5; i++)
        cs[i] = card(RANK_3 + i, 0);
    Move m = movesClassify(cs, 5);
    EXPECT_EQ(m.type, MOVE_STRAIGHT, "type");
    EXPECT_EQ(m.rank, RANK_3, "start rank == RANK_3");
    EXPECT_EQ(m.length, 5, "length == 5");

    // 10-J-Q-K-A (ranks 7-11), length 5
    Card cs2[5];
    for (int i = 0; i < 5; i++)
        cs2[i] = card(RANK_10 + i, 0);
    Move m2 = movesClassify(cs2, 5);
    EXPECT_EQ(m2.type, MOVE_STRAIGHT, "type 10-A");
    EXPECT_EQ(m2.rank, RANK_10, "start rank == RANK_10");
    EXPECT_EQ(m2.length, 5, "length == 5");

    // A straight containing 2 is invalid.
    Card cs3[5];
    for (int i = 0; i < 5; i++)
        cs3[i] = card(RANK_10 + i, 0);
    cs3[4] = card(RANK_2, 0); // replace A with 2
    Move m3 = movesClassify(cs3, 5);
    EXPECT_NE(m3.type, MOVE_STRAIGHT, "straight with 2 is invalid");
}

static void testClassifyPairStraight(void) {
    beginSuite("classify: pair straight");
    // 3344 55 (ranks 0-2, 3 pairs)
    Card cs[6];
    for (int i = 0; i < 3; i++)
        fillRank(cs + i * 2, RANK_3 + i, 2);
    Move m = movesClassify(cs, 6);
    EXPECT_EQ(m.type, MOVE_PAIR_STRAIGHT, "type");
    EXPECT_EQ(m.rank, RANK_3, "start rank");
    EXPECT_EQ(m.length, 3, "length == 3");

    // Only 2 consecutive pairs → invalid (need >= 3).
    Card cs2[4];
    fillRank(cs2, RANK_5, 2);
    fillRank(cs2 + 2, RANK_6, 2);
    Move m2 = movesClassify(cs2, 4);
    EXPECT_NE(m2.type, MOVE_PAIR_STRAIGHT, "2 consecutive pairs invalid");
}

static void testClassifyTripleStraight(void) {
    beginSuite("classify: airplane (triple straight)");
    // 333 444 (ranks 0-1, length 2)
    Card cs[6];
    fillRank(cs, RANK_3, 3);
    fillRank(cs + 3, RANK_4, 3);
    Move m = movesClassify(cs, 6);
    EXPECT_EQ(m.type, MOVE_TRIPLE_STRAIGHT, "type");
    EXPECT_EQ(m.rank, RANK_3, "start rank");
    EXPECT_EQ(m.length, 2, "length == 2");
}

static void testClassifyAirplaneSingles(void) {
    beginSuite("classify: airplane+singles");
    // 333 444 + 5 + 6  (4N = 8, N=2)
    Card cs[8];
    fillRank(cs, RANK_3, 3);
    fillRank(cs + 3, RANK_4, 3);
    cs[6] = card(RANK_5, 0);
    cs[7] = card(RANK_6, 0);
    Move m = movesClassify(cs, 8);
    EXPECT_EQ(m.type, MOVE_TRIPLE_STRAIGHT_SINGLES, "type");
    EXPECT_EQ(m.rank, RANK_3, "plane start rank");
    EXPECT_EQ(m.length, 2, "plane length");
}

static void testClassifyAirplanePairs(void) {
    beginSuite("classify: airplane+pairs");
    // 333 444 + 55 + 66  (5N = 10, N=2)
    Card cs[10];
    fillRank(cs, RANK_3, 3);
    fillRank(cs + 3, RANK_4, 3);
    fillRank(cs + 6, RANK_5, 2);
    fillRank(cs + 8, RANK_6, 2);
    Move m = movesClassify(cs, 10);
    EXPECT_EQ(m.type, MOVE_TRIPLE_STRAIGHT_PAIRS, "type");
    EXPECT_EQ(m.rank, RANK_3, "plane start rank");
    EXPECT_EQ(m.length, 2, "plane length");
}

static void testClassifyFourTwoSingles(void) {
    beginSuite("classify: four+two singles");
    Card cs[6];
    fillRank(cs, RANK_K, 4);
    cs[4] = card(RANK_3, 0);
    cs[5] = card(RANK_A, 0);
    Move m = movesClassify(cs, 6);
    EXPECT_EQ(m.type, MOVE_FOUR_TWO_SINGLES, "type");
    EXPECT_EQ(m.rank, RANK_K, "rank == RANK_K");
}

static void testClassifyFourTwoPairs(void) {
    beginSuite("classify: four+two pairs");
    Card cs[8];
    fillRank(cs, RANK_J, 4);
    fillRank(cs + 4, RANK_3, 2);
    fillRank(cs + 6, RANK_A, 2);
    Move m = movesClassify(cs, 8);
    EXPECT_EQ(m.type, MOVE_FOUR_TWO_PAIRS, "type");
    EXPECT_EQ(m.rank, RANK_J, "rank == RANK_J");
}

static void testClassifyInvalid(void) {
    beginSuite("classify: invalid");
    // Three different ranks.
    Card cs[3] = {card(RANK_3, 0), card(RANK_5, 0), card(RANK_7, 0)};
    Move m = movesClassify(cs, 3);
    EXPECT_EQ(m.type, MOVE_INVALID, "three mixed singles → invalid");
}

// ---------------------------------------------------------------------------
// Tests: movesBeats
// ---------------------------------------------------------------------------

static Move makeSingle(int rank) {
    Move m;
    memset(&m, 0, sizeof(m));
    m.type = MOVE_SINGLE;
    m.rank = rank;
    m.length = 1;
    m.count = 1;
    return m;
}

static Move makePair(int rank) {
    Move m;
    memset(&m, 0, sizeof(m));
    m.type = MOVE_PAIR;
    m.rank = rank;
    m.length = 1;
    m.count = 2;
    return m;
}

static Move makeBomb(int rank) {
    Move m;
    memset(&m, 0, sizeof(m));
    m.type = MOVE_BOMB;
    m.rank = rank;
    m.length = 1;
    m.count = 4;
    return m;
}

static Move makeStraight(int start, int len) {
    Move m;
    memset(&m, 0, sizeof(m));
    m.type = MOVE_STRAIGHT;
    m.rank = start;
    m.length = len;
    m.count = len;
    return m;
}

static Move makePass(void) {
    Move m;
    memset(&m, 0, sizeof(m));
    m.type = MOVE_PASS;
    return m;
}

static Move makeRocket(void) {
    Move m;
    memset(&m, 0, sizeof(m));
    m.type = MOVE_ROCKET;
    m.rank = RANK_BIG_JOKER;
    m.length = 1;
    m.count = 2;
    return m;
}

static void testBeatsBasics(void) {
    beginSuite("beats: basic comparisons");
    Move pass = makePass();
    Move s5 = makeSingle(RANK_5);
    Move s7 = makeSingle(RANK_7);
    Move p5 = makePair(RANK_5);
    Move p8 = makePair(RANK_8);
    Move b3 = makeBomb(RANK_3);
    Move b5 = makeBomb(RANK_5);
    Move rkt = makeRocket();

    // Any non-pass beats an empty table.
    EXPECT(movesBeats(&s5, &pass), "single beats pass");
    EXPECT(movesBeats(&p5, &pass), "pair beats pass");
    EXPECT(movesBeats(&b3, &pass), "bomb beats pass");
    EXPECT(movesBeats(&rkt, &pass), "rocket beats pass");

    // Pass never beats anything.
    EXPECT(!movesBeats(&pass, &s5), "pass never beats single");

    // Higher rank wins.
    EXPECT(movesBeats(&s7, &s5), "7 beats 5");
    EXPECT(!movesBeats(&s5, &s7), "5 does not beat 7");

    // Wrong type.
    EXPECT(!movesBeats(&p5, &s5), "pair does not beat single (wrong type)");
    EXPECT(!movesBeats(&s5, &p5), "single does not beat pair (wrong type)");

    // Higher pair beats lower pair.
    EXPECT(movesBeats(&p8, &p5), "pair 8 beats pair 5");
    EXPECT(!movesBeats(&p5, &p8), "pair 5 does not beat pair 8");

    // Bomb beats non-bomb.
    EXPECT(movesBeats(&b3, &s7), "bomb beats single");
    EXPECT(movesBeats(&b3, &p8), "bomb beats pair");

    // Higher bomb beats lower bomb.
    EXPECT(movesBeats(&b5, &b3), "bomb 5 beats bomb 3");
    EXPECT(!movesBeats(&b3, &b5), "bomb 3 does not beat bomb 5");

    // Rocket beats bomb.
    EXPECT(movesBeats(&rkt, &b5), "rocket beats bomb");

    // Nothing beats rocket.
    EXPECT(!movesBeats(&b5, &rkt), "bomb does not beat rocket");
    EXPECT(!movesBeats(&rkt, &rkt), "rocket does not beat rocket");
}

static void testBeatsStraight(void) {
    beginSuite("beats: straights");
    Move s35 = makeStraight(RANK_3, 5); // 3-4-5-6-7
    Move s45 = makeStraight(RANK_4, 5); // 4-5-6-7-8
    Move s36 = makeStraight(RANK_3, 6); // 3-4-5-6-7-8

    EXPECT(movesBeats(&s45, &s35), "higher start straight beats");
    EXPECT(!movesBeats(&s35, &s45), "lower start does not beat");
    // Different lengths cannot beat each other.
    EXPECT(!movesBeats(&s36, &s35), "length-6 cannot respond to length-5");
}

// ---------------------------------------------------------------------------
// Tests: movesGenerate
// ---------------------------------------------------------------------------

static void testGeneratePass(void) {
    beginSuite("generate: from empty table");
    // Hand: 3♠ 3♥ 5♠ 7♠ 7♥ 7♦
    Card hand[] = {
        card(RANK_3, 0), card(RANK_3, 1), card(RANK_5, 0), card(RANK_7, 0), card(RANK_7, 1), card(RANK_7, 2),
    };
    int hand_size = sizeof(hand) / sizeof(hand[0]);
    Move prev = makePass();
    Move out[256];
    int n = movesGenerate(hand, hand_size, &prev, out, 256);

    EXPECT(n > 0, "at least one move generated");

    // There should be a MOVE_SINGLE for rank 5 somewhere.
    int found_single_5 = 0, found_triple_7 = 0, found_pair_3 = 0;
    for (int i = 0; i < n; i++) {
        if (out[i].type == MOVE_SINGLE && out[i].rank == RANK_5)
            found_single_5 = 1;
        if (out[i].type == MOVE_TRIPLE && out[i].rank == RANK_7)
            found_triple_7 = 1;
        if (out[i].type == MOVE_PAIR && out[i].rank == RANK_3)
            found_pair_3 = 1;
    }
    EXPECT(found_single_5, "single 5 generated");
    EXPECT(found_triple_7, "triple 7 generated");
    EXPECT(found_pair_3, "pair 3 generated");
}

static void testGenerateBeatSingle(void) {
    beginSuite("generate: must beat a single");
    // Hand: 3♠ 7♠ K♠ 2♠
    Card hand[] = {
        card(RANK_3, 0),
        card(RANK_7, 0),
        card(RANK_K, 0),
        card(RANK_2, 0),
    };
    int hand_size = 4;
    Move prev = makeSingle(RANK_5); // on table: single 5
    Move out[256];
    int n = movesGenerate(hand, hand_size, &prev, out, 256);

    // Should only generate singles higher than 5 (7, K, 2).
    int found_3 = 0;
    for (int i = 0; i < n; i++) {
        if (out[i].type == MOVE_SINGLE && out[i].rank == RANK_3)
            found_3 = 1;
        // All generated singles must beat the prev.
        if (out[i].type == MOVE_SINGLE)
            EXPECT(movesBeats(&out[i], &prev), "generated single beats prev");
    }
    EXPECT(!found_3, "single 3 not generated (too low)");
    EXPECT(n >= 3, "at least 3 valid singles (7, K, 2)");
}

static void testGenerateBeatStraight(void) {
    beginSuite("generate: must beat a straight");
    // Hand containing 4-5-6-7-8 and 5-6-7-8-9.
    Card hand[] = {
        card(RANK_4, 0), card(RANK_5, 0), card(RANK_5, 1), card(RANK_6, 0), card(RANK_6, 1),
        card(RANK_7, 0), card(RANK_7, 1), card(RANK_8, 0), card(RANK_8, 1), card(RANK_9, 0),
    };
    int hand_size = 10;
    Move prev = makeStraight(RANK_3, 5); // table: 3-4-5-6-7
    Move out[512];
    int n = movesGenerate(hand, hand_size, &prev, out, 512);

    // 4-5-6-7-8 and 5-6-7-8-9 should both appear.
    int found_4_start = 0, found_5_start = 0;
    for (int i = 0; i < n; i++) {
        if (out[i].type == MOVE_STRAIGHT && out[i].length == 5) {
            if (out[i].rank == RANK_4)
                found_4_start = 1;
            if (out[i].rank == RANK_5)
                found_5_start = 1;
        }
        if (out[i].type == MOVE_STRAIGHT)
            EXPECT(movesBeats(&out[i], &prev), "generated straight beats prev");
    }
    EXPECT(found_4_start, "straight starting at 4 generated");
    EXPECT(found_5_start, "straight starting at 5 generated");
}

static void testGenerateBombAlwaysAvailable(void) {
    beginSuite("generate: bombs generated against any move");
    // Hand has a bomb of 3s.
    Card hand[] = {
        card(RANK_3, 0), card(RANK_3, 1), card(RANK_3, 2), card(RANK_3, 3), card(RANK_7, 0),
    };
    int hand_size = 5;
    Move prev = makePair(RANK_K); // table: pair of Kings
    Move out[256];
    int n = movesGenerate(hand, hand_size, &prev, out, 256);

    int found_bomb = 0;
    for (int i = 0; i < n; i++) {
        if (out[i].type == MOVE_BOMB && out[i].rank == RANK_3)
            found_bomb = 1;
    }
    EXPECT(found_bomb, "bomb of 3s generated against pair of K");
}

static void testGenerateEmptyHand(void) {
    beginSuite("generate: empty hand");
    Move prev = makePass();
    Move out[16];
    int n = movesGenerate(NULL, 0, &prev, out, 16);
    EXPECT_EQ(n, 0, "no moves from empty hand");
}

static void testGenerateRoundTripsThroughClassify(void) {
    beginSuite("generate: round-trip classify");

    Card hand[] = {
        card(RANK_3, 0), card(RANK_3, 1), card(RANK_4, 0), card(RANK_4, 1), card(RANK_5, 0),
        card(RANK_5, 1), card(RANK_5, 2), card(RANK_6, 0), card(RANK_6, 1), card(RANK_6, 2),
        card(RANK_7, 0), card(RANK_7, 1), card(RANK_7, 2), card(RANK_7, 3), card(RANK_8, 0),
        card(RANK_9, 0), card(RANK_Q, 0), card(RANK_Q, 1), card(RANK_SMALL_JOKER, 0), card(RANK_BIG_JOKER, 0),
    };
    Move prev = makePass();
    Move out[512];
    int n = movesGenerate(hand, sizeof(hand) / sizeof(hand[0]), &prev, out, 512);

    EXPECT(n > 0, "generated at least one move");
    for (int i = 0; i < n; i++) {
        Move classified = movesClassify(out[i].cards, out[i].count);
        EXPECT_EQ(classified.type, out[i].type, "generated move type re-classifies identically");
        EXPECT_EQ(classified.count, out[i].count, "generated move count re-classifies identically");
        EXPECT_EQ(classified.rank, out[i].rank, "generated move rank re-classifies identically");
        EXPECT_EQ(classified.length, out[i].length, "generated move length re-classifies identically");
    }
}

static int moveHasExactCards(const Move *move, const Card cards[], int n) {
    if (move->count != n)
        return 0;

    int used[MOVE_MAX_CARDS] = {0};
    for (int i = 0; i < n; i++) {
        int found = 0;
        for (int j = 0; j < n; j++) {
            if (!used[j] && move->cards[j] == cards[i]) {
                used[j] = 1;
                found = 1;
                break;
            }
        }
        if (!found)
            return 0;
    }
    return 1;
}

static void testGeneratePreservesActualCardIdentities(void) {
    beginSuite("generate: preserves source card suits");

    Card hand[] = {
        card(RANK_K, 1), card(RANK_K, 3), card(RANK_5, 2),
    };
    const Card expectedPair[] = {
        card(RANK_K, 1), card(RANK_K, 3),
    };

    Move prev = makePass();
    Move out[64];
    int n = movesGenerate(hand, sizeof(hand) / sizeof(hand[0]), &prev, out, 64);

    int foundExactPair = 0;
    for (int i = 0; i < n; i++) {
        if (out[i].type == MOVE_PAIR && out[i].rank == RANK_K &&
            moveHasExactCards(&out[i], expectedPair, 2)) {
            foundExactPair = 1;
            break;
        }
    }

    EXPECT(foundExactPair,
           "generated pair should keep the exact suit identities from the hand");
}

// ---------------------------------------------------------------------------
// Tests: movesCountRanks
// ---------------------------------------------------------------------------

static void testCountRanks(void) {
    beginSuite("count_ranks");
    Card hand[] = {
        card(RANK_3, 0), card(RANK_3, 1), card(RANK_3, 2), card(RANK_7, 0), card(RANK_BIG_JOKER, 0),
    };
    int cnt[RANK_COUNT_SIZE];
    movesCountRanks(hand, 5, cnt);
    EXPECT_EQ(cnt[RANK_3], 3, "three 3s");
    EXPECT_EQ(cnt[RANK_7], 1, "one 7");
    EXPECT_EQ(cnt[RANK_BIG_JOKER], 1, "one big joker");
    EXPECT_EQ(cnt[RANK_5], 0, "no 5s");
}

// ---------------------------------------------------------------------------
// Tests: movesTypeName
// ---------------------------------------------------------------------------

static void testTypeNames(void) {
    beginSuite("type_names");
    EXPECT(movesTypeName(MOVE_PASS) != NULL, "PASS has name");
    EXPECT(movesTypeName(MOVE_BOMB) != NULL, "BOMB has name");
    EXPECT(movesTypeName(MOVE_ROCKET) != NULL, "ROCKET has name");
    EXPECT(movesTypeName(MOVE_INVALID) != NULL, "INVALID has name");
}

static void testClassifyLongStraight(void) {
    beginSuite("classify: long straight");
    // 3-4-5-6-7-8-9-10-J-Q-K-A (12 cards)
    Card cs[12];
    for (int i = 0; i < 12; i++)
        cs[i] = card(RANK_3 + i, 0);
    Move m = movesClassify(cs, 12);
    EXPECT_EQ(m.type, MOVE_STRAIGHT, "type == MOVE_STRAIGHT");
    EXPECT_EQ(m.length, 12, "length == 12");
}

static void testClassifyAirplaneComplex(void) {
    beginSuite("classify: complex airplane");
    // 333 444 555 + 7 + 8 + 9 (3 sets + 3 kickers)
    Card cs[12];
    fillRank(cs, RANK_3, 3);
    fillRank(cs + 3, RANK_4, 3);
    fillRank(cs + 6, RANK_5, 3);
    cs[9] = card(RANK_7, 0);
    cs[10] = card(RANK_8, 0);
    cs[11] = card(RANK_9, 0);
    Move m = movesClassify(cs, 12);
    EXPECT_EQ(m.type, MOVE_TRIPLE_STRAIGHT_SINGLES, "type");
    EXPECT_EQ(m.length, 3, "length == 3");
    EXPECT_EQ(m.rank, RANK_3, "rank == RANK_3");
}

static void testClassifyFourTwoMixed(void) {
    beginSuite("classify: four-two mixed kickers");
    // Four 10s + J + Q (should be MOVE_FOUR_TWO_SINGLES)
    Card cs[6];
    fillRank(cs, RANK_10, 4);
    cs[4] = card(RANK_J, 0);
    cs[5] = card(RANK_Q, 0);
    Move m = movesClassify(cs, 6);
    EXPECT_EQ(m.type, MOVE_FOUR_TWO_SINGLES, "type");

    // Four 10s + JJ + QQ (should be MOVE_FOUR_TWO_PAIRS)
    Card cs2[8];
    fillRank(cs2, RANK_10, 4);
    fillRank(cs2 + 4, RANK_J, 2);
    fillRank(cs2 + 6, RANK_Q, 2);
    Move m2 = movesClassify(cs2, 8);
    EXPECT_EQ(m2.type, MOVE_FOUR_TWO_PAIRS, "type");
}

static void testClassifyCountsDirect(void) {
    beginSuite("movesClassifyCounts");
    int cnt[RANK_COUNT_SIZE] = {0};
    cnt[RANK_5] = 3;
    cnt[RANK_3] = 1;
    Move m = movesClassifyCounts(cnt, 4);
    EXPECT_EQ(m.type, MOVE_TRIPLE_SINGLE, "3 of 5s + 1 of 3s");
    EXPECT_EQ(m.rank, RANK_5, "rank is 5");
}

static void testSortCards(void) {
    beginSuite("movesSort");
    Move m;
    memset(&m, 0, sizeof(m));
    m.count = 5;
    m.cards[0] = card(RANK_A, 0);
    m.cards[1] = card(RANK_3, 1);
    m.cards[2] = card(RANK_J, 2);
    m.cards[3] = card(RANK_2, 3);
    m.cards[4] = card(RANK_5, 0);

    movesSort(&m);

    EXPECT_EQ(CARD_RANK(m.cards[0]), RANK_3, "index 0 is RANK_3");
    EXPECT_EQ(CARD_RANK(m.cards[1]), RANK_5, "index 1 is RANK_5");
    EXPECT_EQ(CARD_RANK(m.cards[2]), RANK_J, "index 2 is RANK_J");
    EXPECT_EQ(CARD_RANK(m.cards[3]), RANK_A, "index 3 is RANK_A");
    EXPECT_EQ(CARD_RANK(m.cards[4]), RANK_2, "index 4 is RANK_2");
}

static void testBeatsBombVsBomb(void) {
    beginSuite("beats: bomb vs bomb");
    Move b3 = makeBomb(RANK_3);
    Move b4 = makeBomb(RANK_4);
    EXPECT_EQ(movesBeats(&b4, &b3), 1, "bomb 4 beats bomb 3");
    EXPECT_EQ(movesBeats(&b3, &b4), 0, "bomb 3 does not beat bomb 4");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(void) {
    printf("--- Test file: %s ---\n", __FILE__);
    // classify
    testClassifySingle();
    testClassifyPair();
    testClassifyTriple();
    testClassifyTripleSingle();
    testClassifyTriplePair();
    testClassifyBomb();
    testClassifyRocket();
    testClassifyStraight();
    testClassifyLongStraight();
    testClassifyPairStraight();
    testClassifyTripleStraight();
    testClassifyAirplaneSingles();
    testClassifyAirplanePairs();
    testClassifyAirplaneComplex();
    testClassifyFourTwoSingles();
    testClassifyFourTwoPairs();
    testClassifyFourTwoMixed();
    testClassifyInvalid();
    testClassifyCountsDirect();

    // beats
    testBeatsBasics();
    testBeatsStraight();
    testBeatsBombVsBomb();

    // generate
    testGeneratePass();
    testGenerateBeatSingle();
    testGenerateBeatStraight();
    testGenerateBombAlwaysAvailable();
    testGenerateEmptyHand();
    testGenerateRoundTripsThroughClassify();
    testGeneratePreservesActualCardIdentities();

    // utilities
    testCountRanks();
    testTypeNames();
    testSortCards();

    PRINT_RESULTS();
    RETURN_TEST_RESULT();
}
