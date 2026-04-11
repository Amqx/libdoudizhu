/**
 * @file moves.c
 * @brief Implementation of libdoudizhu moves and card logic
 * @author Peng Yang Deng, Emma Le
 * @date 08-Mar-26
 */

#include "moves.h"
#include <stdlib.h>
#include "utils.h"

// Rank definitions
#define RANK_3 0
#define RANK_10 7
#define RANK_A 11
#define RANK_2 12
#define RANK_SMALL_JOKER 13
#define RANK_BIG_JOKER 14
#define RANK_MAX_STRAIGHT RANK_A

static int cmpCardRank(const void* a, const void* b) {
    const int ra = CARD_RANK(*(const Card*) a);
    const int rb = CARD_RANK(*(const Card*) b);
    return ra - rb;
}

void movesCountRanks(const Card cards[], const int n, int cnt[RANK_COUNT_SIZE]) {
    lddzMemset(cnt, 0, RANK_COUNT_SIZE * sizeof(int));
    for (int i = 0; i < n; i++)
        cnt[CARD_RANK(cards[i])]++;
}

/**
 * Internal helper to populate a Move's cards array from a rank-count array.
 * @param cnt Rank count array source
 * @param dst Normal card hand to populate
 */
static void fillCardsFromCounts(const int cnt[RANK_COUNT_SIZE], Card dst[MOVE_MAX_CARDS]) {
    int idx = 0;
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        int needed = cnt[r];
        if (needed <= 0)
            continue;
        // Standard ranks (3–2) have 4 suits; jokers are singletons at indices 52 and 53
        if (r < 13) {
            for (int suit = 0; suit < 4 && needed > 0; suit++) {
                dst[idx++] = (Card) (r * 4 + suit);
                needed--;
            }
        } else if (r == RANK_SMALL_JOKER) {
            dst[idx++] = 52;
        } else {
            dst[idx++] = 53;
        }
    }
}

static void assignActualCardsFromHand(const Card hand[], const int hand_size, Move *move) {
    if (hand == NULL || move == NULL || move->count <= 0)
        return;

    int needed[RANK_COUNT_SIZE];
    movesCountRanks(move->cards, move->count, needed);

    int idx = 0;
    for (int i = 0; i < hand_size && idx < move->count; i++) {
        const Card c = hand[i];
        const int rank = CARD_RANK(c);
        if (needed[rank] > 0) {
            move->cards[idx++] = c;
            needed[rank]--;
        }
    }
}

/* --- Classification Helpers --- */
/**
 * Attempts to classify the move as the lowest single
 * @param cnt Rank count hand
 * @param total Cards in hand
 * @param m Move to classify into
 * @return 1 if successful
 */
static int trySingle(const int cnt[RANK_COUNT_SIZE], const int total, Move* m) {
    if (total != 1)
        return 0;

    //find which card is it
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        //if found -> set move info
        if (cnt[r] == 1) {
            m->type = MOVE_SINGLE;
            m->rank = r;
            m->length = 1;
            return 1;
        }
    }
    return 0;
}

/**
 * Attempts to classify the move as the lowest pair
 * @param cnt Rank count hand
 * @param total Cards in hand
 * @param m Move to classify into
 * @return 1 if successful
 */
static int tryPair(const int cnt[RANK_COUNT_SIZE], const int total, Move* m) {
    if (total != 2)
        return 0;
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

/**
 * Attempts to classify the move as the lowest triple
 * @param cnt Rank count hand
 * @param total Cards in hand
 * @param m Move to classify into
 * @return 1 if successful
 */
static int tryTriple(const int cnt[RANK_COUNT_SIZE], const int total, Move* m) {
    if (total != 3)
        return 0;
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

/**
 * Attempts to classify the move as the lowest bomb
 * @param cnt Rank count hand
 * @param total Cards in hand
 * @param m Move to classify into
 * @return 1 if successful
 */
static int tryBomb(const int cnt[RANK_COUNT_SIZE], const int total, Move* m) {
    if (total != 4)
        return 0;
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

/**
 * Attempts to classify the move as a rocket
 * @param cnt Rank count hand
 * @param total Cards in hand
 * @param m Move to classify into
 * @return 1 if successful
 */
static int tryRocket(const int cnt[RANK_COUNT_SIZE], const int total, Move* m) {
    if (total != 2)
        return 0;
    if (cnt[RANK_SMALL_JOKER] == 1 && cnt[RANK_BIG_JOKER] == 1) {
        m->type = MOVE_ROCKET;
        m->rank = RANK_BIG_JOKER;
        m->length = 1;
        return 1;
    }
    return 0;
}

/**
 * Attempts to classify the move as a 3+1
 * @param cnt Rank count hand
 * @param total Cards in hand
 * @param m Move to classify into
 * @return 1 if successful
 */
static int tryTripleSingle(const int cnt[RANK_COUNT_SIZE], const int total, Move* m) {
    if (total != 4)
        return 0;
    int triple_rank = -1;
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        if (cnt[r] == 3) {
            triple_rank = r;
            break;
        }
    }
    if (triple_rank < 0)
        return 0;
    int remaining = 0;
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        if (r == triple_rank)
            continue;
        remaining += cnt[r];
    }
    if (remaining != 1)
        return 0;
    m->type = MOVE_TRIPLE_SINGLE;
    m->rank = triple_rank;
    m->length = 1;
    return 1;
}

/**
 * Attempts to classify the move as a 3+2
 * @param cnt Rank count hand
 * @param total Cards in hand
 * @param m Move to classify into
 * @return 1 if successful
 */
static int tryTriplePair(const int cnt[RANK_COUNT_SIZE], const int total, Move* m) {
    if (total != 5)
        return 0;
    int triple_rank = -1;
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        if (cnt[r] == 3) {
            triple_rank = r;
            break;
        }
    }
    if (triple_rank < 0)
        return 0;
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        if (r == triple_rank)
            continue;
        if (cnt[r] == 2) {
            m->type = MOVE_TRIPLE_PAIR;
            m->rank = triple_rank;
            m->length = 1;
            return 1;
        }
    }
    return 0;
}

