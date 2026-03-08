/**
 * @file moves.c
 * @brief Implementation of libdoudizhu moves and card logic
 * @author Jonathan
 * @date 08-Mar-26
 */

#include "moves.h"
#include <string.h>
#include <stdlib.h>

/* Internal Helpers */

/**
 * @brief Comparator for qsort to sort cards by rank.
 */
static int cmp_card_rank(const void *a, const void *b) {
    return (CARD_RANK(*(const Card *) a)) - (CARD_RANK(*(const Card *) b));
}

void moves_count_ranks(const Card *cards, const int n, int cnt[RANK_COUNT_SIZE]) {
    memset(cnt, 0, RANK_COUNT_SIZE * sizeof(int));
    for (int i = 0; i < n; i++) {
        const int r = CARD_RANK(cards[i]);
        if (r < RANK_COUNT_SIZE) cnt[r]++;
    }
}

/**
 * @brief Fills a card array from a count array using canonical card values.
 * @param cnt The rank-count source array.
 * @param out The destination card array.
 * @return The total number of cards written.
 */
static int fill_cards_from_counts(const int cnt[RANK_COUNT_SIZE], Card *out) {
    int n = 0;
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        for (int k = 0; k < cnt[r]; k++) {
            // Use suit 0 (spade) as canonical card for that rank.
            out[n++] = (r < 13) ? (Card)(r * 4) : (Card)(52 + (r - 13));
        }
    }
    return n;
}

/* Classification Helpers - Each of these returns 1 on a match and populates the move structures */

static int try_single(const int cnt[RANK_COUNT_SIZE], const int total, Move *m) {
    if (total != 1) return 0;
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        if (cnt[r] == 1) {
            m->type = MOVE_SINGLE;
            m->rank = r;
            m->length = 1;
            return 1;
        }
    }
    return 0;
}

static int try_pair(const int cnt[RANK_COUNT_SIZE], const int total, Move *m) {
    if (total != 2) return 0;
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        if (cnt[r] == 2) {
            m->type = MOVE_PAIR;
            m->rank = r;
            m->length = 1;
            return 1;
        }
    }
    return 0;
}

static int try_triple(const int cnt[RANK_COUNT_SIZE], const int total, Move *m) {
    if (total != 3) return 0;
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        if (cnt[r] == 3) {
            m->type = MOVE_TRIPLE;
            m->rank = r;
            m->length = 1;
            return 1;
        }
    }
    return 0;
}

static int try_bomb(const int cnt[RANK_COUNT_SIZE], const int total, Move *m) {
    if (total != 4) return 0;
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        if (cnt[r] == 4) {
            m->type = MOVE_BOMB;
            m->rank = r;
            m->length = 1;
            return 1;
        }
    }
    return 0;
}

static int try_rocket(const int cnt[RANK_COUNT_SIZE], const int total, Move *m) {
    if (total != 2) return 0;
    if (cnt[RANK_SMALL_JOKER] == 1 && cnt[RANK_BIG_JOKER] == 1) {
        m->type = MOVE_ROCKET;
        m->rank = RANK_BIG_JOKER;
        m->length = 1;
        return 1;
    }
    return 0;
}

static int try_triple_single(const int cnt[RANK_COUNT_SIZE], const int total, Move *m) {
    if (total != 4) return 0;
    int triple_rank = -1;
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        if (cnt[r] == 3) {
            triple_rank = r;
            break;
        }
    }
    if (triple_rank < 0) return 0;
    int remaining = 0;
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        if (r == triple_rank) continue;
        remaining += cnt[r];
    }
    if (remaining != 1) return 0;
    m->type = MOVE_TRIPLE_SINGLE;
    m->rank = triple_rank;
    m->length = 1;
    return 1;
}

static int try_triple_pair(const int cnt[RANK_COUNT_SIZE], const int total, Move *m) {
    if (total != 5) return 0;
    int triple_rank = -1;
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        if (cnt[r] == 3) {
            triple_rank = r;
            break;
        }
    }
    if (triple_rank < 0) return 0;
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        if (r == triple_rank) continue;
        if (cnt[r] == 2) {
            m->type = MOVE_TRIPLE_PAIR;
            m->rank = triple_rank;
            m->length = 1;
            return 1;
        }
    }
    return 0;
}

