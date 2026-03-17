/**
 * @file eval.c
 * @brief Implementation of hand evaluation utilities for libdoudizhu
 * @author Jonathan
 * @date 08-Mar-26
 */

#include "eval.h"
#include "utils.h"

int eval_hand_score(const Card cards[], const int n) {
    int cnt[RANK_COUNT_SIZE];
    moves_count_ranks(cards, n, cnt);

    int score = 0;

    // Rocket (both jokers)
    if (cnt[RANK_SMALL_JOKER] >= 1 && cnt[RANK_BIG_JOKER] >= 1) score += 25;

    // Bombs (four-of-a-kind)
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        if (cnt[r] >= 4) score += 20;
    }

    // High-value singles
    score += cnt[RANK_BIG_JOKER] * 10;
    score += cnt[RANK_SMALL_JOKER] * 8;
    score += cnt[RANK_2] * 6;
    score += cnt[RANK_A] * 3;
    score += cnt[RANK_K] * 2;

    // Bonus for 5+ consecutive singles
    int run = 0;
    for (int r = RANK_3; r <= RANK_A; r++) {
        if (cnt[r] >= 1) {
            run++;
            if (run >= 5) score += 5;
        } else {
            run = 0;
        }
    }

    // Small bonuses for pairs and triples
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        if (cnt[r] == 2) score += 2;
        else if (cnt[r] == 3) score += 5;
    }

    return score;
}

int eval_count_bombs(const Card cards[], const int n) {
    int cnt[RANK_COUNT_SIZE];
    moves_count_ranks(cards, n, cnt);

    int bombs = 0;
    if (cnt[RANK_SMALL_JOKER] >= 1 && cnt[RANK_BIG_JOKER] >= 1) bombs++;
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        if (cnt[r] >= 4) bombs++;
    }
    return bombs;
}

/**
 * Absorb a kicker from rank-count arrays, preferring singles.
 * @param tmp Rank count array of available cards.
 */
static void absorb_kicker(int tmp[RANK_COUNT_SIZE]) {
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        if (tmp[r] == 1) {
            tmp[r] = 0;
            return;
        }
    }
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        if (tmp[r] > 0) {
            tmp[r]--;
            return;
        }
    }
}

/**
 * @brief Core greedy min-plays algorithm operating on a mutable rank-count array.
 * @param tmp Rank count array of available cards.
 */
static int min_plays_inner(int tmp[RANK_COUNT_SIZE]) {
    int plays = 0;

    // Rocket (both jokers together = 1 play)
    if (tmp[RANK_SMALL_JOKER] >= 1 && tmp[RANK_BIG_JOKER] >= 1) {
        plays++;
        tmp[RANK_SMALL_JOKER] = 0;
        tmp[RANK_BIG_JOKER] = 0;
    }

    // Bombs (four-of-a-kind = 1 play each)
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        plays += tmp[r] / 4;
        tmp[r] %= 4;
    }

    // Longest consecutive airplane chain (triples, length >= 2 = 1 play).
    // Each airplane slot absorbs one kicker (saving a future play).
    for (;;) {
        int best_s = -1, best_l = 0, cur_s = -1, cur_l = 0;
        for (int r = 0; r <= RANK_MAX_STRAIGHT; r++) {
            if (tmp[r] >= 3) {
                if (cur_s < 0) cur_s = r;
                cur_l++;
            } else {
                if (cur_l >= 2 && cur_l > best_l) {
                    best_s = cur_s;
                    best_l = cur_l;
                }
                cur_s = -1;
                cur_l = 0;
            }
        }
        if (cur_l >= 2 && cur_l > best_l) {
            best_s = cur_s;
            best_l = cur_l;
        }
        if (best_l < 2) break;
        for (int r = best_s; r < best_s + best_l; r++) tmp[r] -= 3;
        for (int ki = 0; ki < best_l; ki++) absorb_kicker(tmp);
        plays++;
    }

    // Longest consecutive pair straight (length >= 3 pairs = 1 play)
    for (;;) {
        int best_s = -1, best_l = 0, cur_s = -1, cur_l = 0;
        for (int r = 0; r <= RANK_MAX_STRAIGHT; r++) {
            if (tmp[r] >= 2) {
                if (cur_s < 0) cur_s = r;
                cur_l++;
            } else {
                if (cur_l >= 3 && cur_l > best_l) {
                    best_s = cur_s;
                    best_l = cur_l;
                }
                cur_s = -1;
                cur_l = 0;
            }
        }
        if (cur_l >= 3 && cur_l > best_l) {
            best_s = cur_s;
            best_l = cur_l;
        }
        if (best_l < 3) break;
        for (int r = best_s; r < best_s + best_l; r++) tmp[r] -= 2;
        plays++;
    }

    // Longest single straight (length >= 5 = 1 play)
    for (;;) {
        int best_s = -1, best_l = 0, cur_s = -1, cur_l = 0;
        for (int r = 0; r <= RANK_MAX_STRAIGHT; r++) {
            if (tmp[r] >= 1) {
                if (cur_s < 0) cur_s = r;
                cur_l++;
            } else {
                if (cur_l >= 5 && cur_l > best_l) {
                    best_s = cur_s;
                    best_l = cur_l;
                }
                cur_s = -1;
                cur_l = 0;
            }
        }
        if (cur_l >= 5 && cur_l > best_l) {
            best_s = cur_s;
            best_l = cur_l;
        }
        if (best_l < 5) break;
        for (int r = best_s; r < best_s + best_l; r++) tmp[r]--;
        plays++;
    }

    // Remaining triples (each absorbs 1 kicker, saving 1 future play)
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        while (tmp[r] >= 3) {
            tmp[r] -= 3;
            plays++;
            absorb_kicker(tmp);
        }
    }

    // Remaining pairs
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        plays += tmp[r] / 2;
        tmp[r] %= 2;
    }

    // Remaining singles
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        plays += tmp[r];
    }

    return plays;
}