/**
 * Attempts to classify the move as a straight
 * @param cnt Rank count hand
 * @param total Cards in hand
 * @param m Move to classify into
 * @return 1 if successful
 */
static int tryStraight(const int cnt[RANK_COUNT_SIZE], const int total, Move* m) {
    if (total < 5)
        return 0;
    for (int r = RANK_2; r < RANK_COUNT_SIZE; r++) {
        if (cnt[r] > 0)
            return 0;
    }
    int start = -1, length = 0;
    for (int r = 0; r <= RANK_MAX_STRAIGHT; r++) {
        if (cnt[r] == 1) {
            if (start < 0)
                start = r;
            length++;
        } else if (cnt[r] == 0 && start >= 0) {
            break;
        } else if (cnt[r] > 1) {
            return 0;
        }
    }
    if (length != total || length < 5)
        return 0;
    m->type = MOVE_STRAIGHT;
    m->rank = start;
    m->length = length;
    return 1;
}

/**
 * Attempts to classify the move as an N pair straight
 * @param cnt Rank count hand
 * @param total Cards in hand
 * @param m Move to classify into
 * @return 1 if successful
 */
static int tryPairStraight(const int cnt[RANK_COUNT_SIZE], const int total, Move* m) {
    if (total < 6 || total % 2 != 0)
        return 0;
    for (int r = RANK_2; r < RANK_COUNT_SIZE; r++) {
        if (cnt[r] > 0)
            return 0;
    }
    int start = -1, length = 0;
    for (int r = 0; r <= RANK_MAX_STRAIGHT; r++) {
        if (cnt[r] == 2) {
            if (start < 0)
                start = r;
            length++;
        } else if (cnt[r] == 0 && start >= 0) {
            break;
        } else if (cnt[r] != 0) {
            return 0;
        }
    }
    if (length < 3 || length * 2 != total)
        return 0;
    m->type = MOVE_PAIR_STRAIGHT;
    m->rank = start;
    m->length = length;
    return 1;
}

/**
 * Internal helper to find consecutive triples.
 * @param cnt Cards in hand
 * @param out_start Starting position of the plane
 * @param out_len Length of the plane
 */
static int findTripleRun(const int cnt[RANK_COUNT_SIZE], int* out_start, int* out_len) {
    for (int r = RANK_2; r < RANK_COUNT_SIZE; r++) {
        if (cnt[r] > 0)
            return 0;
    }
    int start = -1, length = 0;
    for (int r = 0; r <= RANK_MAX_STRAIGHT; r++) {
        if (cnt[r] > 0) {
            if (start < 0)
                start = r;
            length++;
        } else if (start >= 0) {
            break;
        }
    }
    if (length < 2)
        return 0;
    *out_start = start;
    *out_len = length;
    return 1;
}

/**
 * Attempts to classify the move as an N triplet straight
 * @param cnt Rank count hand
 * @param total Cards in hand
 * @param m Move to classify into
 * @return 1 if successful
 */
static int tryTripleStraight(const int cnt[RANK_COUNT_SIZE], const int total, Move* m) {
    if (total < 6 || total % 3 != 0)
        return 0;
    int triple_cnt[RANK_COUNT_SIZE] = {0};
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        if (cnt[r] % 3 != 0)
            return 0;
        triple_cnt[r] = cnt[r] / 3;
    }
    int start, len;
    if (!findTripleRun(triple_cnt, &start, &len))
        return 0;
    if (len * 3 != total)
        return 0;
    m->type = MOVE_TRIPLE_STRAIGHT;
    m->rank = start;
    m->length = len;
    return 1;
}

