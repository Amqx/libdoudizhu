/**
 * @file bot.c
 * @brief Rule-based AI implementation for libdoudizhu
 * @author Jonathan
 * @date 08-Mar-26
 */

#include "bot.h"
#include "eval.h"
#include <string.h>

#define BOT_MAX_MOVES 512

/* ---------------------------------------------------------------------------
 * BotWeights – default values
 *
 * All scoring magic numbers live here.  Swap this struct out to tune the bot
 * without touching any logic.
 * --------------------------------------------------------------------------- */

static const BotWeights DEFAULT_WEIGHTS = {
    /* Leading */
    .clear_per_card = 15,
    .endgame_clear = 10,
    .chain_length = 4,
    .feeder_penalty = 3,
    .pos_lead_divisor = 3,
    /* Response */
    .pos_resp_divisor = 4,
    .danger_per_card = 10,
    .gatekeeper_bonus = 20,
    .ctrl_2_penalty = 35,
    .ctrl_sj_penalty = 50,
    .ctrl_bj_penalty = 60,
    .must_stop_multiplier = 2,
    /* Combination integrity */
    .break_combo_penalty = 300,
    .kicker_joker = 80,
    .kicker_2 = 50,
    .kicker_ace = 30,
    .kicker_king = 15,
    .kicker_bomb_break = 100,
    .kicker_triple_break = 25,
    .kicker_pair_break = 10,
    /* Opponent inference */
    .bid3_ctrl_reduction = 15,
    .void_threshold = 2,
    /* Strategic lead categories */
    .landlord_weak_bonus = 15,
    .partner_signal_bonus = 10,
};

const BotWeights *bot_default_weights(void) { return &DEFAULT_WEIGHTS; }

void bot_weights_init(BotWeights *out) {
    if (out) *out = DEFAULT_WEIGHTS;
}

static const char *const BOT_WEIGHT_NAMES[] = {
    "clear_per_card",
    "endgame_clear",
    "chain_length",
    "feeder_penalty",
    "pos_lead_divisor",
    "pos_resp_divisor",
    "danger_per_card",
    "gatekeeper_bonus",
    "ctrl_2_penalty",
    "ctrl_sj_penalty",
    "ctrl_bj_penalty",
    "must_stop_multiplier",
    "break_combo_penalty",
    "kicker_joker",
    "kicker_2",
    "kicker_ace",
    "kicker_king",
    "kicker_bomb_break",
    "kicker_triple_break",
    "kicker_pair_break",
    "bid3_ctrl_reduction",
    "void_threshold",
    "landlord_weak_bonus",
    "partner_signal_bonus",
};

static const BotWeights *resolve_weights(const BotWeights *weights) {
    return weights ? weights : &DEFAULT_WEIGHTS;
}

/* Per-weight tuning bounds.
 *
 * Keeps the same field order as BotWeights / BOT_WEIGHT_NAMES.
 *
 * Rules of thumb used:
 *   - Divisors (pos_*_divisor): min=1 to prevent divide-by-zero; max=10
 *     because above ~10 the positional term is effectively zeroed out.
 *   - Count fields (void_threshold): small integer range only.
 *   - Penalty/bonus fields: max ≈ 3–5× the default so the optimiser has
 *     room to explore without going into obviously degenerate territory.
 */
static const int BOT_WEIGHT_MIN[] = {
    /*clear_per_card*/ 1,
    /*endgame_clear*/ 0,
    /*chain_length*/ 0,
    /*feeder_penalty*/ 0,
    /*pos_lead_divisor*/ 1,
    /*pos_resp_divisor*/ 1,
    /*danger_per_card*/ 0,
    /*gatekeeper_bonus*/ 0,
    /*ctrl_2_penalty*/ 0,
    /*ctrl_sj_penalty*/ 0,
    /*ctrl_bj_penalty*/ 0,
    /*must_stop_multiplier*/1,
    /*break_combo_penalty*/ 0,
    /*kicker_joker*/ 0,
    /*kicker_2*/ 0,
    /*kicker_ace*/ 0,
    /*kicker_king*/ 0,
    /*kicker_bomb_break*/ 0,
    /*kicker_triple_break*/ 0,
    /*kicker_pair_break*/ 0,
    /*bid3_ctrl_reduction*/ 0,
    /*void_threshold*/ 1,
    /*landlord_weak_bonus*/ 0,
    /*partner_signal_bonus*/0,
};

static const int BOT_WEIGHT_MAX[] = {
    /*clear_per_card*/ 50,
    /*endgame_clear*/ 40,
    /*chain_length*/ 20,
    /*feeder_penalty*/ 15,
    /*pos_lead_divisor*/ 10,
    /*pos_resp_divisor*/ 10,
    /*danger_per_card*/ 40,
    /*gatekeeper_bonus*/ 80,
    /*ctrl_2_penalty*/ 150,
    /*ctrl_sj_penalty*/ 200,
    /*ctrl_bj_penalty*/ 250,
    /*must_stop_multiplier*/8,
    /*break_combo_penalty*/ 1000,
    /*kicker_joker*/ 300,
    /*kicker_2*/ 200,
    /*kicker_ace*/ 120,
    /*kicker_king*/ 60,
    /*kicker_bomb_break*/ 400,
    /*kicker_triple_break*/ 100,
    /*kicker_pair_break*/ 40,
    /*bid3_ctrl_reduction*/ 60,
    /*void_threshold*/ 5,
    /*landlord_weak_bonus*/ 60,
    /*partner_signal_bonus*/40,
};

