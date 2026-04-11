/**
 * @file bot.c
 * @brief Rule-based AI implementation for libdoudizhu
 * @author Peng Yang Deng, Emma Le
 * @date 08-Mar-26
 */

#include "bot.h"
#include "eval.h"
#include "utils.h"

#define BOT_MAX_MOVES 512

// Default weights
static const BotWeights DEFAULT_WEIGHTS = {
        .clear_per_card = 16,
        .endgame_clear = 10,
        .chain_length = 7,
        .feeder_penalty = 3,
        .pos_lead_divisor = 4,
        .pos_resp_divisor = 4,
        .danger_per_card = 10,
        .gatekeeper_bonus = 20,
        .ctrl_2_penalty = 38,
        .ctrl_sj_penalty = 50,
        .ctrl_bj_penalty = 143,
        .must_stop_multiplier = 2,
        .break_combo_penalty = 300,
        .kicker_joker = 80,
        .kicker_2 = 108,
        .kicker_ace = 30,
        .kicker_king = 60,
        .kicker_bomb_break = 100,
        .kicker_triple_break = 99,
        .kicker_pair_break = 10,
        .bid3_ctrl_reduction = 60,
        .void_threshold = 2,
        .landlord_weak_bonus = 18,
        .partner_signal_bonus = 10,
};

const BotWeights* botDefaultWeights(void) { return &DEFAULT_WEIGHTS; }

/**
 * Resolves the bot's weights before gameplay.
 * @param weights Returns weights to use for play.
 * @return If the weights are NULL, then this will return the default weights.
 */
static const BotWeights* resolveWeights(const BotWeights* weights) { return weights ? weights : &DEFAULT_WEIGHTS; }

// Broad move categories for history inference
#define BROAD_SINGLE 0 // Singles, 3+1, 4+1+1
#define BROAD_PAIR 1 // Pairs, Straight pairs, 3+2, 4+2, etc.
#define BROAD_TRIPLE 2 // Triplets, Straight triplets, Straight triplets + 1, etc.
#define BROAD_CHAIN 3 // Straights
#define BROAD_COUNT 4

/**
 * Classify a move into types.
 * @param t Move to classify.
 * @return Classified type.
 */
static int moveBroadType(const MoveType t) {
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
            return -1; // Bombs and rockets don't get a category.
    }
}

/* --- Internal decision making --- */
typedef struct {
    int is_peasant; // 1 if the bot plays for the peasants
    int partner; // Player index of the peasant partner, PLAYER_NONE if bot is the landlord
    int landlord; // Player index of the landlord
    int is_leading; // 1 if the bot is leading the table
    int cards_left[GAME_NUM_PLAYERS]; // Remaining number of cards each player has
    int landlord_bid; // Bid value of the landlord
    int high_cards_out; // Estimated number of 2's and jokers still in play
    int danger; // Enemy has less than 3 cards
    int endgame; // Any player has ≤ 5 cards left
    int is_gatekeeper; // Peasant directly after landlord (aggressively block landlord)
    int is_feeder; // Peasant directly before landlord (passes small cards)
    int must_stop; // Landlord has ≤ 1 card (must play the largest possible moves)

    // Opponent inference
    int opp_pass_singles[GAME_NUM_PLAYERS]; // Pass count on single-family plays
    int opp_pass_pairs[GAME_NUM_PLAYERS]; // Pass count on pair-family plays

    // Other players' leading categories
    int partner_lead_type; // Broad type the bot's partner is leading in
    int landlord_pass_type; // Broad type the landlord passed the most in
} BotCtx;

/* --- History analysis --- */
/**
 * @brief Accumulates rank counts over all cards played in the history log.
 */
static void countPlayedRanks(const GameState* g, int played[RANK_COUNT_SIZE]) {
    lddzMemset(played, 0, RANK_COUNT_SIZE * sizeof(int));
    for (int i = 0; i < g->history_count; i++) {
        const PlayRecord* rec = &g->history[i];
        if (rec->move.type == MOVE_PASS)
            continue;
        for (int j = 0; j < rec->move.count; j++) {
            const int r = CARD_RANK(rec->move.cards[j]);
            if (r < RANK_COUNT_SIZE)
                played[r]++;
        }
    }
}