static int try_straight(const int cnt[RANK_COUNT_SIZE], const int total, Move *m) {
    if (total < 5) return 0;
    for (int r = RANK_2; r < RANK_COUNT_SIZE; r++) {
        if (cnt[r] > 0) return 0;
    }
    int start = -1, length = 0;
    for (int r = 0; r <= RANK_MAX_STRAIGHT; r++) {
        if (cnt[r] == 1) {
            if (start < 0) start = r;
            length++;
        } else if (cnt[r] == 0 && start >= 0) {
            break;
        } else if (cnt[r] > 1) {
            return 0;
        }
    }
    if (length != total || length < 5) return 0;
    m->type = MOVE_STRAIGHT;
    m->rank = start;
    m->length = length;
    return 1;
}

static int try_pair_straight(const int cnt[RANK_COUNT_SIZE], const int total, Move *m) {
    if (total < 6 || total % 2 != 0) return 0;
    for (int r = RANK_2; r < RANK_COUNT_SIZE; r++) {
        if (cnt[r] > 0) return 0;
    }
    int start = -1, length = 0;
    for (int r = 0; r <= RANK_MAX_STRAIGHT; r++) {
        if (cnt[r] == 2) {
            if (start < 0) start = r;
            length++;
        } else if (cnt[r] == 0 && start >= 0) {
            break;
        } else if (cnt[r] != 0) {
            return 0;
        }
    }
    if (length < 3 || length * 2 != total) return 0;
    m->type = MOVE_PAIR_STRAIGHT;
    m->rank = start;
    m->length = length;
    return 1;
}

/**
 * @brief Internal helper to find consecutive triples for airplanes.
 */
static int find_triple_run(const int cnt[RANK_COUNT_SIZE], int *out_start, int *out_len) {
    for (int r = RANK_2; r < RANK_COUNT_SIZE; r++) {
        if (cnt[r] > 0) return 0;
    }
    int start = -1, length = 0;
    for (int r = 0; r <= RANK_MAX_STRAIGHT; r++) {
        if (cnt[r] > 0) {
            if (start < 0) start = r;
            length++;
        } else if (start >= 0) {
            break;
        }
    }
    if (length < 2) return 0;
    *out_start = start;
    *out_len = length;
    return 1;
}

static int try_triple_straight(const int cnt[RANK_COUNT_SIZE], const int total, Move *m) {
    if (total < 6 || total % 3 != 0) return 0;
    int start, len;
    if (!find_triple_run(cnt, &start, &len)) return 0;
    if (len * 3 != total) return 0;
    m->type = MOVE_TRIPLE_STRAIGHT;
    m->rank = start;
    m->length = len;
    return 1;
}

static int try_triple_straight_singles(const int cnt[RANK_COUNT_SIZE], const int total, Move *m) {
    if (total < 8 || total % 4 != 0) return 0;
    const int n = total / 4;
    int tmp[RANK_COUNT_SIZE];
    memcpy(tmp, cnt, RANK_COUNT_SIZE * sizeof(int));
    int triple_cnt[RANK_COUNT_SIZE] = {0};
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        triple_cnt[r] = tmp[r] / 3;
        tmp[r] = tmp[r] % 3;
    }
    int start, len;
    if (!find_triple_run(triple_cnt, &start, &len) || len != n) return 0;
    int singles = 0;
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        if (tmp[r] > 1) return 0;
        singles += tmp[r];
    }
    if (singles != n) return 0;
    m->type = MOVE_TRIPLE_STRAIGHT_SINGLES;
    m->rank = start;
    m->length = len;
    return 1;
}

static int try_triple_straight_pairs(const int cnt[RANK_COUNT_SIZE], const int total, Move *m) {
    if (total < 10 || total % 5 != 0) return 0;
    const int n = total / 5;
    int tmp[RANK_COUNT_SIZE];
    memcpy(tmp, cnt, RANK_COUNT_SIZE * sizeof(int));
    int triple_cnt[RANK_COUNT_SIZE] = {0};
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        triple_cnt[r] = tmp[r] / 3;
        tmp[r] = tmp[r] % 3;
    }
    int start, len;
    if (!find_triple_run(triple_cnt, &start, &len) || len != n) return 0;
    int pairs = 0;
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        if (tmp[r] == 2) { pairs++; } else if (tmp[r] != 0) return 0;
    }
    if (pairs != n) return 0;
    m->type = MOVE_TRIPLE_STRAIGHT_PAIRS;
    m->rank = start;
    m->length = len;
    return 1;
}