/**
 * Attempts to classify the move as an N triplet straight and N singles
 * @param cnt Rank count hand
 * @param total Cards in hand
 * @param m Move to classify into
 * @return 1 if successful
 */
static int tryTripleStraightSingles(const int cnt[RANK_COUNT_SIZE], const int total, Move* m) {
    if (total < 8 || total % 4 != 0)
        return 0;
    const int n = total / 4;
    int tmp[RANK_COUNT_SIZE];
    lddzMemcpy(tmp, cnt, RANK_COUNT_SIZE * sizeof(int));
    int triple_cnt[RANK_COUNT_SIZE] = {0};
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        triple_cnt[r] = tmp[r] / 3;
        tmp[r] = tmp[r] % 3;
    }
    int start, len;
    if (!findTripleRun(triple_cnt, &start, &len) || len != n)
        return 0;
    int singles = 0;
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        if (tmp[r] > 1)
            return 0;
        singles += tmp[r];
    }
    if (singles != n)
        return 0;
    m->type = MOVE_TRIPLE_STRAIGHT_SINGLES;
    m->rank = start;
    m->length = len;
    return 1;
}

/**
 * Attempts to classify the move as an N triplet straight and N pairs
 * @param cnt Rank count hand
 * @param total Cards in hand
 * @param m Move to classify into
 * @return 1 if successful
 */
static int tryTripleStraightPairs(const int cnt[RANK_COUNT_SIZE], const int total, Move* m) {
    if (total < 10 || total % 5 != 0)
        return 0;
    const int n = total / 5;
    int tmp[RANK_COUNT_SIZE];
    lddzMemcpy(tmp, cnt, RANK_COUNT_SIZE * sizeof(int));
    int triple_cnt[RANK_COUNT_SIZE] = {0};
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        triple_cnt[r] = tmp[r] / 3;
        tmp[r] = tmp[r] % 3;
    }
    int start, len;
    if (!findTripleRun(triple_cnt, &start, &len) || len != n)
        return 0;
    int pairs = 0;
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        if (tmp[r] == 2) {
            pairs++;
        } else if (tmp[r] != 0)
            return 0;
    }
    if (pairs != n)
        return 0;
    m->type = MOVE_TRIPLE_STRAIGHT_PAIRS;
    m->rank = start;
    m->length = len;
    return 1;
}

/**
 * Attempts to classify the move as a quad + 2 singles
 * @param cnt Rank count hand
 * @param total Cards in hand
 * @param m Move to classify into
 * @return 1 if successful
 */
static int tryFourTwoSingles(const int cnt[RANK_COUNT_SIZE], const int total, Move* m) {
    if (total != 6)
        return 0;
    int quad_rank = -1;
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        if (cnt[r] == 4) {
            quad_rank = r;
            break;
        }
    }
    if (quad_rank < 0)
        return 0;
    int remaining = 0;
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        if (r == quad_rank)
            continue;
        if (cnt[r] > 1)
            return 0;
        remaining += cnt[r];
    }
    if (remaining != 2)
        return 0;
    m->type = MOVE_FOUR_TWO_SINGLES;
    m->rank = quad_rank;
    m->length = 1;
    return 1;
}

/**
 * Attempts to classify the move as an N triplet straight + 2 pairs
 * @param cnt Rank count hand
 * @param total Cards in hand
 * @param m Move to classify into
 * @return 1 if successful
 */
static int tryFourTwoPairs(const int cnt[RANK_COUNT_SIZE], const int total, Move* m) {
    if (total != 8)
        return 0;
    int quad_rank = -1;
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        if (cnt[r] == 4) {
            quad_rank = r;
            break;
        }
    }
    if (quad_rank < 0)
        return 0;
    int pairs = 0;
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        if (r == quad_rank)
            continue;
        if (cnt[r] == 2)
            pairs++;
        else if (cnt[r] != 0)
            return 0;
    }
    if (pairs != 2)
        return 0;
    m->type = MOVE_FOUR_TWO_PAIRS;
    m->rank = quad_rank;
    m->length = 1;
    return 1;
}

/* --- Public Classification Functions --- */

// Classify a hand into a move type (single, pair, straight, etc.)
Move movesClassifyCounts(const int cnt[RANK_COUNT_SIZE], const int total) {
    Move m = {0};
    m.type = MOVE_INVALID;
    m.count = total;

    if (total == 0) {
        m.type = MOVE_PASS;
        return m;
    }

    if (tryRocket(cnt, total, &m) || tryBomb(cnt, total, &m) || trySingle(cnt, total, &m) || tryPair(cnt, total, &m) ||
        tryTriple(cnt, total, &m) || tryTripleSingle(cnt, total, &m) || tryTriplePair(cnt, total, &m) ||
        tryFourTwoSingles(cnt, total, &m) || tryFourTwoPairs(cnt, total, &m) || tryStraight(cnt, total, &m) ||
        tryPairStraight(cnt, total, &m) || tryTripleStraight(cnt, total, &m) ||
        tryTripleStraightSingles(cnt, total, &m) || tryTripleStraightPairs(cnt, total, &m)) {
        m.count = total;
    }

    return m;
}