/**
 * @brief Scans the game history and populates opponent-inference fields.
 * @param g The currently playing game
 * @param partner The bot's partner (-1 if bot is landlord)
 * @param opp_pass_singles How many times an opponent passed on pairs (filled by this function)
 * @param opp_pass_pairs How many times an opponent passed on singles (filled by this function)
 * @param partner_lead_type What type the bot's partner is strong in
 * @param landlord_pass_type What type the landlord is weak in
 */
static void analyzeHistory(const GameState* g, const int partner, int opp_pass_singles[GAME_NUM_PLAYERS],
                           int opp_pass_pairs[GAME_NUM_PLAYERS], int* partner_lead_type, int* landlord_pass_type) {
    int pass_cnt[GAME_NUM_PLAYERS][BROAD_COUNT];
    int lead_cnt[GAME_NUM_PLAYERS][BROAD_COUNT];
    lddzMemset(pass_cnt, 0, sizeof(pass_cnt));
    lddzMemset(lead_cnt, 0, sizeof(lead_cnt));

    MoveType table_type = MOVE_PASS;
    int table_owner = PLAYER_NONE;
    int passes_since = 0; // Passes by others since the last non-pass move

    for (int i = 0; i < g->history_count; i++) {
        const PlayRecord* rec = &g->history[i];

        if (rec->move.type == MOVE_PASS) {
            // If a player passed on something, save it for inference
            passes_since++;

            // Get the type of move
            const int bt = moveBroadType(table_type);
            if (bt >= 0 && table_owner != PLAYER_NONE && table_owner != rec->player)
                pass_cnt[rec->player][bt]++;
        } else {
            // Classify it as a new lead and response
            const int is_new_lead = (table_owner == PLAYER_NONE) || (passes_since >= 2);
            const int bt = moveBroadType(rec->move.type);
            if (bt >= 0 && is_new_lead)
                lead_cnt[rec->player][bt]++;

            passes_since = 0;
            table_type = rec->move.type;
            table_owner = rec->player;
        }
    }

    // Populate per player pass counts
    for (int p = 0; p < GAME_NUM_PLAYERS; p++) {
        opp_pass_singles[p] = pass_cnt[p][BROAD_SINGLE];
        opp_pass_pairs[p] = pass_cnt[p][BROAD_PAIR];
    }

    // Infer the partner's preferred type
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

    // Infer the landlord's weakest type
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

/* --- Context computation --- */
static void computeCtx(const GameState* g, const int player, BotCtx* ctx) {
    ctx->is_peasant = gameIsPeasant(g, player);
    ctx->partner = PLAYER_NONE;
    ctx->landlord = g->landlord;

    // --- If the bot is a peasant, find its partner
    if (ctx->is_peasant) {
        for (int p = 0; p < GAME_NUM_PLAYERS; p++) {
            if (p != player && gameIsPeasant(g, p)) {
                ctx->partner = p;
                break;
            }
        }
    }

    // --- Find who is leading the hand
    ctx->is_leading = (g->last_player == PLAYER_NONE || g->last_player == player);

    // --- Get remaining card counts for all players
    for (int p = 0; p < GAME_NUM_PLAYERS; p++)
        ctx->cards_left[p] = g->hands[p].count;

    // --- Get landlord bid amount
    ctx->landlord_bid = (g->landlord != PLAYER_NONE) ? g->bid.scores[g->landlord] : 1;

    // --- Infer how many control cards (2s, jokers) are still in play from history
    int played[RANK_COUNT_SIZE];
    countPlayedRanks(g, played);

    // Add on the number of control cards we own
    int own[RANK_COUNT_SIZE];
    movesCountRanks(g->hands[player].cards, g->hands[player].count, own);

    const int twos_seen = played[RANK_2] + own[RANK_2];
    const int sj_seen = played[RANK_SMALL_JOKER] + own[RANK_SMALL_JOKER];
    const int bj_seen = played[RANK_BIG_JOKER] + own[RANK_BIG_JOKER];

    ctx->high_cards_out = (4 - twos_seen) + (1 - sj_seen) + (1 - bj_seen);

    // --- Bidding: If the landlord bid 3, they likely have at least 2 control cards we may have yet to see.
    // Ensure that enemy control estimates reflect that threat.
    if (ctx->is_peasant && ctx->landlord_bid >= 3) {
        if (ctx->high_cards_out < 2)
            ctx->high_cards_out = 2;
    }

    // --- Endgame/ Danger detection
    // Danger: An enemy team member has <= 3 cards (imminent winning possibility)
    // Endgame: Anyone has <= 5 cards (speed up card-clearing)
    ctx->danger = 0;
    ctx->endgame = 0;
    for (int p = 0; p < GAME_NUM_PLAYERS; p++) {
        if (ctx->cards_left[p] <= 5)
            ctx->endgame = 1;

        if (ctx->cards_left[p] <= 3) {
            const int p_is_peasant = gameIsPeasant(g, p);
            if (ctx->is_peasant != p_is_peasant)
                ctx->danger = 1;
        }
    }

    // --- Gatekeeper/ Feeder roles (for peasant team bot only)
    // Gatekeeper: Peasant immediately after the landlord. They are first to response to landlord leads, and should play
    // aggressively to block the landlord. Feeder: Peasant immediately before the landlord. They want to pass
    // inexpensive leads to the gatekeeper.
    ctx->is_gatekeeper = 0;
    ctx->is_feeder = 0;
    if (ctx->is_peasant && g->landlord != PLAYER_NONE) {
        const int after_landlord = gameNextPlayer(g, g->landlord);
        const int before_landlord = gameNextPlayer(g, after_landlord);
        ctx->is_gatekeeper = (player == after_landlord);
        ctx->is_feeder = (player == before_landlord);
    }

    // --- Must Stop: If the landlord is one play away from winning, the bot must stop the landlord.
    // In this situation, the bot must expend its most valuable hand each play.
    ctx->must_stop = 0;
    if (ctx->is_peasant && g->landlord != PLAYER_NONE) {
        if (ctx->cards_left[g->landlord] <= 1)
            ctx->must_stop = 1;
    }

    analyzeHistory(g, ctx->partner, ctx->opp_pass_singles, ctx->opp_pass_pairs, &ctx->partner_lead_type,
                   &ctx->landlord_pass_type);
}

/* --- Combo integrity --- */
/**
 * @brief Returns 1 if the move partially consumes a bomb or rocket in the hand.
 * @param m Potential move being played
 * @param hand_cnt Cards in the player's hand
 */
static int breaksCombination(const Move* m, const int hand_cnt[RANK_COUNT_SIZE]) {
    if (m->type == MOVE_BOMB || m->type == MOVE_ROCKET)
        return 0; // Rockets and bombs inherently cannot break a combo

    int mc[RANK_COUNT_SIZE];
    movesCountRanks(m->cards, m->count, mc);

    // Partial use of a 4 of a kind
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        if (hand_cnt[r] >= 4 && mc[r] > 0 && mc[r] < 4)
            return 1;
    }

    // Partial use of a rocket (if the other is in hand also)
    const int has_rocket = hand_cnt[RANK_SMALL_JOKER] >= 1 && hand_cnt[RANK_BIG_JOKER] >= 1;
    if (has_rocket) {
        const int uses_sj = mc[RANK_SMALL_JOKER] > 0;
        const int uses_bj = mc[RANK_BIG_JOKER] > 0;
        if ((uses_sj || uses_bj) && !(uses_sj && uses_bj))
            return 1;
    }

    return 0;
}

