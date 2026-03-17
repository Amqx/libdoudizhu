/**
 * @file selfplay.c
 * @brief Weighted self-play arena and hill-climbing tuner.
 * @author Jonathan
 * @date 09-Mar-26
 */

#include "selfplay.h"
#include "utils.h"
#include <stdlib.h>

static int clamp_int(const int value, const int lo, const int hi) {
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

static int default_or_positive(const int value, const int fallback) {
    return value > 0 ? value : fallback;
}

static unsigned int next_rng(unsigned int *state) {
    *state = (*state * 1664525u) + 1013904223u;
    return *state;
}

static void fill_default_cfg(SelfPlayTuneConfig *cfg) {
    cfg->generations = 64;
    cfg->games_per_generation = 24;
    cfg->initial_step = 20;
    cfg->seed = 1u;
}

static void clamp_weights(BotWeights *weights) {
    const int count = bot_weights_count();
    for (int i = 0; i < count; i++) {
        const int value = bot_weights_get(weights, i);
        bot_weights_set(weights, i, clamp_int(value, bot_weights_min(i), bot_weights_max(i)));
    }
}

void selfplay_stats_reset(SelfPlayStats *stats, const int requested_games) {
    if (!stats) return;
    lddz_memset(stats, 0, sizeof(*stats));
    stats->requested_games = requested_games;
}

static void record_completed_game(const GameState *g, SelfPlayStats *stats) {
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
            if (game_is_peasant(g, p)) stats->team_wins[p]++;
        }
    }
}

void selfplay_run_matches(const int num_games, const unsigned int base_seed,
                          const BotWeights *per_player_weights[GAME_NUM_PLAYERS],
                          SelfPlayStats *stats_out) {
    SelfPlayStats local_stats;
    SelfPlayStats *stats = stats_out ? stats_out : &local_stats;
    selfplay_stats_reset(stats, num_games);

    int completed = 0;
    unsigned int seed = base_seed;

    while (completed < num_games) {
        GameState g;
        game_init(&g);
        game_reset_deck(&g);
        game_shuffle(&g, seed++);
        game_deal(&g);
        game_start_bidding(&g, completed % GAME_NUM_PLAYERS);

        int bid_iters = 0;
        while (g.phase == PHASE_BIDDING && bid_iters++ < 20) {
            const int p = g.bid.current_bidder;
            const BotWeights *weights = per_player_weights ? per_player_weights[p] : NULL;
            const int bid = bot_bid_with_weights(&g, p, weights);
            if (!game_bid(&g, p, bid)) break;
        }

        if (g.phase != PHASE_PLAYING) {
            stats->skipped_games++;
            continue;
        }

        int play_iters = 0;
        while (g.phase == PHASE_PLAYING && play_iters++ < 400) {
            const int p = g.current_player;
            const BotWeights *weights = per_player_weights ? per_player_weights[p] : NULL;
            const Move m = bot_play_with_weights(&g, p, weights);
            if (!game_play(&g, p, &m)) break;
        }

        if (g.phase == PHASE_OVER && g.winner >= 0 && g.winner < GAME_NUM_PLAYERS) {
            record_completed_game(&g, stats);
        }

        completed++;
    }
}

static int candidate_team_score(const SelfPlayStats *stats, const int candidate_seat) {
    return stats->team_wins[candidate_seat];
}

static int evaluate_candidate(const BotWeights *incumbent, const BotWeights *candidate,
                              const int games, unsigned int seed) {
    int candidate_score = 0;

    for (int seat = 0; seat < GAME_NUM_PLAYERS; seat++) {
        const BotWeights *weights[GAME_NUM_PLAYERS] = {incumbent, incumbent, incumbent};
        SelfPlayStats stats;

        weights[seat] = candidate;
        selfplay_run_matches(games, seed + (unsigned int) (seat * 100003u), weights, &stats);
        candidate_score += candidate_team_score(&stats, seat);
    }

    return candidate_score;
}

void selfplay_tune(const BotWeights *initial_weights, const SelfPlayTuneConfig *cfg_in,
                   SelfPlayTuneResult *result_out) {
    if (!result_out) return;

    SelfPlayTuneConfig cfg;
    fill_default_cfg(&cfg);
    if (cfg_in) {
        cfg.generations = default_or_positive(cfg_in->generations, cfg.generations);
        cfg.games_per_generation = default_or_positive(cfg_in->games_per_generation, cfg.games_per_generation);
        cfg.initial_step = default_or_positive(cfg_in->initial_step, cfg.initial_step);
        cfg.seed = cfg_in->seed ? cfg_in->seed : cfg.seed;
    }

    result_out->initial_weights = initial_weights ? *initial_weights : *bot_default_weights();
    clamp_weights(&result_out->initial_weights);
    result_out->best_weights = result_out->initial_weights;
    selfplay_stats_reset(&result_out->final_arena, cfg.games_per_generation);
    result_out->generations_attempted = 0;
    result_out->generations_accepted = 0;
    result_out->last_field_index = -1;
    result_out->last_step = 0;

    BotWeights incumbent = result_out->best_weights;
    unsigned int rng = cfg.seed;
    const int weight_count = bot_weights_count();

    for (int gen = 0; gen < cfg.generations; gen++) {
        const int field = (int) (next_rng(&rng) % (unsigned int) weight_count);
        const int direction = (next_rng(&rng) & 1u) ? 1 : -1;

        /* Step is a percentage of this field's sensible range, annealing from
         * initial_step% down to 1% over the run.  This makes the same step
         * value meaningful for both narrow fields (pos_lead_divisor: 1–10) and
         * wide ones (break_combo_penalty: 0–1000). */
        const int field_min = bot_weights_min(field);
        const int field_max = bot_weights_max(field);
        const int field_range = field_max > field_min ? field_max - field_min : 1;
        int pct = cfg.initial_step - (gen * cfg.initial_step) / cfg.generations;
        if (pct < 1) pct = 1;
        int step = (field_range * pct + 50) / 100; /* round to nearest */
        if (step < 1) step = 1;

        BotWeights candidate = incumbent;
        const int current = bot_weights_get(&candidate, field);
        const int mutated = clamp_int(current + direction * step, field_min, field_max);

        result_out->last_field_index = field;
        result_out->last_step = step;
        result_out->generations_attempted++;

        if (mutated == current) continue;
        bot_weights_set(&candidate, field, mutated);

        /* Use the same seed for both evaluations (common random numbers) so
         * that incumbent and candidate play identical deals and any win-rate
         * difference reflects strategy, not luck. */
        const int incumbent_score = evaluate_candidate(&incumbent, &incumbent,
                                                       cfg.games_per_generation, rng);
        const int candidate_score = evaluate_candidate(&incumbent, &candidate,
                                                       cfg.games_per_generation, rng);

        if (candidate_score > incumbent_score) {
            incumbent = candidate;
            result_out->best_weights = candidate;
            result_out->generations_accepted++;
        }
    } {
        const BotWeights *arena_weights[GAME_NUM_PLAYERS] = {
            &incumbent, bot_default_weights(), bot_default_weights()
        };
        selfplay_run_matches(cfg.games_per_generation, cfg.seed + 424242u,
                             arena_weights, &result_out->final_arena);
    }
}