// Classify into an actual move; cards in → move description out
Move movesClassify(const Card cards[], const int n) {
    int cnt[RANK_COUNT_SIZE];
    movesCountRanks(cards, n, cnt);
    Move m = movesClassifyCounts(cnt, n);
    if (n <= MOVE_MAX_CARDS) {
        lddzMemcpy(m.cards, cards, n * sizeof(Card));
    }
    m.count = n;
    return m;
}

/* --- Comparison Logic --- */

// Checks if play beats prev (returns 1 = yes, 0 = no)
// play is your move and prev is previous move on the table
int movesBeats(const Move* play, const Move* prev) {
    // If you played nothing or invalid move -> cannot win
    if (play->type == MOVE_PASS || play->type == MOVE_INVALID)
        return 0;

    // If previous player passed -> you automatically win
    if (prev->type == MOVE_PASS)
        return 1;

    // Rocket beats everything except another rocket
    if (play->type == MOVE_ROCKET)
        return (prev->type != MOVE_ROCKET);

    // Handle bomb logic
    if (play->type == MOVE_BOMB) {
        // Bomb cannot beat rocket
        if (prev->type == MOVE_ROCKET)
            return 0;
        // Bomb vs bomb -> higher rank wins
        if (prev->type == MOVE_BOMB)
            return play->rank > prev->rank;
        // Bomb beats all other normal moves
        return 1;
    }

    // Normal moves cannot beat bomb or rocket
    if (prev->type == MOVE_ROCKET || prev->type == MOVE_BOMB)
        return 0;

    // Must have same type, length, and number of cards to compare
    if (play->type != prev->type || play->length != prev->length || play->count != prev->count) {
        return 0;
    }

    // Same type → higher rank wins
    return play->rank > prev->rank;
}

/* --- Move Generation --- */
/**
 * Logic to choose combinations of kicker ranks.
 * @param chosen Array of chosen kickers
 * @param k Length of the array
 * @param ctx Context pointer
 */
typedef void (*kicker_cb)(const int chosen[], int k, const void* ctx);

/**
 * It finds all ways to choose k ranks (kickers) from your hand and calls a function for each combination
 * @param cnt Rank count hand
 * @param need Number of kickers needed
 * @param k Current depth
 * @param chosen Array of chosen kickers
 * @param depth Depth to search
 * @param start Starting depth
 * @param cb Callback function for choosing kickers
 * @param ctx Context pointer
 */