int bot_weights_count(void) {
    return (int) (sizeof(BotWeights) / sizeof(int));
}

const char *bot_weights_name(const int index) {
    if (index < 0 || index >= bot_weights_count()) return NULL;
    return BOT_WEIGHT_NAMES[index];
}

int bot_weights_get(const BotWeights *weights, const int index) {
    const BotWeights *src = resolve_weights(weights);
    if (index < 0 || index >= bot_weights_count()) return 0;
    return ((const int *) src)[index];
}

int bot_weights_set(BotWeights *weights, const int index, const int value) {
    if (!weights || index < 0 || index >= bot_weights_count()) return 0;
    ((int *) weights)[index] = value;
    return 1;
}

int bot_weights_min(const int index) {
    if (index < 0 || index >= bot_weights_count()) return 0;
    return BOT_WEIGHT_MIN[index];
}

int bot_weights_max(const int index) {
    if (index < 0 || index >= bot_weights_count()) return 0;
    return BOT_WEIGHT_MAX[index];
}

/* ---------------------------------------------------------------------------
 * Broad move-type categories (for history inference)
 *
 * Rather than tracking 14 individual MoveTypes, we bucket them into four
 * broad families that matter for void/weakness detection.
 * --------------------------------------------------------------------------- */

#define BROAD_SINGLE 0  /* MOVE_SINGLE, MOVE_TRIPLE_SINGLE, MOVE_FOUR_TWO_SINGLES */
#define BROAD_PAIR   1  /* MOVE_PAIR, MOVE_PAIR_STRAIGHT, MOVE_TRIPLE_PAIR, etc. */
#define BROAD_TRIPLE 2  /* MOVE_TRIPLE, MOVE_TRIPLE_STRAIGHT, MOVE_TRIPLE_STRAIGHT_SINGLES */
#define BROAD_CHAIN  3  /* MOVE_STRAIGHT */
#define BROAD_COUNT  4

static int move_broad_type(const MoveType t) {
    switch (t) {
        case MOVE_SINGLE:
        case MOVE_TRIPLE_SINGLE:
        case MOVE_FOUR_TWO_SINGLES:
            return BROAD_SINGLE;
        case MOVE_PAIR:
        case MOVE_PAIR_STRAIGHT:
        case MOVE_TRIPLE_PAIR:
        case MOVE_FOUR_TWO_PAIRS:
        case MOVE_TRIPLE_STRAIGHT_PAIRS:
            return BROAD_PAIR;
        case MOVE_TRIPLE:
        case MOVE_TRIPLE_STRAIGHT:
        case MOVE_TRIPLE_STRAIGHT_SINGLES:
            return BROAD_TRIPLE;
        case MOVE_STRAIGHT:
            return BROAD_CHAIN;
        default:
            return -1; /* bombs, rockets — no category */
    }
}

/* ---------------------------------------------------------------------------
 * Internal decision context – computed fresh at each decision point.
 * --------------------------------------------------------------------------- */

typedef struct {
    int is_peasant;    /* 1 if the bot plays for the peasant team */
    int partner;       /* Player index of peasant partner, or PLAYER_NONE */
    int landlord; /* Player index of landlord, or PLAYER_NONE */
    int is_leading; /* 1 if the bot has no card to beat */
    int cards_left[GAME_NUM_PLAYERS]; /* Remaining hand sizes (public information) */
    int landlord_bid; /* Bid value the landlord committed to (1–3) */
    int high_cards_out; /* Estimated 2s + jokers still in enemy hands */
    int danger; /* An enemy has ≤ 3 cards left (near win) */
    int endgame;       /* Any player has ≤ 5 cards left */
    int is_gatekeeper; /* Peasant directly after landlord (aggressive blocker) */
    int is_feeder; /* Peasant directly before landlord (pass small cards) */
    int must_stop;     /* Landlord has ≤ 1 card — must play highest possible */

    /* Opponent Inference ------------------------------------------ */
    int opp_pass_singles[GAME_NUM_PLAYERS]; /* Pass count on single-family combos */
    int opp_pass_pairs[GAME_NUM_PLAYERS]; /* Pass count on pair-family combos */

    /* Strategic Lead Categories ----------------------------------- */
    int partner_lead_type; /* Broad type partner led most (−1 = unknown) */
    int landlord_pass_type; /* Broad type landlord passed on most (−1 = unknown) */
} BotCtx;

/* ---------------------------------------------------------------------------
 * History analysis
 * --------------------------------------------------------------------------- */