static int try_four_two_singles(const int cnt[RANK_COUNT_SIZE], const int total, Move *m) {
    if (total != 6) return 0;
    int quad_rank = -1;
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        if (cnt[r] == 4) {
            quad_rank = r;
            break;
        }
    }
    if (quad_rank < 0) return 0;
    int remaining = 0;
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        if (r == quad_rank) continue;
        if (cnt[r] > 1) return 0;
        remaining += cnt[r];
    }
    if (remaining != 2) return 0;
    m->type = MOVE_FOUR_TWO_SINGLES;
    m->rank = quad_rank;
    m->length = 1;
    return 1;
}

static int try_four_two_pairs(const int cnt[RANK_COUNT_SIZE], const int total, Move *m) {
    if (total != 8) return 0;
    int quad_rank = -1;
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        if (cnt[r] == 4) {
            quad_rank = r;
            break;
        }
    }
    if (quad_rank < 0) return 0;
    int pairs = 0;
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        if (r == quad_rank) continue;
        if (cnt[r] == 2) pairs++;
        else if (cnt[r] != 0) return 0;
    }
    if (pairs != 2) return 0;
    m->type = MOVE_FOUR_TWO_PAIRS;
    m->rank = quad_rank;
    m->length = 1;
    return 1;
}

/* Public Classifications */

Move moves_classify_counts(const int cnt[RANK_COUNT_SIZE], const int total) {
    Move m = {0};
    m.type = MOVE_INVALID;
    m.count = total;

    if (total == 0) {
        m.type = MOVE_PASS;
        return m;
    }

    if (try_rocket(cnt, total, &m) ||
        try_bomb(cnt, total, &m) ||
        try_single(cnt, total, &m) ||
        try_pair(cnt, total, &m) ||
        try_triple(cnt, total, &m) ||
        try_triple_single(cnt, total, &m) ||
        try_triple_pair(cnt, total, &m) ||
        try_four_two_singles(cnt, total, &m) ||
        try_four_two_pairs(cnt, total, &m) ||
        try_straight(cnt, total, &m) ||
        try_pair_straight(cnt, total, &m) ||
        try_triple_straight(cnt, total, &m) ||
        try_triple_straight_singles(cnt, total, &m) ||
        try_triple_straight_pairs(cnt, total, &m)) {
        m.count = total;
    }

    return m;
}

Move moves_classify(const Card *cards, const int n) {
    int cnt[RANK_COUNT_SIZE];
    moves_count_ranks(cards, n, cnt);
    Move m = moves_classify_counts(cnt, n);
    if (n <= MOVE_MAX_CARDS) {
        memcpy(m.cards, cards, n * sizeof(Card));
    }
    m.count = n;
    return m;
}

/* Comparison Logic */

int moves_beats(const Move *play, const Move *prev) {
    if (play->type == MOVE_PASS || play->type == MOVE_INVALID) return 0;
    if (prev->type == MOVE_PASS) return 1;
    if (play->type == MOVE_ROCKET) return (prev->type != MOVE_ROCKET);

    if (play->type == MOVE_BOMB) {
        if (prev->type == MOVE_ROCKET) return 0;
        if (prev->type == MOVE_BOMB) return play->rank > prev->rank;
        return 1;
    }
    if (prev->type == MOVE_ROCKET || prev->type == MOVE_BOMB) return 0;

    if (play->type != prev->type || play->length != prev->length || play->count != prev->count) {
        return 0;
    }

    return play->rank > prev->rank;
}

/* Move Generation */

/**
 * @brief Logic to choose combinations of kicker ranks.
 */
typedef void (*kicker_cb)(const int chosen[], int k, void *ctx);

static void choose_kickers(const int cnt[RANK_COUNT_SIZE], const int need,
                           const int k, int chosen[], const int depth, const int start,
                           const kicker_cb cb, void *ctx) {
    if (depth == k) {
        cb(chosen, k, ctx);
        return;
    }
    for (int r = start; r < RANK_COUNT_SIZE; r++) {
        if (cnt[r] >= need) {
            chosen[depth] = r;
            choose_kickers(cnt, need, k, chosen, depth + 1, r + 1, cb, ctx);
        }
    }
}

/**
 * @struct KickerCtx
 * @brief Context for recursive kicker generation.
 */
typedef struct {
    int plane_start;
    int plane_len;
    int kicker_need;
    MoveType type;
    Move *out;
    int max_out;
    int *n;
    const int *cnt;
} KickerCtx;

