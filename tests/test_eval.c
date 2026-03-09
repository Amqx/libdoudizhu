/**
 * @file test_eval.c
 * @brief Tests for the eval module (hand evaluation utilities)
 * @author Jonathan
 * @date 08-Mar-26
 */

#include "eval.h"
#include "test_framework.h"
#include <string.h>

/* ---------------------------------------------------------------------------
 * Helpers
 * --------------------------------------------------------------------------- */

static Card card(int rank, int suit) {
    if (rank == RANK_SMALL_JOKER) return 52;
    if (rank == RANK_BIG_JOKER) return 53;
    return (Card)(rank * 4 + suit);
}

static void fill_rank(Card *buf, int rank, int n) {
    for (int i = 0; i < n; i++) buf[i] = card(rank, i);
}

/* ---------------------------------------------------------------------------
 * Tests: eval_hand_score
 * --------------------------------------------------------------------------- */

static void test_score_strong_beats_weak(void) {
    begin_suite("eval_hand_score: strong hand scores higher than weak");

    /* Weak hand: low singles only */
    Card weak[] = {
        card(RANK_3, 0), card(RANK_4, 0), card(RANK_5, 0),
        card(RANK_6, 0), card(RANK_7, 0),
    };

    /* Strong hand: bomb + rocket + 2 */
    Card strong[6];
    fill_rank(strong, RANK_K, 4);
    strong[4] = card(RANK_SMALL_JOKER, 0);
    strong[5] = card(RANK_BIG_JOKER, 0);

    int ws = eval_hand_score(weak, 5);
    int ss = eval_hand_score(strong, 6);
    EXPECT(ss > ws, "strong hand scores higher than weak hand");
}

static void test_score_rocket_adds_to_score(void) {
    begin_suite("eval_hand_score: rocket is valued");

    Card no_rocket[] = {card(RANK_3, 0), card(RANK_4, 0)};
    Card has_rocket[] = {card(RANK_SMALL_JOKER, 0), card(RANK_BIG_JOKER, 0)};

    int s1 = eval_hand_score(no_rocket, 2);
    int s2 = eval_hand_score(has_rocket, 2);
    EXPECT(s2 > s1, "hand with rocket scores higher");
}

static void test_score_bomb_adds_to_score(void) {
    begin_suite("eval_hand_score: each bomb raises the score");

    Card no_bomb[4] = {
        card(RANK_3, 0), card(RANK_4, 0), card(RANK_5, 0), card(RANK_6, 0)
    };
    Card has_bomb[4];
    fill_rank(has_bomb, RANK_A, 4);

    int s1 = eval_hand_score(no_bomb, 4);
    int s2 = eval_hand_score(has_bomb, 4);
    EXPECT(s2 > s1, "hand with bomb scores higher");
}

static void test_score_straight_potential_bonus(void) {
    begin_suite("eval_hand_score: 5-card straight potential adds bonus");

    /* Five consecutive ranks: 3-4-5-6-7 */
    Card straight[5];
    for (int i = 0; i < 5; i++) straight[i] = card(RANK_3 + i, 0);

    /* Five non-consecutive ranks of similar values */
    Card scattered[5] = {
        card(RANK_3, 0), card(RANK_5, 0), card(RANK_7, 0),
        card(RANK_9, 0), card(RANK_J, 0),
    };

    int s_run = eval_hand_score(straight, 5);
    int s_scat = eval_hand_score(scattered, 5);
    EXPECT(s_run > s_scat, "consecutive sequence scores higher than scattered");
}

static void test_score_empty_hand(void) {
    begin_suite("eval_hand_score: empty hand scores zero");
    int s = eval_hand_score(NULL, 0);
    EXPECT_EQ(s, 0, "empty hand → score 0");
}

/* ---------------------------------------------------------------------------
 * Tests: eval_count_bombs
 * --------------------------------------------------------------------------- */

static void test_count_bombs_none(void) {
    begin_suite("eval_count_bombs: hand with no bombs");
    Card hand[] = {card(RANK_3, 0), card(RANK_5, 0), card(RANK_7, 0)};
    EXPECT_EQ(eval_count_bombs(hand, 3), 0, "no bombs counted");
}

static void test_count_bombs_one_bomb(void) {
    begin_suite("eval_count_bombs: one four-of-a-kind");
    Card hand[4];
    fill_rank(hand, RANK_9, 4);
    EXPECT_EQ(eval_count_bombs(hand, 4), 1, "one bomb counted");
}

