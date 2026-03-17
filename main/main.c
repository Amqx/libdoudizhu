/**
 * @file main.c
 * @brief CLI Dou Di Zhu — human player vs two bots
 * @author Jonathan
 * @date 08-Mar-26
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Fix console output in Windows systems
#ifdef _WIN32
#include <Windows.h>
#endif

#include "display.h"
#include "game.h"
#include "bot.h"
#include "eval.h"

#define HUMAN       0   // Human is always player 0
#define MAX_LEGAL   512

/**
 * Reads an integer between low and high. Re-prompts on bad inputs.
 * @param prompt Prompt to print.
 * @param lo Minimum low number.
 * @param hi Maximum high numbers.
 * @return Returns the number inputted.
 */
static int read_int(const char *prompt, const int lo, const int hi) {
    for (;;) {
        fputs(prompt, stdout);
        fflush(stdout);
        int v;
        char ch;
        if (scanf("%d", &v) == 1 && v >= lo && v <= hi) {
            while ((ch = (char) getchar()) != '\n' && ch != (char) EOF) {
            }
            return v;
        }
        while ((ch = (char) getchar()) != '\n' && ch != (char) EOF) {
        }
        printf("  Please enter a number between %d and %d.\n", lo, hi);
    }
}

/**
 * Gets the user's bid for a game.
 * @param g The currently playing game.
 * @return The user's bid.
 */
static int human_bid(const GameState *g) {
    const int lo = (g->bid.highest_score < 3) ? g->bid.highest_score + 1 : 4;

    printf("\n  Your hand: ");
    print_hand_sorted(&g->hands[HUMAN]);
    printf("\n");
    printf("  Hand strength: %d\n",
           eval_hand_score(g->hands[HUMAN].cards, g->hands[HUMAN].count));
    printf("  Current highest bid: %d\n\n", g->bid.highest_score);

    if (lo > 3) {
        puts("  Cannot outbid — passing automatically.");
        return 0;
    }

    printf("  Options: 0=Pass");
    for (int i = lo; i <= 3; i++) printf(", %d", i);
    puts("");

    for (;;) {
        const int v = read_int("  Your bid: ", 0, 3);
        if (v == 0) return 0;
        if (v >= lo) return v;
        printf("  Bid must be 0 (pass) or at least %d.\n", lo);
    }
}

static Move human_pick_move(const GameState *g) {
    Move buf[MAX_LEGAL];
    const int n = game_legal_moves(g, HUMAN, buf, MAX_LEGAL);
    const int is_leading = (g->last_player == PLAYER_NONE || g->last_player == HUMAN);

    print_legal_moves(buf, n, is_leading);

    const int lo = is_leading ? 1 : 0;
    const int hi = n;

    for (;;) {
        char prompt[64];
        snprintf(prompt, sizeof(prompt), "  Choose (%d–%d): ", lo, hi);
        const int choice = read_int(prompt, lo, hi);

        if (choice == 0) {
            const Move pass = {MOVE_PASS};
            return pass;
        }
        const int idx = choice - 1;
        if (idx >= 0 && idx < n) return buf[idx];
        printf("  Invalid selection, try again.\n");
    }
}

/**
 * Apply standard Doudizhu scoring to players.
 * @param scores List of scores for players
 * @param g Finished game state.
 */
static void apply_scores(int scores[GAME_NUM_PLAYERS], const GameState *g) {
    const int s = g->score;
    const int landlord_won = !game_is_peasant(g, g->winner);

    for (int p = 0; p < GAME_NUM_PLAYERS; p++) {
        if (p == g->landlord)
            scores[p] += landlord_won ? 2 * s : -2 * s;
        else
            scores[p] += landlord_won ? -s : s;
    }
}

/**
 * Plays a round of Doudizhu.
 * @param scores Player's scores coming into the round.
 */