/**
 * @brief Accumulates rank counts over all cards played in the history log.
 */
static void count_played_ranks(const GameState *g, int played[RANK_COUNT_SIZE]) {
    memset(played, 0, RANK_COUNT_SIZE * sizeof(int));
    for (int i = 0; i < g->history_count; i++) {
        const PlayRecord *rec = &g->history[i];
        if (rec->move.type == MOVE_PASS) continue;
        for (int j = 0; j < rec->move.count; j++) {
            const int r = CARD_RANK(rec->move.cards[j]);
            if (r < RANK_COUNT_SIZE) played[r]++;
        }
    }
}

/**
 * @brief Scans the game history and populates opponent-inference fields.
 *
 * Round reconstruction: tracks `passes_since_last_play` to detect when a
 * player wins the round (all others pass) and re-leads.  A non-pass move is
 * classified as a "new lead" when passes_since_last_play >= 2 (all other
 * players in the 3-player game have passed since the current table-owner's
 * last play) or when no owner exists yet (game start).
 *
 * accumulates per-player pass counts split by broad type.
 * finds the partner's most-common lead type and the landlord's
 *      most-common pass type, used to guide mid-game leading decisions.
 */
static void analyze_history(const GameState *g, const int partner,
                            int opp_pass_singles[GAME_NUM_PLAYERS],
                            int opp_pass_pairs[GAME_NUM_PLAYERS],
                            int *partner_lead_type,
                            int *landlord_pass_type) {
    int pass_cnt[GAME_NUM_PLAYERS][BROAD_COUNT];
    int lead_cnt[GAME_NUM_PLAYERS][BROAD_COUNT];
    memset(pass_cnt, 0, sizeof(pass_cnt));
    memset(lead_cnt, 0, sizeof(lead_cnt));

    MoveType table_type = MOVE_PASS;
    int table_owner = PLAYER_NONE;
    int passes_since = 0; /* passes by others since last non-pass move */

    for (int i = 0; i < g->history_count; i++) {
        const PlayRecord *rec = &g->history[i];

        if (rec->move.type == MOVE_PASS) {
            passes_since++;
            /* Count what type the player passed on (if they weren't the owner) */
            const int bt = move_broad_type(table_type);
            if (bt >= 0 && table_owner != PLAYER_NONE && table_owner != rec->player)
                pass_cnt[rec->player][bt]++;
        } else {
            /* Classify as new lead vs. response */
            const int is_new_lead =
                    (table_owner == PLAYER_NONE) || (passes_since >= 2);
            const int bt = move_broad_type(rec->move.type);
            if (bt >= 0 && is_new_lead)
                lead_cnt[rec->player][bt]++;

            passes_since = 0;
            table_type = rec->move.type;
            table_owner = rec->player;
        }
    }

    /* Populate per-player pass-count arrays */
    for (int p = 0; p < GAME_NUM_PLAYERS; p++) {
        opp_pass_singles[p] = pass_cnt[p][BROAD_SINGLE];
        opp_pass_pairs[p] = pass_cnt[p][BROAD_PAIR];
    }

    /* Partner's preferred lead type: requires > 1 lead in that category */
    *partner_lead_type = -1;
    if (partner != PLAYER_NONE) {
        int best_c = 1, best_t = -1;
        for (int bt = 0; bt < BROAD_COUNT; bt++) {
            if (lead_cnt[partner][bt] > best_c) {
                best_c = lead_cnt[partner][bt];
                best_t = bt;
            }
        }
        *partner_lead_type = best_t;
    }

    /* Landlord's most-passed type: requires > 1 pass in that category */
    *landlord_pass_type = -1;
    if (g->landlord != PLAYER_NONE) {
        int best_c = 1, best_t = -1;
        for (int bt = 0; bt < BROAD_COUNT; bt++) {
            if (pass_cnt[g->landlord][bt] > best_c) {
                best_c = pass_cnt[g->landlord][bt];
                best_t = bt;
            }
        }
        *landlord_pass_type = best_t;
    }
}

/* ---------------------------------------------------------------------------
 * Context computation
 * --------------------------------------------------------------------------- */

