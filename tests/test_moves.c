//
// Created by Jonathan on 08-Mar-26.
//

#include "moves.h"
#include "test_framework.h"

#include <string.h>

// ---------------------------------------------------------------------------
// Card construction helpers
// ---------------------------------------------------------------------------

// Make a card from rank and suit (suit 0–3).
static Card card(int rank, int suit) {
    if (rank == RANK_SMALL_JOKER) return 52;
    if (rank == RANK_BIG_JOKER) return 53;
    return (Card)(rank * 4 + suit);
}

// Make N copies of the same rank (suits 0, 1, 2, 3).
static void fill_rank(Card *buf, int rank, int n) {
    for (int i = 0; i < n; i++) buf[i] = card(rank, i);
}

// ---------------------------------------------------------------------------
// Tests: moves_classify
// ---------------------------------------------------------------------------

static void test_classify_single(void) {
    begin_suite("classify: single");
    Card c = card(RANK_7, 0);
    Move m = moves_classify(&c, 1);
    EXPECT_EQ(m.type, MOVE_SINGLE, "type == MOVE_SINGLE");
    EXPECT_EQ(m.rank, RANK_7, "rank == RANK_7");
    EXPECT_EQ(m.count, 1, "count == 1");

    // Jokers are valid singles.
    Card sj = card(RANK_SMALL_JOKER, 0);
    Move msj = moves_classify(&sj, 1);
    EXPECT_EQ(msj.type, MOVE_SINGLE, "small joker single type");
    EXPECT_EQ(msj.rank, RANK_SMALL_JOKER, "small joker rank");

    Card bj = card(RANK_BIG_JOKER, 0);
    Move mbj = moves_classify(&bj, 1);
    EXPECT_EQ(mbj.type, MOVE_SINGLE, "big joker single type");
    EXPECT_EQ(mbj.rank, RANK_BIG_JOKER, "big joker rank");
}

static void test_classify_pair(void) {
    begin_suite("classify: pair");
    Card cs[2];
    fill_rank(cs, RANK_K, 2);
    Move m = moves_classify(cs, 2);
    EXPECT_EQ(m.type, MOVE_PAIR, "type == MOVE_PAIR");
    EXPECT_EQ(m.rank, RANK_K, "rank == RANK_K");
    EXPECT_EQ(m.count, 2, "count == 2");
}

static void test_classify_triple(void) {
    begin_suite("classify: triple");
    Card cs[3];
    fill_rank(cs, RANK_A, 3);
    Move m = moves_classify(cs, 3);
    EXPECT_EQ(m.type, MOVE_TRIPLE, "type == MOVE_TRIPLE");
    EXPECT_EQ(m.rank, RANK_A, "rank");
    EXPECT_EQ(m.count, 3, "count");
}

static void test_classify_triple_single(void) {
    begin_suite("classify: triple+single");
    Card cs[4];
    fill_rank(cs, RANK_9, 3);
    cs[3] = card(RANK_2, 0);
    Move m = moves_classify(cs, 4);
    EXPECT_EQ(m.type, MOVE_TRIPLE_SINGLE, "type");
    EXPECT_EQ(m.rank, RANK_9, "rank == RANK_9");
    EXPECT_EQ(m.count, 4, "count");
}

static void test_classify_triple_pair(void) {
    begin_suite("classify: triple+pair");
    Card cs[5];
    fill_rank(cs, RANK_8, 3);
    fill_rank(cs + 3, RANK_Q, 2);
    Move m = moves_classify(cs, 5);
    EXPECT_EQ(m.type, MOVE_TRIPLE_PAIR, "type");
    EXPECT_EQ(m.rank, RANK_8, "rank == RANK_8");
    EXPECT_EQ(m.count, 5, "count");
}

static void test_classify_bomb(void) {
    begin_suite("classify: bomb");
    Card cs[4];
    fill_rank(cs, RANK_2, 4);
    Move m = moves_classify(cs, 4);
    EXPECT_EQ(m.type, MOVE_BOMB, "type == MOVE_BOMB");
    EXPECT_EQ(m.rank, RANK_2, "rank == RANK_2");
}

static void test_classify_rocket(void) {
    begin_suite("classify: rocket");
    Card cs[2] = {card(RANK_SMALL_JOKER, 0), card(RANK_BIG_JOKER, 0)};
    Move m = moves_classify(cs, 2);
    EXPECT_EQ(m.type, MOVE_ROCKET, "type == MOVE_ROCKET");
}