static void on_kickers_chosen(const int chosen[], const int k, void *ctx_) {
    const KickerCtx *ctx = ctx_;
    int tmp[RANK_COUNT_SIZE] = {0};
    for (int r = ctx->plane_start; r < ctx->plane_start + ctx->plane_len; r++)
        tmp[r] = 3;
    for (int i = 0; i < k; i++)
        tmp[chosen[i]] += ctx->kicker_need;

    const int total = ctx->plane_len * 3 + k * ctx->kicker_need;
    Move m;
    m.type = ctx->type;
    m.rank = ctx->plane_start;
    m.length = ctx->plane_len;
    m.count = total;
    fill_cards_from_counts(tmp, m.cards);
    if (*ctx->n < ctx->max_out) ctx->out[*ctx->n] = m;
    (*ctx->n)++;
}

/* Generation sub-routines (Singles, Pairs, Triples, Bombs, Straights, etc.) */

static void gen_singles(const int cnt[RANK_COUNT_SIZE], const Move *prev, Move *out, const int max_out, int *n) {
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        if (cnt[r] < 1) continue;
        if (prev->type == MOVE_PASS || (r > prev->rank)) {
            Move m = {MOVE_SINGLE, {0}, 1, r, 1};
            int tmp[RANK_COUNT_SIZE] = {0};
            tmp[r] = 1;
            fill_cards_from_counts(tmp, m.cards);
            if (*n < max_out) out[*n] = m;
            (*n)++;
        }
    }
}

static void gen_pairs(const int cnt[RANK_COUNT_SIZE], const Move *prev, Move *out, const int max_out, int *n) {
    const int min_rank = (prev->type == MOVE_PAIR) ? prev->rank + 1 : 0;
    for (int r = min_rank; r < RANK_COUNT_SIZE; r++) {
        if (cnt[r] < 2) continue;
        Move m = {MOVE_PAIR, {0}, 2, r, 1};
        int tmp[RANK_COUNT_SIZE] = {0};
        tmp[r] = 2;
        fill_cards_from_counts(tmp, m.cards);
        if (*n < max_out) out[*n] = m;
        (*n)++;
    }
}

static void gen_triples(const int cnt[RANK_COUNT_SIZE], const Move *prev, Move *out, const int max_out, int *n) {
    const int min_rank = (prev->type == MOVE_TRIPLE) ? prev->rank + 1 : 0;
    for (int r = min_rank; r < RANK_COUNT_SIZE; r++) {
        if (cnt[r] < 3) continue;
        Move m = {MOVE_TRIPLE, {0}, 3, r, 1};
        int tmp[RANK_COUNT_SIZE] = {0};
        tmp[r] = 3;
        fill_cards_from_counts(tmp, m.cards);
        if (*n < max_out) out[*n] = m;
        (*n)++;
    }
}

static void gen_bombs(const int cnt[RANK_COUNT_SIZE], const Move *prev, Move *out, const int max_out, int *n) {
    const int min_rank = (prev->type == MOVE_BOMB) ? prev->rank + 1 : 0;
    for (int r = min_rank; r < RANK_COUNT_SIZE; r++) {
        if (cnt[r] < 4) continue;
        Move m = {MOVE_BOMB, {0}, 4, r, 1};
        int tmp[RANK_COUNT_SIZE] = {0};
        tmp[r] = 4;
        fill_cards_from_counts(tmp, m.cards);
        if (*n < max_out) out[*n] = m;
        (*n)++;
    }
}

static void gen_rocket(const int cnt[RANK_COUNT_SIZE], const Move *prev, Move *out, const int max_out, int *n) {
    if (prev->type == MOVE_ROCKET) return;
    if (cnt[RANK_SMALL_JOKER] >= 1 && cnt[RANK_BIG_JOKER] >= 1) {
        Move m = {MOVE_ROCKET, {0}, 2, RANK_BIG_JOKER, 1};
        int tmp[RANK_COUNT_SIZE] = {0};
        tmp[RANK_SMALL_JOKER] = 1;
        tmp[RANK_BIG_JOKER] = 1;
        fill_cards_from_counts(tmp, m.cards);
        if (*n < max_out) out[*n] = m;
        (*n)++;
    }
}