static void compute_ctx(const GameState *g, const int player, BotCtx *ctx) {
    ctx->is_peasant = game_is_peasant(g, player);
    ctx->partner    = PLAYER_NONE;
    ctx->landlord   = g->landlord;

    if (ctx->is_peasant) {
        for (int p = 0; p < GAME_NUM_PLAYERS; p++) {
            if (p != player && game_is_peasant(g, p)) {
                ctx->partner = p;
                break;
            }
        }
    }

    ctx->is_leading = (g->last_player == PLAYER_NONE || g->last_player == player);

    for (int p = 0; p < GAME_NUM_PLAYERS; p++)
        ctx->cards_left[p] = g->hands[p].count;

    ctx->landlord_bid = (g->landlord != PLAYER_NONE) ? g->bid.scores[g->landlord] : 1;

    /* -------------------------------------------------------------------
     * Infer how many control cards (2s, jokers) are still in enemy hands.
     * We know: cards in our own hand + cards played in history.
     * Anything else must be distributed among opponents.
     * ------------------------------------------------------------------- */
    int played[RANK_COUNT_SIZE];
    count_played_ranks(g, played);

    int own[RANK_COUNT_SIZE];
    moves_count_ranks(g->hands[player].cards, g->hands[player].count, own);

    const int twos_seen = played[RANK_2] + own[RANK_2];
    const int sj_seen   = played[RANK_SMALL_JOKER] + own[RANK_SMALL_JOKER];
    const int bj_seen   = played[RANK_BIG_JOKER]   + own[RANK_BIG_JOKER];

    /* Max possible: four 2s + one small joker + one big joker = 6 */
    ctx->high_cards_out = (4 - twos_seen) + (1 - sj_seen) + (1 - bj_seen);

    /* -------------------------------------------------------------------
     * Bidding clue: if the landlord bid 3 they likely hold at
     * least 2 control cards we have not yet seen.  Ensure our enemy-control
     * estimate reflects that minimum so we don't under-estimate the threat.
     * ------------------------------------------------------------------- */
    if (ctx->is_peasant && ctx->landlord_bid >= 3) {
        if (ctx->high_cards_out < 2) ctx->high_cards_out = 2;
    }

    /* -------------------------------------------------------------------
     * Endgame / danger detection.
     * danger  = an enemy team member has ≤ 3 cards (imminent win threat).
     * endgame = anyone has ≤ 5 cards (accelerate card-clearing).
     * ------------------------------------------------------------------- */
    ctx->danger  = 0;
    ctx->endgame = 0;
    for (int p = 0; p < GAME_NUM_PLAYERS; p++) {
        if (ctx->cards_left[p] <= 5) ctx->endgame = 1;

        if (ctx->cards_left[p] <= 3) {
            const int p_is_peasant = game_is_peasant(g, p);
            if (ctx->is_peasant != p_is_peasant) ctx->danger = 1;
        }
    }

    /* -------------------------------------------------------------------
     * Gatekeeper / feeder roles (peasant team only).
     *
     * Gatekeeper = the peasant sitting immediately after the landlord in
     * turn order.  They are the first to respond to every landlord lead,
     * so they should play aggressively to block the landlord.
     *
     * Feeder = the peasant sitting immediately before the landlord.  They
     * want to pass cheap leads to the gatekeeper / partner.
     * ------------------------------------------------------------------- */
    ctx->is_gatekeeper = 0;
    ctx->is_feeder     = 0;
    if (ctx->is_peasant && g->landlord != PLAYER_NONE) {
        const int after_landlord  = game_next_player(g, g->landlord);
        const int before_landlord = game_next_player(g, after_landlord);
        ctx->is_gatekeeper = (player == after_landlord);
        ctx->is_feeder     = (player == before_landlord);
    }

    /* -------------------------------------------------------------------
     * Must-stop: landlord is one play away from winning.
     * When must_stop is set, peasants should respond with the highest
     * card they have rather than the cheapest one.
     * ------------------------------------------------------------------- */
    ctx->must_stop = 0;
    if (ctx->is_peasant && g->landlord != PLAYER_NONE) {
        if (ctx->cards_left[g->landlord] <= 1) ctx->must_stop = 1;
    }

    /* -------------------------------------------------------------------
     * History inference.
     *
     * Populate pass-count arrays and infer the partner's signalled lead type
     * plus the landlord's demonstrated weakness type.
     * ------------------------------------------------------------------- */
    analyze_history(g, ctx->partner,
                    ctx->opp_pass_singles,
                    ctx->opp_pass_pairs,
                    &ctx->partner_lead_type,
                    &ctx->landlord_pass_type);
}

/* ---------------------------------------------------------------------------
 * Combination preservation
 * --------------------------------------------------------------------------- */

/**
 * @brief Returns 1 if the move partially consumes a bomb or rocket in hand.
 *
 * Playing e.g. a single 3 when holding four 3s squanders bomb potential.
 * Playing all four 3s as a bomb, or both jokers as a rocket, is fine.
 */
static int breaks_combination(const Move *m, const int hand_cnt[RANK_COUNT_SIZE]) {
    if (m->type == MOVE_BOMB || m->type == MOVE_ROCKET) return 0;

    int mc[RANK_COUNT_SIZE];
    moves_count_ranks(m->cards, m->count, mc);

    /* Partial use of a four-of-a-kind */
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        if (hand_cnt[r] >= 4 && mc[r] > 0 && mc[r] < 4) return 1;
    }

    /* Partial use of a rocket (using only one joker when both are held) */
    const int has_rocket =
            hand_cnt[RANK_SMALL_JOKER] >= 1 && hand_cnt[RANK_BIG_JOKER] >= 1;
    if (has_rocket) {
        const int uses_sj = mc[RANK_SMALL_JOKER] > 0;
        const int uses_bj = mc[RANK_BIG_JOKER] > 0;
        if ((uses_sj || uses_bj) && !(uses_sj && uses_bj)) return 1;
    }

    return 0;
}