static void test_classify_straight(void) {
    begin_suite("classify: straight");
    // 3-4-5-6-7 (ranks 0-4)
    Card cs[5];
    for (int i = 0; i < 5; i++) cs[i] = card(RANK_3 + i, 0);
    Move m = moves_classify(cs, 5);
    EXPECT_EQ(m.type, MOVE_STRAIGHT, "type");
    EXPECT_EQ(m.rank, RANK_3, "start rank == RANK_3");
    EXPECT_EQ(m.length, 5, "length == 5");

    // 10-J-Q-K-A (ranks 7-11), length 5
    Card cs2[5];
    for (int i = 0; i < 5; i++) cs2[i] = card(RANK_10 + i, 0);
    Move m2 = moves_classify(cs2, 5);
    EXPECT_EQ(m2.type, MOVE_STRAIGHT, "type 10-A");
    EXPECT_EQ(m2.rank, RANK_10, "start rank == RANK_10");
    EXPECT_EQ(m2.length, 5, "length == 5");

    // A straight containing 2 is invalid.
    Card cs3[5];
    for (int i = 0; i < 5; i++) cs3[i] = card(RANK_10 + i, 0);
    cs3[4] = card(RANK_2, 0); // replace A with 2
    Move m3 = moves_classify(cs3, 5);
    EXPECT_NE(m3.type, MOVE_STRAIGHT, "straight with 2 is invalid");
}

static void test_classify_pair_straight(void) {
    begin_suite("classify: pair straight");
    // 3344 55 (ranks 0-2, 3 pairs)
    Card cs[6];
    for (int i = 0; i < 3; i++) fill_rank(cs + i * 2, RANK_3 + i, 2);
    Move m = moves_classify(cs, 6);
    EXPECT_EQ(m.type, MOVE_PAIR_STRAIGHT, "type");
    EXPECT_EQ(m.rank, RANK_3, "start rank");
    EXPECT_EQ(m.length, 3, "length == 3");

    // Only 2 consecutive pairs → invalid (need >= 3).
    Card cs2[4];
    fill_rank(cs2, RANK_5, 2);
    fill_rank(cs2 + 2, RANK_6, 2);
    Move m2 = moves_classify(cs2, 4);
    EXPECT_NE(m2.type, MOVE_PAIR_STRAIGHT, "2 consecutive pairs invalid");
}

static void test_classify_triple_straight(void) {
    begin_suite("classify: airplane (triple straight)");
    // 333 444 (ranks 0-1, length 2)
    Card cs[6];
    fill_rank(cs, RANK_3, 3);
    fill_rank(cs + 3, RANK_4, 3);
    Move m = moves_classify(cs, 6);
    EXPECT_EQ(m.type, MOVE_TRIPLE_STRAIGHT, "type");
    EXPECT_EQ(m.rank, RANK_3, "start rank");
    EXPECT_EQ(m.length, 2, "length == 2");
}

static void test_classify_airplane_singles(void) {
    begin_suite("classify: airplane+singles");
    // 333 444 + 5 + 6  (4N = 8, N=2)
    Card cs[8];
    fill_rank(cs, RANK_3, 3);
    fill_rank(cs + 3, RANK_4, 3);
    cs[6] = card(RANK_5, 0);
    cs[7] = card(RANK_6, 0);
    Move m = moves_classify(cs, 8);
    EXPECT_EQ(m.type, MOVE_TRIPLE_STRAIGHT_SINGLES, "type");
    EXPECT_EQ(m.rank, RANK_3, "plane start rank");
    EXPECT_EQ(m.length, 2, "plane length");
}

static void test_classify_airplane_pairs(void) {
    begin_suite("classify: airplane+pairs");
    // 333 444 + 55 + 66  (5N = 10, N=2)
    Card cs[10];
    fill_rank(cs, RANK_3, 3);
    fill_rank(cs + 3, RANK_4, 3);
    fill_rank(cs + 6, RANK_5, 2);
    fill_rank(cs + 8, RANK_6, 2);
    Move m = moves_classify(cs, 10);
    EXPECT_EQ(m.type, MOVE_TRIPLE_STRAIGHT_PAIRS, "type");
    EXPECT_EQ(m.rank, RANK_3, "plane start rank");
    EXPECT_EQ(m.length, 2, "plane length");
}