static void chooseKickers(const int cnt[RANK_COUNT_SIZE], const int need, const int k, int chosen[], const int depth,
                          const int start, const kicker_cb cb, void* ctx) {
    if (depth == k) {
        cb(chosen, k, ctx);
        return;
    }
    for (int r = start; r < RANK_COUNT_SIZE; r++) {
        if (cnt[r] >= need) {
            chosen[depth] = r;
            chooseKickers(cnt, need, k, chosen, depth + 1, r + 1, cb, ctx);
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
    Move* out;
    int max_out;
    int* n;
    const int* cnt;
} KickerCtx;

/**
 * Creates the kicker-typed move once it's been chosen.
 * @param chosen Available kickers
 * @param k Number of kickers to take
 * @param ctx_ Context pointer
 */
static void onKickersChosen(const int chosen[], const int k, const void* ctx_) {
    const KickerCtx* ctx = ctx_;
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
    fillCardsFromCounts(tmp, m.cards);
    if (*ctx->n < ctx->max_out)
        ctx->out[*ctx->n] = m;
    (*ctx->n)++;
}

/* --- Generation Subroutines --- */
/**
 * Generate all valid single-card moves that can beat the previous move
 * @param cnt Rank count hand
 * @param prev Previously played move
 * @param out Out array for moves
 * @param max_out Max number of moves to generate
 * @param n Number of moves currently
 */
static void genSingles(const int cnt[RANK_COUNT_SIZE], const Move* prev, Move out[], const int max_out, int* n) {
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        if (cnt[r] < 1)
            continue;
        if (prev->type == MOVE_PASS || (r > prev->rank)) {
            Move m = {MOVE_SINGLE, {0}, 1, r, 1};
            int tmp[RANK_COUNT_SIZE] = {0};
            tmp[r] = 1;
            fillCardsFromCounts(tmp, m.cards);
            if (*n < max_out)
                out[*n] = m;
            (*n)++;
        }
    }
}

/**
 * Generate all valid pair-card moves that can beat the previous move
 * @param cnt Rank count hand
 * @param prev Previously played move
 * @param out Out array for moves
 * @param max_out Max number of moves to generate
 * @param n Number of moves currently
 */
static void genPairs(const int cnt[RANK_COUNT_SIZE], const Move* prev, Move out[], const int max_out, int* n) {
    const int min_rank = (prev->type == MOVE_PAIR) ? prev->rank + 1 : 0;
    for (int r = min_rank; r < RANK_COUNT_SIZE; r++) {
        if (cnt[r] < 2)
            continue;
        Move m = {MOVE_PAIR, {0}, 2, r, 1};
        int tmp[RANK_COUNT_SIZE] = {0};
        tmp[r] = 2;
        fillCardsFromCounts(tmp, m.cards);
        if (*n < max_out)
            out[*n] = m;
        (*n)++;
    }
}

/**
 * Generate all valid triples-card moves that can beat the previous move
 * @param cnt Rank count hand
 * @param prev Previously played move
 * @param out Out array for moves
 * @param max_out Max number of moves to generate
 * @param n Number of moves currently
 */
static void genTriples(const int cnt[RANK_COUNT_SIZE], const Move* prev, Move out[], const int max_out, int* n) {
    const int min_rank = (prev->type == MOVE_TRIPLE) ? prev->rank + 1 : 0;
    for (int r = min_rank; r < RANK_COUNT_SIZE; r++) {
        if (cnt[r] < 3)
            continue;
        Move m = {MOVE_TRIPLE, {0}, 3, r, 1};
        int tmp[RANK_COUNT_SIZE] = {0};
        tmp[r] = 3;
        fillCardsFromCounts(tmp, m.cards);
        if (*n < max_out)
            out[*n] = m;
        (*n)++;
    }
}

/**
 * Generate all valid bombs moves that can beat the previous move
 * @param cnt Rank count hand
 * @param prev Previously played move
 * @param out Out array for moves
 * @param max_out Max number of moves to generate
 * @param n Number of moves currently
 */
static void genBombs(const int cnt[RANK_COUNT_SIZE], const Move* prev, Move out[], const int max_out, int* n) {
    const int min_rank = (prev->type == MOVE_BOMB) ? prev->rank + 1 : 0;
    for (int r = min_rank; r < RANK_COUNT_SIZE; r++) {
        if (cnt[r] < 4)
            continue;
        Move m = {MOVE_BOMB, {0}, 4, r, 1};
        int tmp[RANK_COUNT_SIZE] = {0};
        tmp[r] = 4;
        fillCardsFromCounts(tmp, m.cards);
        if (*n < max_out)
            out[*n] = m;
        (*n)++;
    }
}

/**
 * Generates a rocket if it's in hand.
 * @param cnt Rank count hand
 * @param prev Previously played move
 * @param out Out array for moves
 * @param max_out Max number of moves to generate
 * @param n Number of moves currently
 */
static void genRocket(const int cnt[RANK_COUNT_SIZE], const Move* prev, Move out[], const int max_out, int* n) {
    if (prev->type == MOVE_ROCKET)
        return;
    if (cnt[RANK_SMALL_JOKER] >= 1 && cnt[RANK_BIG_JOKER] >= 1) {
        Move m = {MOVE_ROCKET, {0}, 2, RANK_BIG_JOKER, 1};
        int tmp[RANK_COUNT_SIZE] = {0};
        tmp[RANK_SMALL_JOKER] = 1;
        tmp[RANK_BIG_JOKER] = 1;
        fillCardsFromCounts(tmp, m.cards);
        if (*n < max_out)
            out[*n] = m;
        (*n)++;
    }
}

/**
 * Generates all valid, playable straights.
 * @param cnt Rank count hand
 * @param prev Previously played move
 * @param out Out array for moves
 * @param max_out Max number of moves to generate
 * @param n Number of moves currently
 */
static void genStraights(const int cnt[RANK_COUNT_SIZE], const Move* prev, Move out[], const int max_out, int* n) {
    const int req_len = (prev->type == MOVE_STRAIGHT) ? prev->length : 5;
    const int min_rank = (prev->type == MOVE_STRAIGHT) ? prev->rank : 0;
    const int len_max = (prev->type == MOVE_STRAIGHT) ? req_len : RANK_MAX_STRAIGHT + 1;
    for (int len = req_len; len <= len_max; len++) {
        const int start_min = (len == req_len && prev->type == MOVE_STRAIGHT) ? min_rank + 1 : 0;
        const int start_max = RANK_MAX_STRAIGHT - len + 1;
        for (int s = start_min; s <= start_max; s++) {
            int ok = 1;
            for (int r = s; r < s + len; r++)
                if (cnt[r] < 1) {
                    ok = 0;
                    break;
                }
            if (!ok)
                continue;
            Move m = {MOVE_STRAIGHT, {0}, len, s, len};
            int tmp[RANK_COUNT_SIZE] = {0};
            for (int r = s; r < s + len; r++)
                tmp[r] = 1;
            fillCardsFromCounts(tmp, m.cards);
            if (*n < max_out)
                out[*n] = m;
            (*n)++;
        }
    }
}

/**
 * Generates all valid, playable pair straights.
 * @param cnt Rank count hand
 * @param prev Previously played move
 * @param out Out array for moves
 * @param max_out Max number of moves to generate
 * @param n Number of moves currently
 */
static void genPairStraights(const int cnt[RANK_COUNT_SIZE], const Move* prev, Move out[], const int max_out, int* n) {
    const int req_len = (prev->type == MOVE_PAIR_STRAIGHT) ? prev->length : 3;
    const int min_rank = (prev->type == MOVE_PAIR_STRAIGHT) ? prev->rank : 0;
    const int len_max = (prev->type == MOVE_PAIR_STRAIGHT) ? req_len : RANK_MAX_STRAIGHT + 1;
    for (int len = req_len; len <= len_max; len++) {
        const int start_min = (len == req_len && prev->type == MOVE_PAIR_STRAIGHT) ? min_rank + 1 : 0;
        const int start_max = RANK_MAX_STRAIGHT - len + 1;
        for (int s = start_min; s <= start_max; s++) {
            int ok = 1;
            for (int r = s; r < s + len; r++)
                if (cnt[r] < 2) {
                    ok = 0;
                    break;
                }
            if (!ok)
                continue;
            Move m = {MOVE_PAIR_STRAIGHT, {0}, len * 2, s, len};
            int tmp[RANK_COUNT_SIZE] = {0};
            for (int r = s; r < s + len; r++)
                tmp[r] = 2;
            fillCardsFromCounts(tmp, m.cards);
            if (*n < max_out)
                out[*n] = m;
            (*n)++;
        }
    }
}

/**
 * Generates all valid, playable triplet straights.
 * @param cnt Rank count hand
 * @param prev Previously played move
 * @param out Out array for moves
 * @param max_out Max number of moves to generate
 * @param n Number of moves currently
 */
static void genTripleStraights(const int cnt[RANK_COUNT_SIZE], const Move* prev, Move out[], const int max_out,
                               int* n) {
    const int req_len = (prev->type == MOVE_TRIPLE_STRAIGHT) ? prev->length : 2;
    const int min_rank = (prev->type == MOVE_TRIPLE_STRAIGHT) ? prev->rank : 0;
    const int len_max = (prev->type == MOVE_TRIPLE_STRAIGHT) ? req_len : RANK_MAX_STRAIGHT + 1;
    for (int len = req_len; len <= len_max; len++) {
        const int start_min = (len == req_len && prev->type == MOVE_TRIPLE_STRAIGHT) ? min_rank + 1 : 0;
        const int start_max = RANK_MAX_STRAIGHT - len + 1;
        for (int s = start_min; s <= start_max; s++) {
            int ok = 1;
            for (int r = s; r < s + len; r++)
                if (cnt[r] < 3) {
                    ok = 0;
                    break;
                }
            if (!ok)
                continue;
            Move m = {MOVE_TRIPLE_STRAIGHT, {0}, len * 3, s, len};
            int tmp[RANK_COUNT_SIZE] = {0};
            for (int r = s; r < s + len; r++)
                tmp[r] = 3;
            fillCardsFromCounts(tmp, m.cards);
            if (*n < max_out)
                out[*n] = m;
            (*n)++;
        }
    }
}

/**
 * Generates all valid, playable 3 + n's.
 * @param cnt Rank count hand
 * @param prev Previously played move
 * @param type Type of 3 + n to generate
 * @param kicker_need Number of kickers needed
 * @param out Out array for moves
 * @param max_out Max number of moves to generate
 * @param n Number of moves currently
 */
static void genTripleKicker(const int cnt[RANK_COUNT_SIZE], const Move* prev, const MoveType type,
                            const int kicker_need, Move out[], const int max_out, int* n) {
    const int req_len = (prev->type == type) ? prev->length : 2;
    const int min_rank = (prev->type == type) ? prev->rank : 0;
    const int max_len = RANK_MAX_STRAIGHT;
    for (int len = req_len; len <= max_len; len++) {
        const int start_min = (len == req_len && prev->type == type) ? min_rank + 1 : 0;
        const int start_max = RANK_MAX_STRAIGHT - len + 1;
        for (int s = start_min; s <= start_max; s++) {
            int ok = 1;
            for (int r = s; r < s + len; r++)
                if (cnt[r] < 3) {
                    ok = 0;
                    break;
                }
            if (!ok)
                continue;
            int rem[RANK_COUNT_SIZE];
            lddzMemcpy(rem, cnt, RANK_COUNT_SIZE * sizeof(int));
            for (int r = s; r < s + len; r++)
                rem[r] = 0;
            KickerCtx ctx = {s, len, kicker_need, type, out, max_out, n, cnt};
            int chosen[20];
            chooseKickers(rem, kicker_need, len, chosen, 0, 0, onKickersChosen, &ctx);
        }
    }
}

/**
 * Generates all valid, playable 4 + 2n's.
 * @param cnt Rank count hand
 * @param prev Previously played move
 * @param type Type of 4 + 2n to generate
 * @param kicker_need Number of kickers needed
 * @param out Out array for moves
 * @param max_out Max number of moves to generate
 * @param n Number of moves currently
 */
static void genFourTwo(const int cnt[RANK_COUNT_SIZE], const Move* prev, const MoveType type, const int kicker_need,
                       Move out[], const int max_out, int* n) {
    const int min_rank = (prev->type == type) ? prev->rank + 1 : 0;
    for (int r = min_rank; r < RANK_COUNT_SIZE; r++) {
        if (cnt[r] < 4)
            continue;
        int rem[RANK_COUNT_SIZE];
        lddzMemcpy(rem, cnt, RANK_COUNT_SIZE * sizeof(int));
        rem[r] -= 4;
        for (int a = 0; a < RANK_COUNT_SIZE; a++) {
            if (rem[a] < kicker_need)
                continue;
            for (int b = a + 1; b < RANK_COUNT_SIZE; b++) {
                if (rem[b] < kicker_need)
                    continue;
                Move m = {type, {0}, 4 + 2 * kicker_need, r, 1};
                int tmp[RANK_COUNT_SIZE] = {0};
                tmp[r] = 4;
                tmp[a] += kicker_need;
                tmp[b] += kicker_need;
                fillCardsFromCounts(tmp, m.cards);
                if (*n < max_out)
                    out[*n] = m;
                (*n)++;
            }
        }
    }
}

/**
 * Generates all valid, playable triples.
 * @param cnt Rank count hand
 * @param prev Previously played move
 * @param out Out array for moves
 * @param max_out Max number of moves to generate
 * @param n Number of moves currently
 */
static void genTripleSingleMove(const int cnt[RANK_COUNT_SIZE], const Move* prev, Move out[], const int max_out,
                                int* n) {
    const int min_rank = (prev->type == MOVE_TRIPLE_SINGLE) ? prev->rank + 1 : 0;
    for (int r = min_rank; r < RANK_COUNT_SIZE; r++) {
        if (cnt[r] < 3)
            continue;
        int rem[RANK_COUNT_SIZE];
        lddzMemcpy(rem, cnt, RANK_COUNT_SIZE * sizeof(int));
        rem[r] -= 3;
        for (int k = 0; k < RANK_COUNT_SIZE; k++) {
            if (k == r)
                continue;
            if (rem[k] < 1)
                continue;
            Move m = {MOVE_TRIPLE_SINGLE, {0}, 4, r, 1};
            int tmp[RANK_COUNT_SIZE] = {0};
            tmp[r] = 3;
            tmp[k] += 1;
            fillCardsFromCounts(tmp, m.cards);
            if (*n < max_out)
                out[*n] = m;
            (*n)++;
        }
    }
}

/**
 * Generates all valid, playable triple pairs.
 * @param cnt Rank count hand
 * @param prev Previously played move
 * @param out Out array for moves
 * @param max_out Max number of moves to generate
 * @param n Number of moves currently
 */
static void genTriplePairMove(const int cnt[RANK_COUNT_SIZE], const Move* prev, Move out[], const int max_out, int* n) {
    const int min_rank = (prev->type == MOVE_TRIPLE_PAIR) ? prev->rank + 1 : 0;
    for (int r = min_rank; r < RANK_COUNT_SIZE; r++) {
        if (cnt[r] < 3)
            continue;
        int rem[RANK_COUNT_SIZE];
        lddzMemcpy(rem, cnt, RANK_COUNT_SIZE * sizeof(int));
        rem[r] -= 3;
        for (int k = 0; k < RANK_COUNT_SIZE; k++) {
            if (k == r)
                continue;
            if (rem[k] < 2)
                continue;
            Move m = {MOVE_TRIPLE_PAIR, {0}, 5, r, 1};
            int tmp[RANK_COUNT_SIZE] = {0};
            tmp[r] = 3;
            tmp[k] += 2;
            fillCardsFromCounts(tmp, m.cards);
            if (*n < max_out)
                out[*n] = m;
            (*n)++;
        }
    }
}

/**
 * Generates all valid, playable moves.
 * @param hand Available cards
 * @param hand_size Number of cards in hand
 * @param prev Previously played move
 * @param out Out array for moves
 * @param max_out Max number of moves to generate
 */
int movesGenerate(const Card hand[], const int hand_size, const Move* prev, Move out[], const int max_out) {
    int cnt[RANK_COUNT_SIZE];
    movesCountRanks(hand, hand_size, cnt);
    int n = 0;

    genBombs(cnt, prev, out, max_out, &n);
    genRocket(cnt, prev, out, max_out, &n);

    if (prev->type == MOVE_BOMB || prev->type == MOVE_ROCKET)
        return n;

    switch (prev->type) {
        case MOVE_PASS:
            genSingles(cnt, prev, out, max_out, &n);
            genPairs(cnt, prev, out, max_out, &n);
            genTriples(cnt, prev, out, max_out, &n);
            genTripleSingleMove(cnt, prev, out, max_out, &n);
            genTriplePairMove(cnt, prev, out, max_out, &n);
            genStraights(cnt, prev, out, max_out, &n);
            genPairStraights(cnt, prev, out, max_out, &n);
            genTripleStraights(cnt, prev, out, max_out, &n);
            genTripleKicker(cnt, prev, MOVE_TRIPLE_STRAIGHT_SINGLES, 1, out, max_out, &n);
            genTripleKicker(cnt, prev, MOVE_TRIPLE_STRAIGHT_PAIRS, 2, out, max_out, &n);
            genFourTwo(cnt, prev, MOVE_FOUR_TWO_SINGLES, 1, out, max_out, &n);
            genFourTwo(cnt, prev, MOVE_FOUR_TWO_PAIRS, 2, out, max_out, &n);
            break;
        case MOVE_SINGLE:
            genSingles(cnt, prev, out, max_out, &n);
            break;
        case MOVE_PAIR:
            genPairs(cnt, prev, out, max_out, &n);
            break;
        case MOVE_TRIPLE:
            genTriples(cnt, prev, out, max_out, &n);
            break;
        case MOVE_TRIPLE_SINGLE:
            genTripleSingleMove(cnt, prev, out, max_out, &n);
            break;
        case MOVE_TRIPLE_PAIR:
            genTriplePairMove(cnt, prev, out, max_out, &n);
            break;
        case MOVE_STRAIGHT:
            genStraights(cnt, prev, out, max_out, &n);
            break;
        case MOVE_PAIR_STRAIGHT:
            genPairStraights(cnt, prev, out, max_out, &n);
            break;
        case MOVE_TRIPLE_STRAIGHT:
            genTripleStraights(cnt, prev, out, max_out, &n);
            break;
        case MOVE_TRIPLE_STRAIGHT_SINGLES:
            genTripleKicker(cnt, prev, MOVE_TRIPLE_STRAIGHT_SINGLES, 1, out, max_out, &n);
            break;
        case MOVE_TRIPLE_STRAIGHT_PAIRS:
            genTripleKicker(cnt, prev, MOVE_TRIPLE_STRAIGHT_PAIRS, 2, out, max_out, &n);
            break;
        case MOVE_FOUR_TWO_SINGLES:
            genFourTwo(cnt, prev, MOVE_FOUR_TWO_SINGLES, 1, out, max_out, &n);
            break;
        case MOVE_FOUR_TWO_PAIRS:
            genFourTwo(cnt, prev, MOVE_FOUR_TWO_PAIRS, 2, out, max_out, &n);
            break;
        default:
            break;
    }

    const int written = (n < max_out) ? n : max_out;
    for (int i = 0; i < written; i++)
        assignActualCardsFromHand(hand, hand_size, &out[i]);

    return n;
}

const char* movesTypeName(const MoveType type) {
    switch (type) {
        case MOVE_PASS:
            return "Pass";
        case MOVE_SINGLE:
            return "Single";
        case MOVE_PAIR:
            return "Pair";
        case MOVE_TRIPLE:
            return "Triple";
        case MOVE_TRIPLE_SINGLE:
            return "Triple+Single";
        case MOVE_TRIPLE_PAIR:
            return "Triple+Pair";
        case MOVE_STRAIGHT:
            return "Straight";
        case MOVE_PAIR_STRAIGHT:
            return "Pair Straight";
        case MOVE_TRIPLE_STRAIGHT:
            return "Airplane";
        case MOVE_TRIPLE_STRAIGHT_SINGLES:
            return "Airplane+Singles";
        case MOVE_TRIPLE_STRAIGHT_PAIRS:
            return "Airplane+Pairs";
        case MOVE_FOUR_TWO_SINGLES:
            return "Four+Two Singles";
        case MOVE_FOUR_TWO_PAIRS:
            return "Four+Two Pairs";
        case MOVE_BOMB:
            return "Bomb";
        case MOVE_ROCKET:
            return "Rocket";
        case MOVE_INVALID:
            return "Invalid";
        default:
            return "Unknown";
    }
}

void movesSort(Move* m) { qsort(m->cards, m->count, sizeof(Card), cmpCardRank); }