static void test_count_bombs_rocket_counts(void) {
    begin_suite("eval_count_bombs: rocket counts as one bomb");
    Card hand[2] = {card(RANK_SMALL_JOKER, 0), card(RANK_BIG_JOKER, 0)};
    EXPECT_EQ(eval_count_bombs(hand, 2), 1, "rocket counts as bomb");
}

static void test_count_bombs_multiple(void) {
    begin_suite("eval_count_bombs: two bombs + rocket = 3");
    Card hand[10];
    fill_rank(hand, RANK_3, 4);
    fill_rank(hand + 4, RANK_7, 4);
    hand[8] = card(RANK_SMALL_JOKER, 0);
    hand[9] = card(RANK_BIG_JOKER, 0);
    EXPECT_EQ(eval_count_bombs(hand, 10), 3, "two bombs + rocket = 3");
}

/* ---------------------------------------------------------------------------
 * Tests: eval_move_cost
 * --------------------------------------------------------------------------- */

static void test_cost_ordering(void) {
    begin_suite("eval_move_cost: ordering (single < pair < triple < bomb < rocket)");

    Move single = {MOVE_SINGLE, {0}, 1, RANK_3, 1};
    Move pair = {MOVE_PAIR, {0}, 2, RANK_3, 1};
    Move triple = {MOVE_TRIPLE, {0}, 3, RANK_3, 1};
    Move bomb = {MOVE_BOMB, {0}, 4, RANK_3, 1};
    Move rocket = {MOVE_ROCKET, {0}, 2, RANK_BIG_JOKER, 1};

    EXPECT(eval_move_cost(&single) < eval_move_cost(&pair), "single < pair");
    EXPECT(eval_move_cost(&pair) < eval_move_cost(&triple), "pair < triple");
    EXPECT(eval_move_cost(&triple) < eval_move_cost(&bomb), "triple < bomb");
    EXPECT(eval_move_cost(&bomb) < eval_move_cost(&rocket), "bomb < rocket");
}

static void test_cost_higher_rank_costs_more(void) {
    begin_suite("eval_move_cost: higher rank = higher cost within same type");

    Move s_low = {MOVE_SINGLE, {0}, 1, RANK_3, 1};
    Move s_high = {MOVE_SINGLE, {0}, 1, RANK_2, 1};
    Move p_low = {MOVE_PAIR, {0}, 2, RANK_4, 1};
    Move p_high = {MOVE_PAIR, {0}, 2, RANK_K, 1};

    EXPECT(eval_move_cost(&s_low) < eval_move_cost(&s_high), "single 3 < single 2");
    EXPECT(eval_move_cost(&p_low) < eval_move_cost(&p_high), "pair 4 < pair K");
}

static void test_cost_pass_and_invalid(void) {
    begin_suite("eval_move_cost: PASS and INVALID return 0");
    Move pass = {MOVE_PASS, {0}, 0, 0, 0};
    Move invalid = {MOVE_INVALID, {0}, 0, 0, 0};
    EXPECT_EQ(eval_move_cost(&pass), 0, "PASS cost == 0");
    EXPECT_EQ(eval_move_cost(&invalid), 0, "INVALID cost == 0");
}

/* ---------------------------------------------------------------------------
 * Tests: eval_min_plays
 * --------------------------------------------------------------------------- */

static void test_min_plays_rocket_is_one(void) {
    begin_suite("eval_min_plays: rocket counts as 1 play");
    Card hand[2] = {card(RANK_SMALL_JOKER, 0), card(RANK_BIG_JOKER, 0)};
    EXPECT_EQ(eval_min_plays(hand, 2), 1, "rocket = 1 play");
}

static void test_min_plays_bomb_is_one(void) {
    begin_suite("eval_min_plays: four-of-a-kind counts as 1 play");
    Card hand[4];
    fill_rank(hand, RANK_7, 4);
    EXPECT_EQ(eval_min_plays(hand, 4), 1, "bomb = 1 play");
}

static void test_min_plays_straight_is_one(void) {
    begin_suite("eval_min_plays: 5-card straight counts as 1 play");
    Card hand[5];
    for (int i = 0; i < 5; i++) hand[i] = card(RANK_3 + i, 0);
    EXPECT_EQ(eval_min_plays(hand, 5), 1, "5-card straight = 1 play");
}

static void test_min_plays_triple_with_kicker(void) {
    begin_suite("eval_min_plays: triple+single consumed as 1 play");
    /* Three 5s + one isolated 3 → triple can absorb 3 as kicker = 1 play */
    Card hand[4] = {
        card(RANK_5, 0), card(RANK_5, 1), card(RANK_5, 2),
        card(RANK_3, 0)
    };
    EXPECT_EQ(eval_min_plays(hand, 4), 1, "triple+kicker = 1 play");
}