/* ---------------------------------------------------------------------------
 * Kicker quality
 * --------------------------------------------------------------------------- */

/**
 * @brief Returns a penalty score for the quality of kicker cards in a move.
 *
 * Using a control card (2, Ace, Joker) or a bomb component as a kicker is
 * wasteful.  This penalty discourages those choices so that the bot attaches
 * its most "useless" singles or pairs instead.
 *
 * Constants are drawn from BotWeights (W) so they can be tuned centrally.
 */
static int kicker_penalty(const Move *m, const int hand_cnt[RANK_COUNT_SIZE],
                          const BotWeights *weights) {
    /* Only moves with kicker slots are penalised */
    int kicker_need;
    int core_min; /* ranks with this many cards in the move are "core", not kickers */
    switch (m->type) {
        case MOVE_TRIPLE_SINGLE:
        case MOVE_TRIPLE_STRAIGHT_SINGLES:
            kicker_need = 1; core_min = 3; break;
        case MOVE_TRIPLE_PAIR:
        case MOVE_TRIPLE_STRAIGHT_PAIRS:
            kicker_need = 2;
            core_min = 3;
            break;
        case MOVE_FOUR_TWO_SINGLES:
            kicker_need = 1; core_min = 4;
            break;
        case MOVE_FOUR_TWO_PAIRS:
            kicker_need = 2;
            core_min = 4;
            break;
        default:
            return 0;
    }

    int mc[RANK_COUNT_SIZE];
    moves_count_ranks(m->cards, m->count, mc);

    int penalty = 0;
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        if (mc[r] != kicker_need) continue;
        if (mc[r] >= core_min) continue;

        /* Base penalty: higher rank = more valuable = worse kicker */
        int val;
        if (r == RANK_BIG_JOKER || r == RANK_SMALL_JOKER) val = weights->kicker_joker;
        else if (r == RANK_2) val = weights->kicker_2;
        else if (r == RANK_A) val = weights->kicker_ace;
        else if (r == RANK_K) val = weights->kicker_king;
        else val = r;

        /* Extra penalty for breaking latent bomb / pair / triple potential. */
        if (hand_cnt[r] >= 4) val += weights->kicker_bomb_break;
        else if (hand_cnt[r] == 3) val += weights->kicker_triple_break;
        else if (hand_cnt[r] == 2 && kicker_need < 2) val += weights->kicker_pair_break;

        penalty += val;
    }
    return penalty;
}

/* ---------------------------------------------------------------------------
 * Post-move position evaluation
 * --------------------------------------------------------------------------- */

/**
 * @brief Estimates play position after removing a move's cards from the hand.
 *
 * Used for 1-ply lookahead: the quality of the remaining hand is factored into
 * move scoring, naturally penalising moves that break straights or pair chains
 * and rewarding moves that leave the hand in a clean, few-play state.
 */
static int position_after_move(const int hand_cnt[RANK_COUNT_SIZE],
                               const Move *m, const int hand_size) {
    if (m->count >= hand_size) return 200; /* empty hand = perfect */

    int mc[RANK_COUNT_SIZE];
    moves_count_ranks(m->cards, m->count, mc);

    int after[RANK_COUNT_SIZE];
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        after[r] = hand_cnt[r] - mc[r];
        if (after[r] < 0) after[r] = 0;
    }
    return eval_play_position_counts(after, hand_size - m->count);
}

/* ---------------------------------------------------------------------------
 * Move scoring
 * --------------------------------------------------------------------------- */

/**
 * @brief Scores a candidate leading move. Higher score = more desirable.
 *
 * Strategy:
 *  - A finishing move (clears entire hand) is always top priority.
 *  - Prefer combos that clear more cards per turn.
 *  - Prefer lower-ranked moves to save control cards.
 *  - Penalise moves that break a latent bomb or chain.
 *  - Post-move position: score the remaining hand quality (shape preservation).
 *  - In early game, bonus for leading chain moves (straights, airplanes).
 *  - In endgame, weight card-clearing more heavily.
 *  - Bombs and rockets are scored very low (saved for when needed).
 *  - Bonus for leading types the landlord is weak/void in, or
 *    types the partner has signalled.
 */