/* --- Kicker Quality --- */
/**
 * @brief Returns a penalty score for the quality of kicker cards in a move.
 * @param m The move being considered
 * @param hand_cnt Cards in the player's hand
 * @param weights The bot's weights
 * @details Using a control card (2, aces, jokers) or a bomb component as a kicker is wasteful.
 * This penalty discourages those choices, so the bot attaches its most "useless" singles or pairs instead.
 */
static int kickerPenalty(const Move* m, const int hand_cnt[RANK_COUNT_SIZE], const BotWeights* weights) {
    // Only moves with kicker slots are penalized
    int kicker_need;
    int core_min; // Ranks with this many cards in the move are "core", not kickers
    switch (m->type) {
        case MOVE_TRIPLE_SINGLE:
        case MOVE_TRIPLE_STRAIGHT_SINGLES:
            kicker_need = 1;
            core_min = 3;
            break;
        case MOVE_TRIPLE_PAIR:
        case MOVE_TRIPLE_STRAIGHT_PAIRS:
            kicker_need = 2;
            core_min = 3;
            break;
        case MOVE_FOUR_TWO_SINGLES:
            kicker_need = 1;
            core_min = 4;
            break;
        case MOVE_FOUR_TWO_PAIRS:
            kicker_need = 2;
            core_min = 4;
            break;
        default:
            return 0;
    }

    int mc[RANK_COUNT_SIZE];
    movesCountRanks(m->cards, m->count, mc);

    int penalty = 0;
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        if (mc[r] != kicker_need)
            continue;
        if (mc[r] >= core_min)
            continue;

        // Base penalty: higher rank = more valuable = worse kicker
        int val;
        if (r == RANK_BIG_JOKER || r == RANK_SMALL_JOKER)
            val = weights->kicker_joker;
        else if (r == RANK_2)
            val = weights->kicker_2;
        else if (r == RANK_A)
            val = weights->kicker_ace;
        else if (r == RANK_K)
            val = weights->kicker_king;
        else
            val = r;

        // Extra penalty for breaking bombs / pairs / triples
        if (hand_cnt[r] >= 4)
            val += weights->kicker_bomb_break;
        else if (hand_cnt[r] == 3)
            val += weights->kicker_triple_break;
        else if (hand_cnt[r] == 2 && kicker_need < 2)
            val += weights->kicker_pair_break;

        penalty += val;
    }
    return penalty;
}