static void gen_straights(const int cnt[RANK_COUNT_SIZE], const Move *prev, Move *out, const int max_out, int *n) {
    const int req_len = (prev->type == MOVE_STRAIGHT) ? prev->length : 5;
    const int min_rank = (prev->type == MOVE_STRAIGHT) ? prev->rank : 0;
    const int len_max = (prev->type == MOVE_STRAIGHT) ? req_len : RANK_MAX_STRAIGHT + 1;
    for (int len = req_len; len <= len_max; len++) {
        const int start_min = (len == req_len && prev->type == MOVE_STRAIGHT) ? min_rank + 1 : 0;
        const int start_max = RANK_MAX_STRAIGHT - len + 1;
        for (int s = start_min; s <= start_max; s++) {
            int ok = 1;
            for (int r = s; r < s + len; r++) if (cnt[r] < 1) {
                ok = 0;
                break;
            }
            if (!ok) continue;
            Move m = {MOVE_STRAIGHT, {0}, len, s, len};
            int tmp[RANK_COUNT_SIZE] = {0};
            for (int r = s; r < s + len; r++) tmp[r] = 1;
            fill_cards_from_counts(tmp, m.cards);
            if (*n < max_out) out[*n] = m;
            (*n)++;
        }
    }
}

static void gen_pair_straights(const int cnt[RANK_COUNT_SIZE], const Move *prev, Move *out, const int max_out, int *n) {
    const int req_len = (prev->type == MOVE_PAIR_STRAIGHT) ? prev->length : 3;
    const int min_rank = (prev->type == MOVE_PAIR_STRAIGHT) ? prev->rank : 0;
    const int len_max = (prev->type == MOVE_PAIR_STRAIGHT) ? req_len : RANK_MAX_STRAIGHT + 1;
    for (int len = req_len; len <= len_max; len++) {
        const int start_min = (len == req_len && prev->type == MOVE_PAIR_STRAIGHT) ? min_rank + 1 : 0;
        const int start_max = RANK_MAX_STRAIGHT - len + 1;
        for (int s = start_min; s <= start_max; s++) {
            int ok = 1;
            for (int r = s; r < s + len; r++) if (cnt[r] < 2) {
                ok = 0;
                break;
            }
            if (!ok) continue;
            Move m = {MOVE_PAIR_STRAIGHT, {0}, len * 2, s, len};
            int tmp[RANK_COUNT_SIZE] = {0};
            for (int r = s; r < s + len; r++) tmp[r] = 2;
            fill_cards_from_counts(tmp, m.cards);
            if (*n < max_out) out[*n] = m;
            (*n)++;
        }
    }
}

static void gen_triple_straights(const int cnt[RANK_COUNT_SIZE], const Move *prev, Move *out, const int max_out,
                                 int *n) {
    const int req_len = (prev->type == MOVE_TRIPLE_STRAIGHT) ? prev->length : 2;
    const int min_rank = (prev->type == MOVE_TRIPLE_STRAIGHT) ? prev->rank : 0;
    const int len_max = (prev->type == MOVE_TRIPLE_STRAIGHT) ? req_len : RANK_MAX_STRAIGHT + 1;
    for (int len = req_len; len <= len_max; len++) {
        const int start_min = (len == req_len && prev->type == MOVE_TRIPLE_STRAIGHT) ? min_rank + 1 : 0;
        const int start_max = RANK_MAX_STRAIGHT - len + 1;
        for (int s = start_min; s <= start_max; s++) {
            int ok = 1;
            for (int r = s; r < s + len; r++) if (cnt[r] < 3) {
                ok = 0;
                break;
            }
            if (!ok) continue;
            Move m = {MOVE_TRIPLE_STRAIGHT, {0}, len * 3, s, len};
            int tmp[RANK_COUNT_SIZE] = {0};
            for (int r = s; r < s + len; r++) tmp[r] = 3;
            fill_cards_from_counts(tmp, m.cards);
            if (*n < max_out) out[*n] = m;
            (*n)++;
        }
    }
}