static void test_classify_four_two_singles(void) {
    begin_suite("classify: four+two singles");
    Card cs[6];
    fill_rank(cs, RANK_K, 4);
    cs[4] = card(RANK_3, 0);
    cs[5] = card(RANK_A, 0);
    Move m = moves_classify(cs, 6);
    EXPECT_EQ(m.type, MOVE_FOUR_TWO_SINGLES, "type");
    EXPECT_EQ(m.rank, RANK_K, "rank == RANK_K");
}

static void test_classify_four_two_pairs(void) {
    begin_suite("classify: four+two pairs");
    Card cs[8];
    fill_rank(cs, RANK_J, 4);
    fill_rank(cs + 4, RANK_3, 2);
    fill_rank(cs + 6, RANK_A, 2);
    Move m = moves_classify(cs, 8);
    EXPECT_EQ(m.type, MOVE_FOUR_TWO_PAIRS, "type");
    EXPECT_EQ(m.rank, RANK_J, "rank == RANK_J");
}

static void test_classify_invalid(void) {
    begin_suite("classify: invalid");
    // Three different ranks.
    Card cs[3] = {card(RANK_3, 0), card(RANK_5, 0), card(RANK_7, 0)};
    Move m = moves_classify(cs, 3);
    EXPECT_EQ(m.type, MOVE_INVALID, "three mixed singles → invalid");
}

// ---------------------------------------------------------------------------
// Tests: moves_beats
// ---------------------------------------------------------------------------

static Move make_single(int rank) {
    Move m;
    memset(&m, 0, sizeof(m));
    m.type = MOVE_SINGLE;
    m.rank = rank;
    m.length = 1;
    m.count = 1;
    return m;
}

static Move make_pair(int rank) {
    Move m;
    memset(&m, 0, sizeof(m));
    m.type = MOVE_PAIR;
    m.rank = rank;
    m.length = 1;
    m.count = 2;
    return m;
}

static Move make_bomb(int rank) {
    Move m;
    memset(&m, 0, sizeof(m));
    m.type = MOVE_BOMB;
    m.rank = rank;
    m.length = 1;
    m.count = 4;
    return m;
}

static Move make_straight(int start, int len) {
    Move m;
    memset(&m, 0, sizeof(m));
    m.type = MOVE_STRAIGHT;
    m.rank = start;
    m.length = len;
    m.count = len;
    return m;
}

static Move make_pass(void) {
    Move m;
    memset(&m, 0, sizeof(m));
    m.type = MOVE_PASS;
    return m;
}

static Move make_rocket(void) {
    Move m;
    memset(&m, 0, sizeof(m));
    m.type = MOVE_ROCKET;
    m.rank = RANK_BIG_JOKER;
    m.length = 1;
    m.count = 2;
    return m;
}

static void test_beats_basics(void) {
    begin_suite("beats: basic comparisons");
    Move pass = make_pass();
    Move s5 = make_single(RANK_5);
    Move s7 = make_single(RANK_7);
    Move p5 = make_pair(RANK_5);
    Move p8 = make_pair(RANK_8);
    Move b3 = make_bomb(RANK_3);
    Move b5 = make_bomb(RANK_5);
    Move rkt = make_rocket();

    // Any non-pass beats an empty table.
    EXPECT(moves_beats(&s5, &pass), "single beats pass");
    EXPECT(moves_beats(&p5, &pass), "pair beats pass");
    EXPECT(moves_beats(&b3, &pass), "bomb beats pass");
    EXPECT(moves_beats(&rkt, &pass), "rocket beats pass");

    // Pass never beats anything.
    EXPECT(!moves_beats(&pass, &s5), "pass never beats single");

    // Higher rank wins.
    EXPECT(moves_beats(&s7, &s5), "7 beats 5");
    EXPECT(!moves_beats(&s5, &s7), "5 does not beat 7");

    // Wrong type.
    EXPECT(!moves_beats(&p5, &s5), "pair does not beat single (wrong type)");
    EXPECT(!moves_beats(&s5, &p5), "single does not beat pair (wrong type)");

    // Higher pair beats lower pair.
    EXPECT(moves_beats(&p8, &p5), "pair 8 beats pair 5");
    EXPECT(!moves_beats(&p5, &p8), "pair 5 does not beat pair 8");

    // Bomb beats non-bomb.
    EXPECT(moves_beats(&b3, &s7), "bomb beats single");
    EXPECT(moves_beats(&b3, &p8), "bomb beats pair");

    // Higher bomb beats lower bomb.
    EXPECT(moves_beats(&b5, &b3), "bomb 5 beats bomb 3");
    EXPECT(!moves_beats(&b3, &b5), "bomb 3 does not beat bomb 5");

    // Rocket beats bomb.
    EXPECT(moves_beats(&rkt, &b5), "rocket beats bomb");

    // Nothing beats rocket.
    EXPECT(!moves_beats(&b5, &rkt), "bomb does not beat rocket");
    EXPECT(!moves_beats(&rkt, &rkt), "rocket does not beat rocket");
}

