/**
 * @file test_selfplay.c
 * @brief Tests for the self-play framework
 * @author Peng Yang Deng, Emma Le
 * @date 09-Mar-26
 */

#include "selfplay.h"
#include "test_framework.h"

static void testMatchStatsSumCorrectly(void) {
    beginSuite("selfplay: match stats sum to completed games");

    const BotWeights* weights[GAME_NUM_PLAYERS] = {botDefaultWeights(), botDefaultWeights(), botDefaultWeights()};
    SelfPlayStats stats;
    int seat_sum = 0;

    selfplayRunMatches(12, 1234u, weights, &stats);

    for (int p = 0; p < GAME_NUM_PLAYERS; p++)
        seat_sum += stats.seat_wins[p];

    EXPECT_EQ(stats.requested_games, 12, "requested game count recorded");
    EXPECT_EQ(stats.completed_games, 12, "requested number of games completed");
    EXPECT_EQ(seat_sum, stats.completed_games, "seat wins sum to completed games");
    EXPECT_EQ(stats.landlord_wins + stats.peasant_wins, stats.completed_games,
              "team outcome split sums to completed games");
}

static void testTunerRespectsBounds(void) {
    beginSuite("selfplay: tuner mutates within per-field bounds");

    SelfPlayTuneConfig cfg = {
            .generations = 8,
            .games_per_generation = 6,
            .initial_step = 20,
            .seed = 99u,
    };
    SelfPlayTuneResult result;
    const int count = botWeightsCount();

    selfplayTune(botDefaultWeights(), &cfg, &result);

    EXPECT_EQ(result.generations_attempted, 8, "generation counter matches config");
    EXPECT(result.final_arena.completed_games == cfg.games_per_generation,
           "final arena produced requested number of completed games");

    for (int i = 0; i < count; i++) {
        const int value = botWeightsGet(&result.best_weights, i);
        EXPECT(value >= botWeightsMin(i) && value <= botWeightsMax(i),
               "all tuned weights remain within per-field bounds");
    }
}

int main(void) {
    testMatchStatsSumCorrectly();
    testTunerRespectsBounds();
    PRINT_RESULTS();
    RETURN_TEST_RESULT();
}
