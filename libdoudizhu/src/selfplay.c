/**
 * @file selfplay.c
 * @brief Weighted self-play arena and hill-climbing tuner.
 * @author Peng Yang Deng, Emma Le
 * @date 09-Mar-26
 */

#include "selfplay.h"
#include <stdlib.h>
#include "utils.h"

static int clampInt(const int value, const int lo, const int hi) {
    if (value < lo)
        return lo;
    if (value > hi)
        return hi;
    return value;
}

static int defaultOrPositive(const int value, const int fallback) { return value > 0 ? value : fallback; }

static unsigned int nextRng(unsigned int *state) {
    *state = (*state * 1664525u) + 1013904223u;
    return *state;
}

static void fillDefaultCfg(SelfPlayTuneConfig *cfg) {
    cfg->generations = 64;
    cfg->games_per_generation = 24;
    cfg->initial_step = 20;
    cfg->seed = 1u;
}

static void clampWeights(BotWeights *weights) {
    const int count = botWeightsCount();
    for (int i = 0; i < count; i++) {
        const int value = botWeightsGet(weights, i);
        botWeightsSet(weights, i, clampInt(value, botWeightsMin(i), botWeightsMax(i)));
    }
}

void selfplayStatsReset(SelfPlayStats *stats, const int requested_games) {
    if (!stats)
        return;
    lddzMemset(stats, 0, sizeof(*stats));
    stats->requested_games = requested_games;
}

static void recordCompletedGame(const GameState *g, SelfPlayStats *stats) {
    const int winner = g->winner;
    const int winner_is_landlord = (winner == g->landlord);

    stats->completed_games++;
    stats->seat_wins[winner]++;
    stats->total_score += g->score;

    if (winner_is_landlord) {
        stats->landlord_wins++;
        stats->team_wins[winner]++;
    } else {
        stats->peasant_wins++;
        for (int p = 0; p < GAME_NUM_PLAYERS; p++) {
            if (gameIsPeasant(g, p))
                stats->team_wins[p]++;
        }
    }
}

void selfplayRunMatches(const int num_games, const unsigned int base_seed,
                        const BotWeights *per_player_weights[GAME_NUM_PLAYERS], SelfPlayStats *stats_out) {
    SelfPlayStats local_stats;
    SelfPlayStats *stats = stats_out ? stats_out : &local_stats;
    selfplayStatsReset(stats, num_games);

    int completed = 0;
    unsigned int seed = base_seed;

    while (completed < num_games) {
        GameState g;
        gameInit(&g);
        gameResetDeck(&g);
        gameShuffle(&g, seed++);
        gameDeal(&g);
        gameStartBidding(&g, completed % GAME_NUM_PLAYERS);

        int bid_iters = 0;
        while (g.phase == PHASE_BIDDING && bid_iters++ < 20) {
            const int p = g.bid.current_bidder;
            const BotWeights *weights = per_player_weights ? per_player_weights[p] : NULL;
            const int bid = botBidWithWeights(&g, p, weights);
            if (!gameBid(&g, p, bid))
                break;
        }

        if (g.phase != PHASE_PLAYING) {
            stats->skipped_games++;
            continue;
        }

        int play_iters = 0;
        while (g.phase == PHASE_PLAYING && play_iters++ < 400) {
            const int p = g.current_player;
            const BotWeights *weights = per_player_weights ? per_player_weights[p] : NULL;
            const Move m = botPlayWithWeights(&g, p, weights);
            if (!gamePlay(&g, p, &m))
                break;
        }

        if (g.phase == PHASE_OVER && g.winner >= 0 && g.winner < GAME_NUM_PLAYERS) {
            recordCompletedGame(&g, stats);
        }

        completed++;
    }
}

static int candidateTeamScore(const SelfPlayStats *stats, const int candidate_seat) {
    return stats->team_wins[candidate_seat];
}

