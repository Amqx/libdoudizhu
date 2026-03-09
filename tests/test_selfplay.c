/**
 * @file test_selfplay.c
 * @brief Tests for the self-play framework
 * @author Jonathan
 * @date 09-Mar-26
 */

#include "selfplay.h"
#include "test_framework.h"

static void test_match_stats_sum_correctly(void) {
    begin_suite("selfplay: match stats sum to completed games");

    const BotWeights *weights[GAME_NUM_PLAYERS] = {
        bot_default_weights(), bot_default_weights(), bot_default_weights()
    };
    SelfPlayStats stats;
    int seat_sum = 0;

    selfplay_run_matches(12, 1234u, weights, &stats);

    for (int p = 0; p < GAME_NUM_PLAYERS; p++) seat_sum += stats.seat_wins[p];

    EXPECT_EQ(stats.requested_games, 12, "requested game count recorded");
    EXPECT_EQ(stats.completed_games, 12, "requested number of games completed");
    EXPECT_EQ(seat_sum, stats.completed_games, "seat wins sum to completed games");
    EXPECT_EQ(stats.landlord_wins + stats.peasant_wins, stats.completed_games,
              "team outcome split sums to completed games");
}

static void test_tuner_respects_bounds(void) {
    begin_suite("selfplay: tuner mutates within configured bounds");

    SelfPlayTuneConfig cfg = {
        .generations = 8,
        .games_per_generation = 6,
        .initial_step = 5,
        .min_weight = 0,
        .max_weight = 80,
        .seed = 99u,
    };
    SelfPlayTuneResult result;
    const int count = bot_weights_count();

    selfplay_tune(bot_default_weights(), &cfg, &result);

    EXPECT_EQ(result.generations_attempted, 8, "generation counter matches config");
    EXPECT(result.final_arena.completed_games == cfg.games_per_generation,
           "final arena produced requested number of completed games");

    for (int i = 0; i < count; i++) {
        const int value = bot_weights_get(&result.best_weights, i);
        EXPECT(value >= cfg.min_weight && value <= cfg.max_weight,
               "all tuned weights remain within configured bounds");
    }
}

int main(void) {
    test_match_stats_sum_correctly();
    test_tuner_respects_bounds();
    PRINT_RESULTS();
    RETURN_TEST_RESULT();
}