static void test_min_plays_scattered_worse_than_chain(void) {
    begin_suite("eval_min_plays: scattered hand needs more plays than chain");
    /* Chain: 3-4-5-6-7-8 straight + 9-10-J pair straight = potentially 2 plays */
    Card chain[8];
    for (int i = 0; i < 6; i++) chain[i] = card(RANK_3 + i, 0); /* 3-8 straight */
    chain[6] = card(RANK_9, 0);
    chain[7] = card(RANK_9, 1); /* pair 9s */

    /* Scattered: 8 different isolated ranks, no combos possible */
    Card scattered[8] = {
        card(RANK_3, 0), card(RANK_5, 0), card(RANK_7, 0), card(RANK_9, 0),
        card(RANK_J, 0), card(RANK_2, 0), card(RANK_A, 0), card(RANK_K, 0),
    };

    const int p_chain = eval_min_plays(chain, 8);
    const int p_scat = eval_min_plays(scattered, 8);
    EXPECT(p_chain < p_scat, "chain hand clears in fewer plays than scattered");
}

/* ---------------------------------------------------------------------------
 * Tests: eval_bid_strength
 * --------------------------------------------------------------------------- */

static void test_bid_strength_rocket_highest(void) {
    begin_suite("eval_bid_strength: rocket+bomb hand outscores plain singles");
    Card strong[6];
    fill_rank(strong, RANK_A, 4);
    strong[4] = card(RANK_SMALL_JOKER, 0);
    strong[5] = card(RANK_BIG_JOKER, 0);

    Card weak[6];
    for (int i = 0; i < 6; i++) weak[i] = card(RANK_3 + i, 0);

    EXPECT(eval_bid_strength(strong, 6) > eval_bid_strength(weak, 6),
           "rocket+bomb scores higher than singles for bidding");
}

static void test_bid_strength_twos_valued(void) {
    begin_suite("eval_bid_strength: 2s contribute significantly");
    Card with_twos[4] = {card(RANK_2, 0), card(RANK_2, 1), card(RANK_2, 2), card(RANK_2, 3)};
    Card without_twos[4] = {card(RANK_3, 0), card(RANK_4, 0), card(RANK_5, 0), card(RANK_6, 0)};
    EXPECT(eval_bid_strength(with_twos, 4) > eval_bid_strength(without_twos, 4),
           "hand with four 2s bids stronger than low singles");
}

/* ---------------------------------------------------------------------------
 * Tests: eval_play_position
 * --------------------------------------------------------------------------- */

static void test_play_position_fewer_plays_better(void) {
    begin_suite("eval_play_position: fewer required plays = better position");
    /* Straight: 1 play */
    Card straight[5];
    for (int i = 0; i < 5; i++) straight[i] = card(RANK_3 + i, 0);

    /* Scattered singles: 5 plays */
    Card singles[5] = {
        card(RANK_3, 0), card(RANK_5, 0), card(RANK_7, 0),
        card(RANK_9, 0), card(RANK_J, 0),
    };

    EXPECT(eval_play_position(straight, 5) > eval_play_position(singles, 5),
           "playable straight has better position than scattered singles");
}

static void test_play_position_control_cards_boost(void) {
    begin_suite("eval_play_position: control cards improve score");
    Card base[3] = {card(RANK_3, 0), card(RANK_4, 0), card(RANK_5, 0)};
    Card control[3] = {card(RANK_2, 0), card(RANK_A, 0), card(RANK_K, 0)};
    EXPECT(eval_play_position(control, 3) > eval_play_position(base, 3),
           "hand with 2/A/K has better play position than 3/4/5");
}

/* ---------------------------------------------------------------------------
 * Tests: eval_min_plays_counts / eval_play_position_counts
 * --------------------------------------------------------------------------- */

static void test_min_plays_counts_matches_card_variant(void) {
    begin_suite("eval_min_plays_counts: agrees with eval_min_plays");
    Card hand[6];
    for (int i = 0; i < 5; i++) hand[i] = card(RANK_3 + i, 0); /* 3-7 straight */
    hand[5] = card(RANK_9, 0);

    int cnt[RANK_COUNT_SIZE];
    /* Build count manually */
    memset(cnt, 0, sizeof(cnt));
    for (int i = 0; i < 6; i++) {
        int r = CARD_RANK(hand[i]);
        cnt[r]++;
    }

    int mp_cards = eval_min_plays(hand, 6);
    int mp_counts = eval_min_plays_counts(cnt, 6);
    EXPECT_EQ(mp_cards, mp_counts, "count-array variant matches card-array variant");
}