static int score_lead(const Move *m, const BotCtx *ctx,
                      const int hand_size, const int hand_cnt[RANK_COUNT_SIZE],
                      const BotWeights *weights) {
    if (m->type == MOVE_BOMB || m->type == MOVE_ROCKET) return -5000;
    if (m->count == hand_size) return 10000; /* win immediately */

    int sc = 0;

    /* Clearing more cards per turn is intrinsically valuable */
    sc += m->count * weights->clear_per_card;

    /* Prefer weaker ranks so control cards are preserved */
    sc -= eval_move_cost(m);

    /* Heavy penalty for breaking up a latent bomb or rocket */
    if (breaks_combination(m, hand_cnt)) sc -= weights->break_combo_penalty;

    /* Penalty for attaching high-value or bomb-breaking cards as kickers */
    sc -= kicker_penalty(m, hand_cnt, weights);

    /* Post-move position: reward moves that leave the hand in good shape. */
    const int pld = weights->pos_lead_divisor > 0 ? weights->pos_lead_divisor : 1;
    sc += position_after_move(hand_cnt, m, hand_size) / pld;

    /* In endgame, accelerate: clear as many cards as possible each turn */
    if (ctx->endgame) sc += m->count * weights->endgame_clear;

    /* Early game: bonus for leading chain combos to reduce hand size quickly
     * and probe what opponents have. */
    if (!ctx->endgame) {
        switch (m->type) {
            case MOVE_STRAIGHT:
            case MOVE_PAIR_STRAIGHT:
            case MOVE_TRIPLE_STRAIGHT:
            case MOVE_TRIPLE_STRAIGHT_SINGLES:
            case MOVE_TRIPLE_STRAIGHT_PAIRS:
                sc += m->length * weights->chain_length;
                break;
            default: break;
        }
    }

    /* Feeder (peasant before landlord): prefer smaller leads to give the
     * gatekeeper / partner opportunities to respond to the landlord. */
    if (ctx->is_feeder) sc -= m->count * weights->feeder_penalty;

    /* -------------------------------------------------------------------
     * Exploit history-inferred weaknesses and partner signals.
     *
     * landlord_pass_type: the broad type the landlord has passed on the most.
     *   If we lead that type, the landlord is likely unable to respond strongly.
     *
     * partner_lead_type: the broad type our partner has led the most.
     *   Matching it reinforces their preferred avenue of attack (mid-game only,
     *   since in endgame we clear as fast as possible regardless of type).
     * ------------------------------------------------------------------- */
    if (ctx->is_peasant) {
        const int lead_bt = move_broad_type(m->type);
        if (lead_bt >= 0) {
            if (lead_bt == ctx->landlord_pass_type)
                sc += weights->landlord_weak_bonus;
            if (!ctx->endgame && lead_bt == ctx->partner_lead_type)
                sc += weights->partner_signal_bonus;
        }
    }

    return sc;
}

/**
 * @brief Scores a candidate response move. Higher score = more desirable.
 *
 * Strategy:
 *  - Finishing move is always top priority.
 *  - Respond with the cheapest move that beats the table.
 *  - Penalise moves that break a latent bomb or chain.
 *  - Post-move position: factor in remaining hand quality after responding.
 *  - In danger, prefer heavier responses to keep pressure on the enemy.
 *  - Control card preservation: penalise using 2s/jokers unless must-stop.
 *  - When the landlord bid 3, reduce the control-card save penalty
 *    because the landlord likely has many strong cards and must be fought.
 */
static int score_response(const Move *m, const BotCtx *ctx,
                          const int hand_size, const int hand_cnt[RANK_COUNT_SIZE],
                          const GameState *g, const BotWeights *weights) {
    if (m->count == hand_size) return 10000;

    int sc = 0;

    /* Cheapest card that beats is the default — negate cost so lower cost = higher score */
    sc -= eval_move_cost(m);

    /* Avoid wasting a bomb component as a kicker */
    if (breaks_combination(m, hand_cnt)) sc -= weights->break_combo_penalty;

    /* Penalty for attaching high-value or bomb-breaking cards as kickers */
    sc -= kicker_penalty(m, hand_cnt, weights);

    /* Post-move position: penalise responses that leave the hand in poor shape */
    const int prd = weights->pos_resp_divisor > 0 ? weights->pos_resp_divisor : 1;
    sc += position_after_move(hand_cnt, m, hand_size) / prd;

    /* Under threat, prefer heavier responses to seize control */
    if (ctx->danger) sc += m->count * weights->danger_per_card;

    /* Gatekeeper: play aggressively when directly stopping the landlord's lead. */
    if (ctx->is_gatekeeper && g->last_player == g->landlord) sc += weights->gatekeeper_bonus;

    /* Pass-value / control card preservation: when a peasant is not in danger,
     * penalise using precious control cards to beat something they don't have
     * to stop.  This makes the bot prefer to pass rather than burn a 2 / Joker.
     *
     * Bid clue: if the landlord bid 3 they almost certainly have strong
     * control cards.  Reduce the save-penalty so peasants are more willing to
     * spend control cards to hold the line against a strong-hand landlord. */
    if (ctx->is_peasant && !ctx->must_stop && !ctx->danger) {
        int mc[RANK_COUNT_SIZE];
        moves_count_ranks(m->cards, m->count, mc);

        int p2 = weights->ctrl_2_penalty;
        int psj = weights->ctrl_sj_penalty;
        int pbj = weights->ctrl_bj_penalty;

        if (ctx->landlord_bid >= 3) {
            const int red = weights->bid3_ctrl_reduction;
            p2 = (p2 > red) ? p2 - red : 0;
            psj = (psj > red) ? psj - red : 0;
            pbj = (pbj > red) ? pbj - red : 0;
        }

        sc -= mc[RANK_2] * p2;
        sc -= mc[RANK_SMALL_JOKER] * psj;
        sc -= mc[RANK_BIG_JOKER] * pbj;
    }

    /* Must-stop: landlord has ≤ 1 card and will win next turn if not stopped.
     * Reverse the usual "cheapest beats" preference — play the strongest card. */
    if (ctx->must_stop && m->type != MOVE_BOMB && m->type != MOVE_ROCKET) {
        sc += weights->must_stop_multiplier * eval_move_cost(m);
    }

    return sc;
}