static void gen_triple_kicker(const int cnt[RANK_COUNT_SIZE], const Move *prev, const MoveType type,
                              const int kicker_need, Move *out, const int max_out, int *n) {
    const int req_len = (prev->type == type) ? prev->length : 1;
    const int min_rank = (prev->type == type) ? prev->rank : 0;
    const int max_len = RANK_MAX_STRAIGHT;
    for (int len = req_len; len <= max_len; len++) {
        const int start_min = (len == req_len && prev->type == type) ? min_rank + 1 : 0;
        const int start_max = RANK_MAX_STRAIGHT - len + 1;
        for (int s = start_min; s <= start_max; s++) {
            int ok = 1;
            for (int r = s; r < s + len; r++) if (cnt[r] < 3) {
                ok = 0;
                break;
            }
            if (!ok) continue;
            int rem[RANK_COUNT_SIZE];
            memcpy(rem, cnt, RANK_COUNT_SIZE * sizeof(int));
            for (int r = s; r < s + len; r++) rem[r] -= 3;
            KickerCtx ctx = {s, len, kicker_need, type, out, max_out, n, cnt};
            int chosen[20];
            choose_kickers(rem, kicker_need, len, chosen, 0, 0, on_kickers_chosen, &ctx);
        }
    }
}

static void gen_four_two(const int cnt[RANK_COUNT_SIZE], const Move *prev, const MoveType type, const int kicker_need,
                         Move *out, const int max_out, int *n) {
    const int min_rank = (prev->type == type) ? prev->rank + 1 : 0;
    for (int r = min_rank; r < RANK_COUNT_SIZE; r++) {
        if (cnt[r] < 4) continue;
        int rem[RANK_COUNT_SIZE];
        memcpy(rem, cnt, RANK_COUNT_SIZE * sizeof(int));
        rem[r] -= 4;
        for (int a = 0; a < RANK_COUNT_SIZE; a++) {
            if (rem[a] < kicker_need) continue;
            for (int b = a + 1; b < RANK_COUNT_SIZE; b++) {
                if (rem[b] < kicker_need) continue;
                Move m = {type, {0}, 4 + 2 * kicker_need, r, 1};
                int tmp[RANK_COUNT_SIZE] = {0};
                tmp[r] = 4;
                tmp[a] += kicker_need;
                tmp[b] += kicker_need;
                fill_cards_from_counts(tmp, m.cards);
                if (*n < max_out) out[*n] = m;
                (*n)++;
            }
        }
    }
}

static void gen_triple_single_move(const int cnt[RANK_COUNT_SIZE], const Move *prev, Move *out, const int max_out,
                                   int *n) {
    const int min_rank = (prev->type == MOVE_TRIPLE_SINGLE) ? prev->rank + 1 : 0;
    for (int r = min_rank; r < RANK_COUNT_SIZE; r++) {
        if (cnt[r] < 3) continue;
        int rem[RANK_COUNT_SIZE];
        memcpy(rem, cnt, RANK_COUNT_SIZE * sizeof(int));
        rem[r] -= 3;
        for (int k = 0; k < RANK_COUNT_SIZE; k++) {
            if (rem[k] < 1) continue;
            Move m = {MOVE_TRIPLE_SINGLE, {0}, 4, r, 1};
            int tmp[RANK_COUNT_SIZE] = {0};
            tmp[r] = 3;
            tmp[k] += 1;
            fill_cards_from_counts(tmp, m.cards);
            if (*n < max_out) out[*n] = m;
            (*n)++;
        }
    }
}

static void gen_triple_pair_move(const int cnt[RANK_COUNT_SIZE], const Move *prev, Move *out, const int max_out,
                                 int *n) {
    const int min_rank = (prev->type == MOVE_TRIPLE_PAIR) ? prev->rank + 1 : 0;
    for (int r = min_rank; r < RANK_COUNT_SIZE; r++) {
        if (cnt[r] < 3) continue;
        int rem[RANK_COUNT_SIZE];
        memcpy(rem, cnt, RANK_COUNT_SIZE * sizeof(int));
        rem[r] -= 3;
        for (int k = 0; k < RANK_COUNT_SIZE; k++) {
            if (rem[k] < 2) continue;
            Move m = {MOVE_TRIPLE_PAIR, {0}, 5, r, 1};
            int tmp[RANK_COUNT_SIZE] = {0};
            tmp[r] = 3;
            tmp[k] += 2;
            fill_cards_from_counts(tmp, m.cards);
            if (*n < max_out) out[*n] = m;
            (*n)++;
        }
    }
}

