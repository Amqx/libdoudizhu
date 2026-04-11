/**
 * @file eval.c
 * @brief Implementation of hand evaluation utilities for libdoudizhu
 * @author Peng Yang Deng, Emma Le
 * @date 08-Mar-26
 */

#include "eval.h"
#include "utils.h"

int evalHandScore(const Card cards[], const int n) {
    int cnt[RANK_COUNT_SIZE];
    movesCountRanks(cards, n, cnt); // count how many of each rank you have

    int score = 0; // total hand score

    // Rocket (both jokers)
    if (cnt[RANK_SMALL_JOKER] >= 1 && cnt[RANK_BIG_JOKER] >= 1)
        score += 25;

    // Bombs (four-of-a-kind)
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        if (cnt[r] >= 4)
            score += 20;
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
            if (run >= 5)
                score += 5;
        } else {
            run = 0;
        }
    }

    // Small bonuses for pairs and triples
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        if (cnt[r] == 2)
            score += 2;
        else if (cnt[r] == 3)
            score += 5;
    }

    return score;
}

int evalCountBombs(const Card cards[], const int n) {
    int cnt[RANK_COUNT_SIZE];
    movesCountRanks(cards, n, cnt);

    int bombs = 0;
    if (cnt[RANK_SMALL_JOKER] >= 1 && cnt[RANK_BIG_JOKER] >= 1)
        bombs++;
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        if (cnt[r] >= 4)
            bombs++;
    }
    return bombs;
}

/**
 * Absorb a kicker from rank-count arrays, preferring singles.
 * This function is used to remove one card (preferably a single) when attaching kickers to combos to reduce total moves
 * @param tmp Rank count array of available cards.
 */
static void absorbKicker(int tmp[RANK_COUNT_SIZE]) {
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
 * Calculates the MINIMUM number of moves needed to play all your cards
 * input: tmp[] or cards[] - your hand
 * output: plays (int) - how many turns to finish your hand
 * @brief Core greedy min-plays algorithm operating on a mutable rank-count array.
 * @param tmp Rank count array of available cards.
 */
static int minPlaysInner(int tmp[RANK_COUNT_SIZE]) {
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
                if (cur_s < 0)
                    cur_s = r;
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
        if (best_l < 2)
            break;
        for (int r = best_s; r < best_s + best_l; r++)
            tmp[r] -= 3;
        for (int ki = 0; ki < best_l; ki++)
            absorbKicker(tmp);
        plays++;
    }

    // Longest consecutive pair straight (length >= 3 pairs = 1 play)
    for (;;) {
        int best_s = -1, best_l = 0, cur_s = -1, cur_l = 0;
        for (int r = 0; r <= RANK_MAX_STRAIGHT; r++) {
            if (tmp[r] >= 2) {
                if (cur_s < 0)
                    cur_s = r;
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
        if (best_l < 3)
            break;
        for (int r = best_s; r < best_s + best_l; r++)
            tmp[r] -= 2;
        plays++;
    }

    // Longest single straight (length >= 5 = 1 play)
    for (;;) {
        int best_s = -1, best_l = 0, cur_s = -1, cur_l = 0;
        for (int r = 0; r <= RANK_MAX_STRAIGHT; r++) {
            if (tmp[r] >= 1) {
                if (cur_s < 0)
                    cur_s = r;
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
        if (best_l < 5)
            break;
        for (int r = best_s; r < best_s + best_l; r++)
            tmp[r]--;
        plays++;
    }

    // Remaining triples (each absorbs 1 kicker, saving 1 future play)
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        while (tmp[r] >= 3) {
            tmp[r] -= 3;
            plays++;
            absorbKicker(tmp);
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

int evalMinPlays(const Card cards[], const int n) {
    int tmp[RANK_COUNT_SIZE];
    movesCountRanks(cards, n, tmp);
    return minPlaysInner(tmp);
}

int evalMinPlaysCounts(const int cnt[RANK_COUNT_SIZE]) {
    int tmp[RANK_COUNT_SIZE];
    lddzMemcpy(tmp, cnt, RANK_COUNT_SIZE * sizeof(int));
    return minPlaysInner(tmp);
}

/**
 * Gives a bonus for control cards in a rank count array.
 * @param cnt Cards available in hand.
 */

static int controlBonus(const int cnt[RANK_COUNT_SIZE]) {
    int c = 0; // total control bonus score

    // Rocket (both jokers together) - very strong control
    if (cnt[RANK_SMALL_JOKER] >= 1 && cnt[RANK_BIG_JOKER] >= 1)
        c += 15;

    // Bombs (four-of-a-kind) - strong control plays
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        if (cnt[r] >= 4)
            c += 10;
    }

    // High control cards -> harder to beat
    c += cnt[RANK_2] * 5;
    c += cnt[RANK_A] * 2;

    return c;
}

int evalBidStrength(const Card cards[], const int n) {
    int cnt[RANK_COUNT_SIZE];
    movesCountRanks(cards, n, cnt);
    int score = 0;

    // Rocket gets the biggest bonus
    if (cnt[RANK_SMALL_JOKER] >= 1 && cnt[RANK_BIG_JOKER] >= 1)
        score += 30;

    // Bombs
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        if (cnt[r] >= 4)
            score += 20;
    }

    // High-control singles
    score += cnt[RANK_BIG_JOKER] * 10;
    score += cnt[RANK_SMALL_JOKER] * 8;
    score += cnt[RANK_2] * 7;
    score += cnt[RANK_A] * 3;
    score += cnt[RANK_K] * 1;

    // Bonus for a hand that empties quickly
    const int mp = evalMinPlaysCounts(cnt);
    if (mp <= 5)
        score += 5;
    if (mp <= 3)
        score += 5;

    return score;
}

int evalPlayPosition(const Card cards[], const int n) {
    int cnt[RANK_COUNT_SIZE];
    movesCountRanks(cards, n, cnt);
    const int mp = evalMinPlaysCounts(cnt);
    return 200 - mp * 15 + controlBonus(cnt);
}

int evalPlayPositionCounts(const int cnt[RANK_COUNT_SIZE], const int n) {
    if (n <= 0)
        return 200;
    const int mp = evalMinPlaysCounts(cnt);
    return 200 - mp * 15 + controlBonus(cnt);
}

int evalMoveCost(const Move* m) {
    switch (m->type) {
        case MOVE_SINGLE:
            return m->rank;
        case MOVE_PAIR:
            return m->rank + 15;
        case MOVE_TRIPLE:
            return m->rank + 30;
        case MOVE_TRIPLE_SINGLE:
            return m->rank + 35;
        case MOVE_TRIPLE_PAIR:
            return m->rank + 40;
        case MOVE_STRAIGHT:
            return m->rank + 45;
        case MOVE_PAIR_STRAIGHT:
            return m->rank + 50;
        case MOVE_TRIPLE_STRAIGHT:
            return m->rank + 55;
        case MOVE_TRIPLE_STRAIGHT_SINGLES:
            return m->rank + 60;
        case MOVE_TRIPLE_STRAIGHT_PAIRS:
            return m->rank + 65;
        case MOVE_FOUR_TWO_SINGLES:
            return m->rank + 70;
        case MOVE_FOUR_TWO_PAIRS:
            return m->rank + 75;
        case MOVE_BOMB:
            return m->rank + 80;
        case MOVE_ROCKET:
            return 100;
        default:
            return 0;
    }
}