static void test_beats_straight(void) {
    begin_suite("beats: straights");
    Move s35 = make_straight(RANK_3, 5); // 3-4-5-6-7
    Move s45 = make_straight(RANK_4, 5); // 4-5-6-7-8
    Move s36 = make_straight(RANK_3, 6); // 3-4-5-6-7-8

    EXPECT(moves_beats(&s45, &s35), "higher start straight beats");
    EXPECT(!moves_beats(&s35, &s45), "lower start does not beat");
    // Different lengths cannot beat each other.
    EXPECT(!moves_beats(&s36, &s35), "length-6 cannot respond to length-5");
}

// ---------------------------------------------------------------------------
// Tests: moves_generate
// ---------------------------------------------------------------------------

static void test_generate_pass(void) {
    begin_suite("generate: from empty table");
    // Hand: 3♠ 3♥ 5♠ 7♠ 7♥ 7♦
    Card hand[] = {
        card(RANK_3, 0), card(RANK_3, 1),
        card(RANK_5, 0),
        card(RANK_7, 0), card(RANK_7, 1), card(RANK_7, 2),
    };
    int hand_size = (int) (sizeof(hand) / sizeof(hand[0]));
    Move prev = make_pass();
    Move out[256];
    int n = moves_generate(hand, hand_size, &prev, out, 256);

    EXPECT(n > 0, "at least one move generated");

    // There should be a MOVE_SINGLE for rank 5 somewhere.
    int found_single_5 = 0, found_triple_7 = 0, found_pair_3 = 0;
    for (int i = 0; i < n; i++) {
        if (out[i].type == MOVE_SINGLE && out[i].rank == RANK_5) found_single_5 = 1;
        if (out[i].type == MOVE_TRIPLE && out[i].rank == RANK_7) found_triple_7 = 1;
        if (out[i].type == MOVE_PAIR && out[i].rank == RANK_3) found_pair_3 = 1;
    }
    EXPECT(found_single_5, "single 5 generated");
    EXPECT(found_triple_7, "triple 7 generated");
    EXPECT(found_pair_3, "pair 3 generated");
}

static void test_generate_beat_single(void) {
    begin_suite("generate: must beat a single");
    // Hand: 3♠ 7♠ K♠ 2♠
    Card hand[] = {
        card(RANK_3, 0), card(RANK_7, 0),
        card(RANK_K, 0), card(RANK_2, 0),
    };
    int hand_size = 4;
    Move prev = make_single(RANK_5); // on table: single 5
    Move out[256];
    int n = moves_generate(hand, hand_size, &prev, out, 256);

    // Should only generate singles higher than 5 (7, K, 2).
    int found_3 = 0;
    for (int i = 0; i < n; i++) {
        if (out[i].type == MOVE_SINGLE && out[i].rank == RANK_3) found_3 = 1;
        // All generated singles must beat the prev.
        if (out[i].type == MOVE_SINGLE)
            EXPECT(moves_beats(&out[i], &prev), "generated single beats prev");
    }
    EXPECT(!found_3, "single 3 not generated (too low)");
    EXPECT(n >= 3, "at least 3 valid singles (7, K, 2)");
}