int moves_generate(const Card *hand, const int hand_size, const Move *prev, Move *out, const int max_out) {
    int cnt[RANK_COUNT_SIZE];
    moves_count_ranks(hand, hand_size, cnt);
    int n = 0;

    gen_bombs(cnt, prev, out, max_out, &n);
    gen_rocket(cnt, prev, out, max_out, &n);

    if (prev->type == MOVE_BOMB || prev->type == MOVE_ROCKET) return n;

    switch (prev->type) {
        case MOVE_PASS:
            gen_singles(cnt, prev, out, max_out, &n);
            gen_pairs(cnt, prev, out, max_out, &n);
            gen_triples(cnt, prev, out, max_out, &n);
            gen_triple_single_move(cnt, prev, out, max_out, &n);
            gen_triple_pair_move(cnt, prev, out, max_out, &n);
            gen_straights(cnt, prev, out, max_out, &n);
            gen_pair_straights(cnt, prev, out, max_out, &n);
            gen_triple_straights(cnt, prev, out, max_out, &n);
            gen_triple_kicker(cnt, prev, MOVE_TRIPLE_STRAIGHT_SINGLES, 1, out, max_out, &n);
            gen_triple_kicker(cnt, prev, MOVE_TRIPLE_STRAIGHT_PAIRS, 2, out, max_out, &n);
            gen_four_two(cnt, prev, MOVE_FOUR_TWO_SINGLES, 1, out, max_out, &n);
            gen_four_two(cnt, prev, MOVE_FOUR_TWO_PAIRS, 2, out, max_out, &n);
            break;
        case MOVE_SINGLE: gen_singles(cnt, prev, out, max_out, &n);
            break;
        case MOVE_PAIR: gen_pairs(cnt, prev, out, max_out, &n);
            break;
        case MOVE_TRIPLE: gen_triples(cnt, prev, out, max_out, &n);
            break;
        case MOVE_TRIPLE_SINGLE: gen_triple_single_move(cnt, prev, out, max_out, &n);
            break;
        case MOVE_TRIPLE_PAIR: gen_triple_pair_move(cnt, prev, out, max_out, &n);
            break;
        case MOVE_STRAIGHT: gen_straights(cnt, prev, out, max_out, &n);
            break;
        case MOVE_PAIR_STRAIGHT: gen_pair_straights(cnt, prev, out, max_out, &n);
            break;
        case MOVE_TRIPLE_STRAIGHT: gen_triple_straights(cnt, prev, out, max_out, &n);
            break;
        case MOVE_TRIPLE_STRAIGHT_SINGLES: gen_triple_kicker(cnt, prev, MOVE_TRIPLE_STRAIGHT_SINGLES, 1, out, max_out,
                                                             &n);
            break;
        case MOVE_TRIPLE_STRAIGHT_PAIRS: gen_triple_kicker(cnt, prev, MOVE_TRIPLE_STRAIGHT_PAIRS, 2, out, max_out, &n);
            break;
        case MOVE_FOUR_TWO_SINGLES: gen_four_two(cnt, prev, MOVE_FOUR_TWO_SINGLES, 1, out, max_out, &n);
            break;
        case MOVE_FOUR_TWO_PAIRS: gen_four_two(cnt, prev, MOVE_FOUR_TWO_PAIRS, 2, out, max_out, &n);
            break;
        default: break;
    }
    return n;
}

/* Util functions */

const char *moves_type_name(const MoveType type) {
    switch (type) {
        case MOVE_PASS: return "Pass";
        case MOVE_SINGLE: return "Single";
        case MOVE_PAIR: return "Pair";
        case MOVE_TRIPLE: return "Triple";
        case MOVE_TRIPLE_SINGLE: return "Triple+Single";
        case MOVE_TRIPLE_PAIR: return "Triple+Pair";
        case MOVE_STRAIGHT: return "Straight";
        case MOVE_PAIR_STRAIGHT: return "Pair Straight";
        case MOVE_TRIPLE_STRAIGHT: return "Airplane";
        case MOVE_TRIPLE_STRAIGHT_SINGLES: return "Airplane+Singles";
        case MOVE_TRIPLE_STRAIGHT_PAIRS: return "Airplane+Pairs";
        case MOVE_FOUR_TWO_SINGLES: return "Four+Two Singles";
        case MOVE_FOUR_TWO_PAIRS: return "Four+Two Pairs";
        case MOVE_BOMB: return "Bomb";
        case MOVE_ROCKET: return "Rocket";
        case MOVE_INVALID: return "Invalid";
        default: return "Unknown";
    }
}

void moves_sort(Move *m) {
    qsort(m->cards, m->count, sizeof(Card), cmp_card_rank);
}