int eval_min_plays(const Card cards[], const int n) {
    int tmp[RANK_COUNT_SIZE];
    moves_count_ranks(cards, n, tmp);
    return min_plays_inner(tmp);
}

int eval_min_plays_counts(const int cnt[RANK_COUNT_SIZE], const int n) {
    (void) n;
    int tmp[RANK_COUNT_SIZE];
    memcpy(tmp, cnt, RANK_COUNT_SIZE * sizeof(int));
    return min_plays_inner(tmp);
}

/**
 * Gives a bonus for control cards in a rank count array.
 * @param cnt Cards available in hand.
 */
static int control_bonus(const int cnt[RANK_COUNT_SIZE]) {
    int c = 0;
    if (cnt[RANK_SMALL_JOKER] >= 1 && cnt[RANK_BIG_JOKER] >= 1) c += 15;
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        if (cnt[r] >= 4) c += 10;
    }
    c += cnt[RANK_2] * 5;
    c += cnt[RANK_A] * 2;
    return c;
}

int eval_bid_strength(const Card cards[], const int n) {
    int cnt[RANK_COUNT_SIZE];
    moves_count_ranks(cards, n, cnt);
    int score = 0;

    // Rocket gets the biggest bonus
    if (cnt[RANK_SMALL_JOKER] >= 1 && cnt[RANK_BIG_JOKER] >= 1) score += 30;

    // Bombs
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        if (cnt[r] >= 4) score += 20;
    }

    // High-control singles
    score += cnt[RANK_BIG_JOKER] * 10;
    score += cnt[RANK_SMALL_JOKER] * 8;
    score += cnt[RANK_2] * 7;
    score += cnt[RANK_A] * 3;
    score += cnt[RANK_K] * 1;

    // Bonus for a hand that empties quickly
    const int mp = eval_min_plays_counts(cnt, n);
    if (mp <= 5) score += 5;
    if (mp <= 3) score += 5;

    return score;
}

int eval_play_position(const Card cards[], const int n) {
    int cnt[RANK_COUNT_SIZE];
    moves_count_ranks(cards, n, cnt);
    const int mp = eval_min_plays_counts(cnt, n);
    return 200 - mp * 15 + control_bonus(cnt);
}

int eval_play_position_counts(const int cnt[RANK_COUNT_SIZE], const int n) {
    if (n <= 0) return 200;
    const int mp = eval_min_plays_counts(cnt, n);
    return 200 - mp * 15 + control_bonus(cnt);
}

int eval_move_cost(const Move *m) {
    switch (m->type) {
        case MOVE_SINGLE: return m->rank;
        case MOVE_PAIR: return m->rank + 15;
        case MOVE_TRIPLE: return m->rank + 30;
        case MOVE_TRIPLE_SINGLE: return m->rank + 35;
        case MOVE_TRIPLE_PAIR: return m->rank + 40;
        case MOVE_STRAIGHT: return m->rank + 45;
        case MOVE_PAIR_STRAIGHT: return m->rank + 50;
        case MOVE_TRIPLE_STRAIGHT: return m->rank + 55;
        case MOVE_TRIPLE_STRAIGHT_SINGLES: return m->rank + 60;
        case MOVE_TRIPLE_STRAIGHT_PAIRS: return m->rank + 65;
        case MOVE_FOUR_TWO_SINGLES: return m->rank + 70;
        case MOVE_FOUR_TWO_PAIRS: return m->rank + 75;
        case MOVE_BOMB: return m->rank + 80;
        case MOVE_ROCKET: return 100;
        default: return 0;
    }
}
