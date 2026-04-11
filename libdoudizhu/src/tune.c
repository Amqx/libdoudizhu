/**
 * @file tune.c
 * @brief Weight introspection, mutation, and self-play simulation for bot tuning.
 * @author Peng Yang Deng, Emma Le
 * @date 08-Mar-26
 */

#include "tune.h"
#include <stddef.h>

static const char *const BOT_WEIGHT_NAMES[] = {
    "clear_per_card", "endgame_clear", "chain_length",
    "feeder_penalty", "pos_lead_divisor", "pos_resp_divisor",
    "danger_per_card", "gatekeeper_bonus", "ctrl_2_penalty",
    "ctrl_sj_penalty", "ctrl_bj_penalty", "must_stop_multiplier",
    "break_combo_penalty", "kicker_joker", "kicker_2",
    "kicker_ace", "kicker_king", "kicker_bomb_break",
    "kicker_triple_break", "kicker_pair_break", "bid3_ctrl_reduction",
    "void_threshold", "landlord_weak_bonus", "partner_signal_bonus",
};

/* Per-weight tuning bounds.
 *
 * Keeps the same field order as BotWeights / BOT_WEIGHT_NAMES.
 *
 * Rules of thumb used:
 *   - Divisors (pos_*_divisor): min=1 to prevent divide-by-zero; max=10
 *     because above ~10 the positional term is effectively zeroed out.
 *   - Count fields (void_threshold): small integer range only.
 *   - Penalty/bonus fields: max ≈ 3–5× the default so the optimizer has
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
    /*must_stop_multiplier*/ 1,
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
    /*partner_signal_bonus*/ 0,
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
    /*must_stop_multiplier*/ 8,
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
    /*partner_signal_bonus*/ 40,
};

void botWeightsInit(BotWeights *out) {
    if (out)
        *out = *botDefaultWeights();
}

int botWeightsCount(void) { return sizeof(BotWeights) / sizeof(int); }

const char *botWeightsName(const int index) {
    if (index < 0 || index >= botWeightsCount())
        return NULL;
    return BOT_WEIGHT_NAMES[index];
}

int botWeightsGet(const BotWeights *weights, const int index) {
    const BotWeights *src = weights ? weights : botDefaultWeights();
    if (index < 0 || index >= botWeightsCount())
        return 0;
    return ((const int *) src)[index];
}

int botWeightsSet(BotWeights *weights, const int index, const int value) {
    if (!weights || index < 0 || index >= botWeightsCount())
        return 0;
    ((int *) weights)[index] = value;
    return 1;
}

int botWeightsMin(const int index) {
    if (index < 0 || index >= botWeightsCount())
        return 0;
    return BOT_WEIGHT_MIN[index];
}

int botWeightsMax(const int index) {
    if (index < 0 || index >= botWeightsCount())
        return 0;
    return BOT_WEIGHT_MAX[index];
}

/* ---------------------------------------------------------------------------
 * Self-play simulation driver
 * --------------------------------------------------------------------------- */

void botSimulateWithWeights(const int num_games, const unsigned int base_seed,
                            const BotWeights *per_player_weights[GAME_NUM_PLAYERS], int wins_out[GAME_NUM_PLAYERS]) {
    for (int p = 0; p < GAME_NUM_PLAYERS; p++)
        wins_out[p] = 0;

    int completed = 0;
    unsigned int seed = base_seed;

    while (completed < num_games) {
        GameState g;
        gameInit(&g);
        gameResetDeck(&g);
        gameShuffle(&g, seed++);
        gameDeal(&g);
        gameStartBidding(&g, 0);

        /* Bidding phase */
        int bid_iters = 0;
        while (g.phase == PHASE_BIDDING && bid_iters++ < 20) {
            const int p = g.bid.current_bidder;
            const BotWeights *weights = per_player_weights ? per_player_weights[p] : NULL;
            const int bid = botBidWithWeights(&g, p, weights);
            if (!gameBid(&g, p, bid))
                break;
        }

        /* Skip games where no one wanted to be landlord */
        if (g.phase != PHASE_PLAYING)
            continue;

        /* Playing phase */
        int play_iters = 0;
        while (g.phase == PHASE_PLAYING && play_iters++ < 400) {
            const int p = g.current_player;
            const BotWeights *weights = per_player_weights ? per_player_weights[p] : NULL;
            const Move m = botPlayWithWeights(&g, p, weights);
            if (!gamePlay(&g, p, &m))
                break;
        }

        if (g.phase == PHASE_OVER && g.winner >= 0 && g.winner < GAME_NUM_PLAYERS)
            wins_out[g.winner]++;

        completed++;
    }
}

void botSimulate(const int num_games, const unsigned int base_seed, int wins_out[GAME_NUM_PLAYERS]) {
    botSimulateWithWeights(num_games, base_seed, NULL, wins_out);
}

void botEvaluateWeights(const BotWeights *candidate, const int num_games, const unsigned int base_seed,
                        int *wins_as_landlord, int *wins_as_peasant) {
    /* Series A: candidate at seat 0 (becomes landlord when they win bidding),
     * default weights at seats 1 & 2. */
    const BotWeights *a_weights[GAME_NUM_PLAYERS] = {candidate, NULL, NULL};
    int a_wins[GAME_NUM_PLAYERS];
    botSimulateWithWeights(num_games, base_seed, a_weights, a_wins);
    if (wins_as_landlord)
        *wins_as_landlord = a_wins[0];

    /* Series B: default at seat 0, candidate at seats 1 & 2.
     * A peasant-team win is any game the landlord (seat 0) did NOT win. */
    const BotWeights *b_weights[GAME_NUM_PLAYERS] = {NULL, candidate, candidate};
    int b_wins[GAME_NUM_PLAYERS];
    botSimulateWithWeights(num_games, base_seed + (unsigned int) num_games, b_weights, b_wins);
    if (wins_as_peasant)
        *wins_as_peasant = b_wins[1] + b_wins[2];
}