/* --- Post-move Position Evaluations --- */
/**
 * @brief Estimates playing position after removing a move's cards from the hand.
 * @param hand_cnt Current cards in hand
 * @param m Move to play
 * @param hand_size Number of cards in hand
 * @details This is used for 1-move lookahead: The quality of the remaining hand is used in move scoring.
 * Remaining hands that are challenging to clear are penalized, while cleaner hands are rewarded.
 */
static int positionAfterMove(const int hand_cnt[RANK_COUNT_SIZE], const Move* m, const int hand_size) {
    if (m->count >= hand_size)
        return 200; // Empty hand = perfect

    int mc[RANK_COUNT_SIZE];
    movesCountRanks(m->cards, m->count, mc);

    int after[RANK_COUNT_SIZE];
    for (int r = 0; r < RANK_COUNT_SIZE; r++) {
        after[r] = hand_cnt[r] - mc[r];
        if (after[r] < 0)
            after[r] = 0;
    }
    return evalPlayPositionCounts(after, hand_size - m->count);
}

/* --- Move Scoring --- */
/**
 * @brief Scores a candidate leading move. Higher score = more desirable.
 * @param m Candidate move
 * @param ctx Context
 * @param hand_size Size of the bot's hand
 * @param hand_cnt Cards in the bot's hand
 * @param weights The bot's weights
 */
