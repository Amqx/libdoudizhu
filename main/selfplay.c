/**
 * @file selfplay.c
 * @brief CLI driver for weighted self-play tuning.
 * @author Jonathan
 * @date 09-Mar-26
 */

#include <stdio.h>
#include <stdlib.h>
#include "selfplay.h"

static void print_weights(const BotWeights *weights) {
    const int count = bot_weights_count();
    for (int i = 0; i < count; i++) {
        printf("%s=%d\n", bot_weights_name(i), bot_weights_get(weights, i));
    }
}

int main(int argc, char **argv) {
    SelfPlayTuneConfig cfg = {
        .generations = 1024,
        .games_per_generation = 256,
        .initial_step = 30,
        .min_weight = 0,
        .max_weight = 1000,
        .seed = 10u,
    };
    SelfPlayTuneResult result;

    if (argc > 1) cfg.generations = atoi(argv[1]);
    if (argc > 2) cfg.games_per_generation = atoi(argv[2]);
    if (argc > 3) cfg.initial_step = atoi(argv[3]);
    if (argc > 4) cfg.seed = (unsigned int) strtoul(argv[4], NULL, 10);

    selfplay_tune(bot_default_weights(), &cfg, &result);

    printf("attempted=%d accepted=%d\n",
           result.generations_attempted, result.generations_accepted);
    /* final_arena: seat 0 carries the tuned weights; seats 1+2 use defaults.
     * seat0_wins lets the combine step rank shards by tuned-bot head-to-head wins. */
    printf("final_arena completed=%d skipped=%d landlord=%d peasants=%d"
           " seat0_wins=%d total_score=%d\n",
           result.final_arena.completed_games,
           result.final_arena.skipped_games,
           result.final_arena.landlord_wins,
           result.final_arena.peasant_wins,
           result.final_arena.seat_wins[0],
           result.final_arena.total_score);
    print_weights(&result.best_weights);
    return 0;
}