/* ---------------------------------------------------------------------------
 * Bomb worthiness
 * --------------------------------------------------------------------------- */

/**
 * @brief Returns 1 if using a bomb is justified by the current game state.
 *
 * Bombing doubles the score, so it should only happen when:
 *  - An enemy is about to win (danger), or
 *  - We are in endgame with a high-stakes round (landlord bid >= 2), or
 *  - The opponent we are targeting has 5 or fewer cards, or
 *  - We have a "ready-to-go" hand (≤ 2 plays to clear) and are in endgame —
 *    bomb to seize the lead and potentially win in 1-2 more turns.
 */
static int should_bomb(const BotCtx *ctx, const GameState *g, const int player) {
    if (ctx->danger) return 1;
    if (ctx->endgame && ctx->landlord_bid >= 2) return 1;

    if (ctx->is_peasant) {
        if (g->hands[g->landlord].count <= 5) return 1;
    } else {
        for (int p = 0; p < GAME_NUM_PLAYERS; p++) {
            if (game_is_peasant(g, p) && g->hands[p].count <= 5) return 1;
        }
    }

    /* Initiative / tempo: bomb to seize the lead when our hand is
     * nearly clean and we are in endgame. */
    if (ctx->endgame) {
        const int mp = eval_min_plays(g->hands[player].cards, g->hands[player].count);
        if (mp <= 2) return 1;
    }

    return 0;
}

/* ---------------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------------- */

int bot_bid_with_weights(const GameState *g, const int player, const BotWeights *weights) {
    (void) weights;
    const int score = eval_bid_strength(g->hands[player].cards, g->hands[player].count);

    int want;
    if      (score >= 50) want = 3;
    else if (score >= 30) want = 2;
    else if (score >= 15) want = 1;
    else return 0;

    if (want <= g->bid.highest_score) {
        if (g->bid.highest_score < 3) want = g->bid.highest_score + 1;
        else return 0;
    }

    return want;
}

int bot_bid(const GameState *g, const int player) {
    return bot_bid_with_weights(g, player, &DEFAULT_WEIGHTS);
}

Move bot_play_with_weights(const GameState *g, const int player, const BotWeights *weights) {
    const BotWeights *w = resolve_weights(weights);
    BotCtx ctx;
    compute_ctx(g, player, &ctx);

    Move buf[BOT_MAX_MOVES];
    const int n = game_legal_moves(g, player, buf, BOT_MAX_MOVES);

    if (n == 0) {
        const Move pass = {MOVE_PASS};
        return pass;
    }

    int hand_cnt[RANK_COUNT_SIZE];
    moves_count_ranks(g->hands[player].cards, g->hands[player].count, hand_cnt);
    const int hand_size = g->hands[player].count;

    /* -------------------------------------------------------------------
     * Peasant cooperation (Yield Rule): if our partner controls the table
     * and the landlord is not an immediate threat, yield to let them lead.
     *
     * Exception: if we can go out this turn (win the game), play the
     * winning move rather than yielding — don't sit on a victory.
     * ------------------------------------------------------------------- */
    if (!ctx.is_leading &&
        ctx.is_peasant &&
        ctx.partner != PLAYER_NONE &&
        g->last_player == ctx.partner &&
        !ctx.danger) {
        int can_win = 0;
        for (int i = 0; i < n; i++) {
            if (buf[i].type != MOVE_PASS && buf[i].count == hand_size) {
                can_win = 1;
                break;
            }
        }
        if (!can_win) {
            const Move pass = {MOVE_PASS};
            return pass;
        }
    }

    /* -------------------------------------------------------------------
     * Score every legal move; separate bombs from normal moves since the
     * decision to bomb depends on context rather than just cost.
     * ------------------------------------------------------------------- */
    Move best      = {0}; best.type      = MOVE_INVALID;
    Move best_bomb = {0}; best_bomb.type = MOVE_INVALID;
    int best_sc = -99999;
    int best_bomb_cost = 99999;

    for (int i = 0; i < n; i++) {
        const Move *m = &buf[i];
        if (m->type == MOVE_PASS) continue;

        const int is_bomb = (m->type == MOVE_BOMB || m->type == MOVE_ROCKET);

        if (is_bomb) {
            const int cost = eval_move_cost(m);
            if (best_bomb.type == MOVE_INVALID || cost < best_bomb_cost) {
                best_bomb      = *m;
                best_bomb_cost = cost;
            }
            continue;
        }

        const int sc = ctx.is_leading
                           ? score_lead(m, &ctx, hand_size, hand_cnt, w)
                           : score_response(m, &ctx, hand_size, hand_cnt, g, w);

        if (best.type == MOVE_INVALID || sc > best_sc) {
            best = *m;
            best_sc = sc;
        }
    }

    /* -------------------------------------------------------------------
     * Final selection:
     *   - When leading  : prefer a normal move; fall back to bomb only if
     *                     no normal moves exist (hand is all bombs).
     *   - When following: prefer a normal move; bomb only when justified.
     *
     * Pass-value: when following, if the best normal response has
     * a negative score it means playing it actively costs us (e.g. burning
     * a control card we'd rather keep).  In that case, pass instead.
     * ------------------------------------------------------------------- */
    if (ctx.is_leading) {
        if (best.type != MOVE_INVALID) return best;
        if (best_bomb.type != MOVE_INVALID) return best_bomb;
    } else {
        if (best.type != MOVE_INVALID && best_sc < 0 && !ctx.danger && !ctx.must_stop) {
            const Move pass = {MOVE_PASS};
            return pass;
        }
        if (best.type != MOVE_INVALID) return best;
        if (best_bomb.type != MOVE_INVALID && should_bomb(&ctx, g, player))
            return best_bomb;
    }

    const Move pass = {MOVE_PASS};
    return pass;
}