static int scoreLead(const Move* m, const BotCtx* ctx, const int hand_size, const int hand_cnt[RANK_COUNT_SIZE],
                     const BotWeights* weights) {
    if (m->type == MOVE_BOMB || m->type == MOVE_ROCKET)
        return -5000;
    if (m->count == hand_size)
        return 10000; // Immediate win

    int sc = 0;

    sc += m->count * weights->clear_per_card;
    sc -= evalMoveCost(m); // Prefer weaker ranks so control cards are preserved
    if (breaksCombination(m, hand_cnt))
        sc -= weights->break_combo_penalty;
    sc -= kickerPenalty(m, hand_cnt, weights); // Penalty for using control cards or breaking bombs for a kicker
    const int pld = weights->pos_lead_divisor > 0 ? weights->pos_lead_divisor : 1;
    sc += positionAfterMove(hand_cnt, m, hand_size) / pld; // Post-move position
    // Speed up the score of moves that clear more cards in the endgame
    if (ctx->endgame)
        sc += m->count * weights->endgame_clear;

    // In the early game add a bonus for chains that reduce hand size quickly
    if (!ctx->endgame) {
        switch (m->type) {
            case MOVE_STRAIGHT:
            case MOVE_PAIR_STRAIGHT:
            case MOVE_TRIPLE_STRAIGHT:
            case MOVE_TRIPLE_STRAIGHT_SINGLES:
            case MOVE_TRIPLE_STRAIGHT_PAIRS:
                sc += m->length * weights->chain_length;
                break;
            default:
                break;
        }
    }

    // Feeder penalty: discourage leading big combos so the gatekeeper gets the chance
    if (ctx->is_feeder)
        sc -= m->count * weights->feeder_penalty;

    // History inference bonuses
    if (ctx->is_peasant) {
        const int lead_bt = moveBroadType(m->type);
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
 * @param m Candidate move
 * @param ctx Context
 * @param hand_size Size of the bot's hand
 * @param hand_cnt Cards in the bot's hand
 * @param g The current game state
 * @param weights The bot's weights
 */
static int scoreResponse(const Move* m, const BotCtx* ctx, const int hand_size, const int hand_cnt[RANK_COUNT_SIZE],
                         const GameState* g, const BotWeights* weights) {
    if (m->count == hand_size)
        return 10000;

    int sc = 0;

    sc -= evalMoveCost(m); // Prefer the cheapest card that beats; negate its cost.
    if (breaksCombination(m, hand_cnt))
        sc -= weights->break_combo_penalty; // Avoid wasting a bomb or rocket component
    sc -= kickerPenalty(m, hand_cnt, weights); // Penalty for using control of bomb components as kickers

    // Post-move position bonuses
    const int prd = weights->pos_resp_divisor > 0 ? weights->pos_resp_divisor : 1;
    sc += positionAfterMove(hand_cnt, m, hand_size) / prd;

    // When danger is present, prefer using heavier responses for control.
    if (ctx->danger)
        sc += m->count * weights->danger_per_card;
    // The gatekeeper should play aggressively to stop the landlord.
    if (ctx->is_gatekeeper && g->last_player == g->landlord)
        sc += weights->gatekeeper_bonus;

    // Control card preservation: The bot shouldn't waste 2's or jokers to beat something they don't have to stop.
    // Bid clues: If the landlord bids 3, they most likely have strong controlling cards. Reduce the saving penalty so
    // peasant bots are more willing to control against landlords.
    if (ctx->is_peasant && !ctx->must_stop && !ctx->danger) {
        int mc[RANK_COUNT_SIZE];
        movesCountRanks(m->cards, m->count, mc);

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

    // Must-stop: landlord has ≤ 1 card and will win the next turn if not stopped.
    // Reverse the usual "cheapest beats" preference and instead award the strongest card
    if (ctx->must_stop && m->type != MOVE_BOMB && m->type != MOVE_ROCKET) {
        sc += weights->must_stop_multiplier * evalMoveCost(m);
    }

    return sc;
}

/* --- Bomb worthiness --- */
/**
 * @brief Returns 1 if using a bomb is justified by the current game state.
 * @param ctx Bot context
 * @param g The current game state
 * @param player The bot's player number
 */
static int shouldBomb(const BotCtx* ctx, const GameState* g, const int player) {
    if (ctx->danger)
        return 1; // Always bomb if we are in danger
    // Always bomb if the landlord bid high and we are in the endgame
    if (ctx->endgame && ctx->landlord_bid >= 2)
        return 1;

    // If a member of the opposing team is in an endgame state, the bot should bomb for control.
    if (ctx->is_peasant) {
        if (g->hands[g->landlord].count <= 5)
            return 1;
    } else {
        for (int p = 0; p < GAME_NUM_PLAYERS; p++) {
            if (gameIsPeasant(g, p) && g->hands[p].count <= 5)
                return 1;
        }
    }

    // Initiatives: Use a bomb to seize the lead when our hand is nearly cleaned and in endgame.
    if (ctx->endgame) {
        const int mp = evalMinPlays(g->hands[player].cards, g->hands[player].count);
        if (mp <= 2)
            return 1;
    }

    return 0;
}


/* --- Public functions --- */
int botBidWithWeights(const GameState* g, const int player, const BotWeights* weights) {
    (void) weights;
    const int score = evalBidStrength(g->hands[player].cards, g->hands[player].count);

    int want;
    if (score >= 50)
        want = 3;
    else if (score >= 30)
        want = 2;
    else if (score >= 15)
        want = 1;
    else
        return 0;

    if (want <= g->bid.highest_score) {
        if (g->bid.highest_score < 3)
            want = g->bid.highest_score + 1;
        else
            return 0;
    }

    return want;
}

int botBid(const GameState* g, const int player) { return botBidWithWeights(g, player, &DEFAULT_WEIGHTS); }

Move botPlayWithWeights(const GameState* g, const int player, const BotWeights* weights) {
    const BotWeights* w = resolveWeights(weights);
    BotCtx ctx;
    computeCtx(g, player, &ctx);

    Move buf[BOT_MAX_MOVES];
    const int n = gameLegalMoves(g, player, buf, BOT_MAX_MOVES);

    // If we have no legal moves, we pass
    if (n == 0) {
        const Move pass = {MOVE_PASS, {0}, 0, 0, 0};
        return pass;
    }

    int hand_cnt[RANK_COUNT_SIZE];
    movesCountRanks(g->hands[player].cards, g->hands[player].count, hand_cnt);
    const int hand_size = g->hands[player].count;

    // Peasant cooperation: If our partner controls the table and the landlord isn't an immediate thread, we yield to
    // our partner. Exception: If we can win on this turn, play to win.
    if (!ctx.is_leading && ctx.is_peasant && ctx.partner != PLAYER_NONE && g->last_player == ctx.partner &&
        !ctx.danger) {
        int can_win = 0;
        for (int i = 0; i < n; i++) {
            if (buf[i].type != MOVE_PASS && buf[i].count == hand_size) {
                can_win = 1;
                break;
            }
        }
        if (!can_win) {
            const Move pass = {MOVE_PASS, {0}, 0, 0, 0};
            return pass;
        }
    }

    // Score all the legal moves and separate bomb moves
    Move best = {0};
    best.type = MOVE_INVALID;
    Move best_bomb = {0};
    best_bomb.type = MOVE_INVALID;
    int best_sc = -99999;
    int best_bomb_cost = 99999;

    for (int i = 0; i < n; i++) {
        const Move* m = &buf[i];
        if (m->type == MOVE_PASS)
            continue;

        const int is_bomb = (m->type == MOVE_BOMB || m->type == MOVE_ROCKET);

        if (is_bomb) {
            const int cost = evalMoveCost(m);
            if (best_bomb.type == MOVE_INVALID || cost < best_bomb_cost) {
                best_bomb = *m;
                best_bomb_cost = cost;
            }
            continue;
        }

        const int sc = ctx.is_leading ? scoreLead(m, &ctx, hand_size, hand_cnt, w)
                                      : scoreResponse(m, &ctx, hand_size, hand_cnt, g, w);

        if (best.type == MOVE_INVALID || sc > best_sc) {
            best = *m;
            best_sc = sc;
        }
    }

    // Final move selection: Prefer a normal move until bombing is necessary.
    if (ctx.is_leading) {
        if (best.type != MOVE_INVALID)
            return best;
        if (best_bomb.type != MOVE_INVALID)
            return best_bomb;
    } else {
        if (best.type != MOVE_INVALID && best_sc < 0 && !ctx.danger && !ctx.must_stop) {
            const Move pass = {MOVE_PASS, {0}, 0, 0, 0};
            return pass;
        }
        if (best.type != MOVE_INVALID)
            return best;
        if (best_bomb.type != MOVE_INVALID && shouldBomb(&ctx, g, player))
            return best_bomb;
    }

    const Move pass = {MOVE_PASS, {0}, 0, 0, 0};
    return pass;
}

Move botPlay(const GameState* g, const int player) { return botPlayWithWeights(g, player, &DEFAULT_WEIGHTS); }