static void test_generate_beat_straight(void) {
    begin_suite("generate: must beat a straight");
    // Hand containing 4-5-6-7-8 and 5-6-7-8-9.
    Card hand[] = {
        card(RANK_4, 0), card(RANK_5, 0), card(RANK_5, 1),
        card(RANK_6, 0), card(RANK_6, 1),
        card(RANK_7, 0), card(RANK_7, 1),
        card(RANK_8, 0), card(RANK_8, 1),
        card(RANK_9, 0),
    };
    int hand_size = 10;
    Move prev = make_straight(RANK_3, 5); // table: 3-4-5-6-7
    Move out[512];
    int n = moves_generate(hand, hand_size, &prev, out, 512);

    // 4-5-6-7-8 and 5-6-7-8-9 should both appear.
    int found_4_start = 0, found_5_start = 0;
    for (int i = 0; i < n; i++) {
        if (out[i].type == MOVE_STRAIGHT && out[i].length == 5) {
            if (out[i].rank == RANK_4) found_4_start = 1;
            if (out[i].rank == RANK_5) found_5_start = 1;
        }
        if (out[i].type == MOVE_STRAIGHT)
            EXPECT(moves_beats(&out[i], &prev), "generated straight beats prev");
    }
    EXPECT(found_4_start, "straight starting at 4 generated");
    EXPECT(found_5_start, "straight starting at 5 generated");
}

static void test_generate_bomb_always_available(void) {
    begin_suite("generate: bombs generated against any move");
    // Hand has a bomb of 3s.
    Card hand[] = {
        card(RANK_3, 0), card(RANK_3, 1), card(RANK_3, 2), card(RANK_3, 3),
        card(RANK_7, 0),
    };
    int hand_size = 5;
    Move prev = make_pair(RANK_K); // table: pair of Kings
    Move out[256];
    int n = moves_generate(hand, hand_size, &prev, out, 256);

    int found_bomb = 0;
    for (int i = 0; i < n; i++) {
        if (out[i].type == MOVE_BOMB && out[i].rank == RANK_3) found_bomb = 1;
    }
    EXPECT(found_bomb, "bomb of 3s generated against pair of K");
}

static void test_generate_empty_hand(void) {
    begin_suite("generate: empty hand");
    Move prev = make_pass();
    Move out[16];
    int n = moves_generate(NULL, 0, &prev, out, 16);
    EXPECT_EQ(n, 0, "no moves from empty hand");
}

// ---------------------------------------------------------------------------
// Tests: moves_count_ranks
// ---------------------------------------------------------------------------

static void test_count_ranks(void) {
    begin_suite("count_ranks");
    Card hand[] = {
        card(RANK_3, 0), card(RANK_3, 1), card(RANK_3, 2),
        card(RANK_7, 0),
        card(RANK_BIG_JOKER, 0),
    };
    int cnt[RANK_COUNT_SIZE];
    moves_count_ranks(hand, 5, cnt);
    EXPECT_EQ(cnt[RANK_3], 3, "three 3s");
    EXPECT_EQ(cnt[RANK_7], 1, "one 7");
    EXPECT_EQ(cnt[RANK_BIG_JOKER], 1, "one big joker");
    EXPECT_EQ(cnt[RANK_5], 0, "no 5s");
}

// ---------------------------------------------------------------------------
// Tests: moves_type_name
// ---------------------------------------------------------------------------

static void test_type_names(void) {
    begin_suite("type_names");
    EXPECT(moves_type_name(MOVE_PASS) != NULL, "PASS has name");
    EXPECT(moves_type_name(MOVE_BOMB) != NULL, "BOMB has name");
    EXPECT(moves_type_name(MOVE_ROCKET) != NULL, "ROCKET has name");
    EXPECT(moves_type_name(MOVE_INVALID) != NULL, "INVALID has name");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(void) {
    // classify
    test_classify_single();
    test_classify_pair();
    test_classify_triple();
    test_classify_triple_single();
    test_classify_triple_pair();
    test_classify_bomb();
    test_classify_rocket();
    test_classify_straight();
    test_classify_pair_straight();
    test_classify_triple_straight();
    test_classify_airplane_singles();
    test_classify_airplane_pairs();
    test_classify_four_two_singles();
    test_classify_four_two_pairs();
    test_classify_invalid();

    // beats
    test_beats_basics();
    test_beats_straight();

    // generate
    test_generate_pass();
    test_generate_beat_single();
    test_generate_beat_straight();
    test_generate_bomb_always_available();
    test_generate_empty_hand();

    // utilities
    test_count_ranks();
    test_type_names();

    PRINT_RESULTS();
    RETURN_TEST_RESULT();
}