Move bot_play(const GameState *g, const int player) {
    return bot_play_with_weights(g, player, &DEFAULT_WEIGHTS);
}

/* ---------------------------------------------------------------------------
 * Self-play simulation driver
 * --------------------------------------------------------------------------- */

/**
 * @brief Runs num_games bot-vs-bot games and tallies per-player win counts.
 *
 * Each game uses seed (base_seed + game_index) for the shuffle so results are
 * deterministic and reproducible.  Games in which every bot passes during
 * bidding (no landlord assigned) are retried with the next seed rather than
 * counted as a wasted game.
 */
void bot_simulate_with_weights(const int num_games, const unsigned int base_seed,
                               const BotWeights *per_player_weights[GAME_NUM_PLAYERS],
                               int wins_out[GAME_NUM_PLAYERS]) {
    for (int p = 0; p < GAME_NUM_PLAYERS; p++) wins_out[p] = 0;

    int completed = 0;
    unsigned int seed = base_seed;

    while (completed < num_games) {
        GameState g;
        game_init(&g);
        game_reset_deck(&g);
        game_shuffle(&g, seed++);
        game_deal(&g);
        game_start_bidding(&g, 0);

        /* Bidding phase */
        int bid_iters = 0;
        while (g.phase == PHASE_BIDDING && bid_iters++ < 20) {
            const int p = g.bid.current_bidder;
            const BotWeights *weights = per_player_weights ? per_player_weights[p] : NULL;
            const int bid = bot_bid_with_weights(&g, p, weights);
            if (!game_bid(&g, p, bid)) break;
        }

        /* Skip games where no one wanted to be landlord */
        if (g.phase != PHASE_PLAYING) continue;

        /* Playing phase */
        int play_iters = 0;
        while (g.phase == PHASE_PLAYING && play_iters++ < 400) {
            const int p = g.current_player;
            const BotWeights *weights = per_player_weights ? per_player_weights[p] : NULL;
            const Move m = bot_play_with_weights(&g, p, weights);
            if (!game_play(&g, p, &m)) break; /* safety: illegal move exits */
        }

        if (g.phase == PHASE_OVER && g.winner >= 0 && g.winner < GAME_NUM_PLAYERS)
            wins_out[g.winner]++;

        completed++;
    }
}

void bot_simulate(const int num_games, const unsigned int base_seed,
                  int wins_out[GAME_NUM_PLAYERS]) {
    bot_simulate_with_weights(num_games, base_seed, NULL, wins_out);
}

void bot_evaluate_weights(const BotWeights *candidate, const int num_games,
                          const unsigned int base_seed,
                          int *wins_as_landlord, int *wins_as_peasant) {
    /* Series A: candidate at seat 0 (becomes landlord when they win bidding),
     * default weights at seats 1 & 2. */
    const BotWeights *a_weights[GAME_NUM_PLAYERS] = {candidate, NULL, NULL};
    int a_wins[GAME_NUM_PLAYERS];
    bot_simulate_with_weights(num_games, base_seed, a_weights, a_wins);
    if (wins_as_landlord) *wins_as_landlord = a_wins[0];

    /* Series B: default at seat 0, candidate at seats 1 & 2.
     * A peasant-team win is any game the landlord (seat 0) did NOT win. */
    const BotWeights *b_weights[GAME_NUM_PLAYERS] = {NULL, candidate, candidate};
    int b_wins[GAME_NUM_PLAYERS];
    bot_simulate_with_weights(num_games, base_seed + (unsigned int) num_games,
                              b_weights, b_wins);
    if (wins_as_peasant) *wins_as_peasant = b_wins[1] + b_wins[2];
}