static void play_round(int scores[GAME_NUM_PLAYERS]) {
    GameState g;
    game_init(&g);
    game_reset_deck(&g);
    game_shuffle(&g, (unsigned int) time(NULL));
    game_deal(&g);

    // Bidding
    banner("BIDDING");
    game_start_bidding(&g, HUMAN);
    while (g.phase == PHASE_BIDDING) {
        const int p = g.bid.current_bidder;
        int bid;

        if (p == HUMAN) {
            printf("\n  Your turn to bid.\n");
            bid = human_bid(&g);
            printf("  You bid: %d%s\n", bid, bid == 0 ? " (pass)" : "");
        } else {
            bid = bot_bid(&g, p);
            printf("  %s bids: %d%s\n", pname(p), bid, bid == 0 ? " (pass)" : "");
        }

        /* game_bid can fail if the bot tried an illegal value; fall back to pass */
        if (!game_bid(&g, p, bid)) game_bid(&g, p, 0);
    }

    // If we didn't get bids from anyone we finish the round.
    if (g.phase != PHASE_PLAYING) {
        puts("\n  Nobody bid — no landlord this round.");
        return; /* scores unchanged */
    }

    // Reveal kitty
    banner("LANDLORD DETERMINED");
    printf("  %s is the Landlord  (bid score: %d)\n\n", pname(g.landlord), g.base_score);
    printf("  Kitty: ");
    for (int i = 0; i < GAME_KITTY_SIZE; i++) {
        if (i) putchar(' ');
        print_card(g.kitty[i]);
    }
    puts("\n");

    // Print human's hand
    if (g.landlord == HUMAN) {
        printf("  Your hand now (%d cards): ", g.hands[HUMAN].count);
        print_hand_sorted(&g.hands[HUMAN]);
        puts("\n");
    } else {
        printf("  Your hand (%d cards): ", g.hands[HUMAN].count);
        print_hand_sorted(&g.hands[HUMAN]);
        puts("\n");
    }

    // Gameplay loop
    banner("PLAYING");
    print_status(&g);
    while (g.phase == PHASE_PLAYING) {
        const int p = g.current_player;

        if (p == HUMAN) {
            hr();
            printf("  YOUR TURN\n");
            print_status(&g);
            printf("  Your hand (%d cards): ", g.hands[HUMAN].count);
            print_hand_sorted(&g.hands[HUMAN]);
            puts("\n");

            const Move m = human_pick_move(&g);
            game_play(&g, HUMAN, &m);

            printf("  You played: ");
            print_move_inline(&m);
            puts("\n");
        } else {
            const Move m = bot_play(&g, p);
            game_play(&g, p, &m);

            printf("  %s: ", pname(p));
            print_move_inline(&m);
            putchar('\n');

            if (m.type != MOVE_PASS && g.phase == PHASE_PLAYING) {
                print_status(&g);
            }
        }
    }

    // Result
    banner("ROUND OVER");
    printf("  Winner:  %s\n", pname(g.winner));
    printf("  Team:    %s wins!\n", game_is_peasant(&g, g.winner) ? "Peasant" : "Landlord");
    printf("  Score:   %d", g.score);
    if (g.bomb_count > 0)
        printf("  (base %d × 2^%d bombs)", g.base_score, g.bomb_count);
    puts("\n");

    apply_scores(scores, &g);
    print_scoreboard(scores);
}

int main(void) {
    // Fix the printing being weird on Windows
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    puts("\n");
    puts("Doudizhu\n");
    puts("  Rules: bid to become Landlord, then empty your hand first to win.\n");
    puts("  Bombs (four of a kind) and the rocket (both jokers) beat everything.\n");

    // Initial score count
    int scores[GAME_NUM_PLAYERS] = {0, 0, 0};

    // Keep playing until the user wants to quit
    for (;;) {
        print_scoreboard(scores);
        play_round(scores);
        puts("  Play again? (y/n): ");
        fflush(stdout);
        char ch = 0;
        scanf(" %c", &ch);
        if (ch != 'y' && ch != 'Y') break;
    }

    puts("\n  Thanks for playing!\n");
    return 0;
}