static int evaluateCandidate(const BotWeights *incumbent, const BotWeights *candidate, const int games,
                             const unsigned int seed) {
    int candidate_score = 0;

    for (int seat = 0; seat < GAME_NUM_PLAYERS; seat++) {
        const BotWeights *weights[GAME_NUM_PLAYERS] = {incumbent, incumbent, incumbent};
        SelfPlayStats stats;

        weights[seat] = candidate;
        selfplayRunMatches(games, seed + seat * 100003u, weights, &stats);
        candidate_score += candidateTeamScore(&stats, seat);
    }

    return candidate_score;
}

void selfplayTune(const BotWeights *initial_weights, const SelfPlayTuneConfig *cfg_in, SelfPlayTuneResult *result_out) {
    if (!result_out)
        return;

    SelfPlayTuneConfig cfg;
    fillDefaultCfg(&cfg);
    if (cfg_in) {
        cfg.generations = defaultOrPositive(cfg_in->generations, cfg.generations);
        cfg.games_per_generation = defaultOrPositive(cfg_in->games_per_generation, cfg.games_per_generation);
        cfg.initial_step = defaultOrPositive(cfg_in->initial_step, cfg.initial_step);
        cfg.seed = cfg_in->seed ? cfg_in->seed : cfg.seed;
    }

    result_out->initial_weights = initial_weights ? *initial_weights : *botDefaultWeights();
    clampWeights(&result_out->initial_weights);
    result_out->best_weights = result_out->initial_weights;
    selfplayStatsReset(&result_out->final_arena, cfg.games_per_generation);
    result_out->generations_attempted = 0;
    result_out->generations_accepted = 0;
    result_out->last_field_index = -1;
    result_out->last_step = 0;

    BotWeights incumbent = result_out->best_weights;
    unsigned int rng = cfg.seed;
    const int weight_count = botWeightsCount();

    for (int gen = 0; gen < cfg.generations; gen++) {
        const int field = (int) (nextRng(&rng) % (unsigned int) weight_count);
        const int direction = (nextRng(&rng) & 1u) ? 1 : -1;

        /* Step is a percentage of this field's sensible range, annealing from
         * initial_step% down to 1% over the run.  This makes the same step
         * value meaningful for both narrow fields (pos_lead_divisor: 1–10) and
         * wide ones (break_combo_penalty: 0–1000). */
        const int field_min = botWeightsMin(field);
        const int field_max = botWeightsMax(field);
        const int field_range = field_max > field_min ? field_max - field_min : 1;
        int pct = cfg.initial_step - (gen * cfg.initial_step) / cfg.generations;
        if (pct < 1)
            pct = 1;
        int step = (field_range * pct + 50) / 100; /* round to nearest */
        if (step < 1)
            step = 1;

        BotWeights candidate = incumbent;
        const int current = botWeightsGet(&candidate, field);
        const int mutated = clampInt(current + direction * step, field_min, field_max);

        result_out->last_field_index = field;
        result_out->last_step = step;
        result_out->generations_attempted++;

        if (mutated == current)
            continue;
        botWeightsSet(&candidate, field, mutated);

        /* Use the same seed for both evaluations (common random numbers) so
         * that incumbent and candidate play identical deals and any win-rate
         * difference reflects strategy, not luck. */
        const int incumbent_score = evaluateCandidate(&incumbent, &incumbent, cfg.games_per_generation, rng);
        const int candidate_score = evaluateCandidate(&incumbent, &candidate, cfg.games_per_generation, rng);

        if (candidate_score > incumbent_score) {
            incumbent = candidate;
            result_out->best_weights = candidate;
            result_out->generations_accepted++;
        }
    } {
        const BotWeights *arena_weights[GAME_NUM_PLAYERS] = {&incumbent, botDefaultWeights(), botDefaultWeights()};
        selfplayRunMatches(cfg.games_per_generation, cfg.seed + 424242u, arena_weights, &result_out->final_arena);
    }
}