static void test_play_position_counts_matches_card_variant(void) {
    begin_suite("eval_play_position_counts: agrees with eval_play_position");
    Card hand[4];
    fill_rank(hand, RANK_A, 4); /* bomb of Aces */

    int cnt[RANK_COUNT_SIZE];
    memset(cnt, 0, sizeof(cnt));
    cnt[RANK_A] = 4;

    int pos_cards = eval_play_position(hand, 4);
    int pos_counts = eval_play_position_counts(cnt, 4);
    EXPECT_EQ(pos_cards, pos_counts, "count-array variant matches card-array variant");
}

/* ---------------------------------------------------------------------------
 * main
 * --------------------------------------------------------------------------- */

static void test_min_plays_complex_airplane(void) {
    begin_suite("eval_min_plays: airplane with kickers");
    // 333 444 + 5 + 6 (2 sets + 2 kickers = 1 play)
    Card hand[8];
    fill_rank(hand, RANK_3, 3);
    fill_rank(hand + 3, RANK_4, 3);
    hand[6] = card(RANK_5, 0);
    hand[7] = card(RANK_6, 0);
    EXPECT_EQ(eval_min_plays(hand, 8), 1, "airplane with 2 kickers = 1 play");
}

static void test_min_plays_nested_straights(void) {
    begin_suite("eval_min_plays: nested/overlapping straights");
    // 3-4-5-6-7-8-9 (7-card straight)
    Card hand[7];
    for (int i = 0; i < 7; i++) hand[i] = card(RANK_3 + i, 0);
    EXPECT_EQ(eval_min_plays(hand, 7), 1, "7-card straight = 1 play");
}

static void test_bid_strength_vs_hand_score(void) {
    begin_suite("eval_bid_strength: different from eval_hand_score");
    Card hand[4];
    fill_rank(hand, RANK_2, 4); // bomb of 2s
    int s1 = eval_hand_score(hand, 4);
    int s2 = eval_bid_strength(hand, 4);
    // Both should be high, but they are calculated differently.
    EXPECT(s1 > 0 && s2 > 0, "both give positive scores for bomb of 2s");
}

static void test_play_position_extreme(void) {
    begin_suite("eval_play_position: best and worst cases");
    Card best[2] = {card(RANK_SMALL_JOKER, 0), card(RANK_BIG_JOKER, 0)};
    Card worst[1] = {card(RANK_3, 0)};

    int p_best = eval_play_position(best, 2);
    int p_worst = eval_play_position(worst, 1);
    EXPECT(p_best > p_worst, "rocket has better play position than single 3");
}

static void test_min_plays_airplane_no_kickers(void) {
    begin_suite("eval_min_plays: airplane without kickers");
    Card hand[6];
    fill_rank(hand, RANK_5, 3);
    fill_rank(hand + 3, RANK_6, 3);
    EXPECT_EQ(eval_min_plays(hand, 6), 1, "333 444 = 1 play");
}

int main(void) {
    printf("--- Test file: %s ---\n", __FILE__);
    test_score_strong_beats_weak();
    test_score_rocket_adds_to_score();
    test_score_bomb_adds_to_score();
    test_score_straight_potential_bonus();
    test_score_empty_hand();

    test_count_bombs_none();
    test_count_bombs_one_bomb();
    test_count_bombs_rocket_counts();
    test_count_bombs_multiple();

    test_cost_ordering();
    test_cost_higher_rank_costs_more();
    test_cost_pass_and_invalid();

    test_min_plays_rocket_is_one();
    test_min_plays_bomb_is_one();
    test_min_plays_straight_is_one();
    test_min_plays_triple_with_kicker();
    test_min_plays_complex_airplane();
    test_min_plays_airplane_no_kickers();
    test_min_plays_nested_straights();
    test_min_plays_scattered_worse_than_chain();

    test_bid_strength_rocket_highest();
    test_bid_strength_twos_valued();
    test_bid_strength_vs_hand_score();

    test_play_position_fewer_plays_better();
    test_play_position_control_cards_boost();
    test_play_position_extreme();

    test_min_plays_counts_matches_card_variant();
    test_play_position_counts_matches_card_variant();

    PRINT_RESULTS();
    RETURN_TEST_RESULT();
}
