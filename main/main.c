/**
 * @file main.c
 * @brief CLI Dou Di Zhu — human player vs two bots
 * @author Peng Yang Deng (), Emma Le ()
 * @date 08-Mar-26
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Fix console output in Windows systems
#ifdef _WIN32
#include <Windows.h>
#endif

#include "bot.h"
#include "display.h"
#include "eval.h"
#include "game.h"

#define HUMAN 0 // Human is always player 0
#define MAX_LEGAL 512

/**
 * Reads an integer between low and high. Re-prompts on bad inputs.
 * @param prompt Prompt to print.
 * @param lo Minimum low number.
 * @param hi Maximum high numbers.
 * @return Returns the number inputted.
 */
static int readInt(const char *prompt, const int lo, const int hi) {
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
static int humanBid(const GameState *g) {
    const int lo = (g->bid.highest_score < 3) ? g->bid.highest_score + 1 : 4;

    printf("\n  Your hand: ");
    printHandSorted(&g->hands[HUMAN]);
    printf("\n");
    printf("  Hand strength: %d\n", evalHandScore(g->hands[HUMAN].cards, g->hands[HUMAN].count));
    printf("  Current highest bid: %d\n\n", g->bid.highest_score);

    if (lo > 3) {
        puts("  Cannot outbid — passing automatically.");
        return 0;
    }

    printf("  Options: 0=Pass");
    for (int i = lo; i <= 3; i++)
        printf(", %d", i);
    puts("");

    for (;;) {
        const int v = readInt("  Your bid: ", 0, 3);
        if (v == 0)
            return 0;
        if (v >= lo)
            return v;
        printf("  Bid must be 0 (pass) or at least %d.\n", lo);
    }
}

static Move humanPickMove(const GameState *g) {
    Move buf[MAX_LEGAL];
    const int n = gameLegalMoves(g, HUMAN, buf, MAX_LEGAL);
    const int is_leading = (g->last_player == PLAYER_NONE || g->last_player == HUMAN);

    printLegalMoves(buf, n, is_leading);

    const int lo = is_leading ? 1 : 0;
    const int hi = n;

    for (;;) {
        char prompt[64];
        snprintf(prompt, sizeof(prompt), "  Choose (%d–%d): ", lo, hi);
        const int choice = readInt(prompt, lo, hi);

        if (choice == 0) {
            const Move pass = {MOVE_PASS, {0}, 0, 0, 0};
            return pass;
        }
        const int idx = choice - 1;
        if (idx >= 0 && idx < n)
            return buf[idx];
        printf("  Invalid selection, try again.\n");
    }
}

/**
 * Apply standard Doudizhu scoring to players.
 * @param scores List of scores for players
 * @param g Finished game state.
 */
static void applyScores(int scores[GAME_NUM_PLAYERS], const GameState *g) {
    const int s = g->score;
    const int landlord_won = !gameIsPeasant(g, g->winner);

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
static void playRound(int scores[GAME_NUM_PLAYERS]) {
    GameState g;
    gameInit(&g);
    gameResetDeck(&g);
    gameShuffle(&g, (unsigned int) time(NULL));
    gameDeal(&g);

    // Bidding
    banner("BIDDING");
    gameStartBidding(&g, HUMAN);
    while (g.phase == PHASE_BIDDING) {
        const int p = g.bid.current_bidder;
        int bid;

        if (p == HUMAN) {
            printf("\n  Your turn to bid.\n");
            bid = humanBid(&g);
            printf("  You bid: %d%s\n", bid, bid == 0 ? " (pass)" : "");
        } else {
            bid = botBid(&g, p);
            printf("  %s bids: %d%s\n", pname(p), bid, bid == 0 ? " (pass)" : "");
        }

        /* gameBid can fail if the bot tried an illegal value; fall back to pass */
        if (!gameBid(&g, p, bid))
            gameBid(&g, p, 0);
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
        if (i)
            putchar(' ');
        printCard(g.kitty[i]);
    }
    puts("\n");

    // Print human's hand
    if (g.landlord == HUMAN) {
        printf("  Your hand now (%d cards): ", g.hands[HUMAN].count);
        printHandSorted(&g.hands[HUMAN]);
        puts("\n");
    } else {
        printf("  Your hand (%d cards): ", g.hands[HUMAN].count);
        printHandSorted(&g.hands[HUMAN]);
        puts("\n");
    }

    // Gameplay loop
    banner("PLAYING");
    printStatus(&g);
    while (g.phase == PHASE_PLAYING) {
        const int p = g.current_player;

        if (p == HUMAN) {
            hr();
            printf("  YOUR TURN\n");
            printStatus(&g);
            printf("  Your hand (%d cards): ", g.hands[HUMAN].count);
            printHandSorted(&g.hands[HUMAN]);
            puts("\n");

            const Move m = humanPickMove(&g);
            gamePlay(&g, HUMAN, &m);

            printf("  You played: ");
            printMoveInline(&m);
            puts("\n");
        } else {
            const Move m = botPlay(&g, p);
            gamePlay(&g, p, &m);

            printf("  %s: ", pname(p));
            printMoveInline(&m);
            putchar('\n');

            if (m.type != MOVE_PASS && g.phase == PHASE_PLAYING) {
                printStatus(&g);
            }
        }
    }

    // Result
    banner("ROUND OVER");
    printf("  Winner:  %s\n", pname(g.winner));
    printf("  Team:    %s wins!\n", gameIsPeasant(&g, g.winner) ? "Peasant" : "Landlord");
    printf("  Score:   %d", g.score);
    if (g.bomb_count > 0)
        printf("  (base %d × 2^%d bombs)", g.base_score, g.bomb_count);
    puts("\n");

    applyScores(scores, &g);
    printScoreboard(scores);
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
        printScoreboard(scores);
        playRound(scores);
        puts("  Play again? (y/n): ");
        fflush(stdout);
        char ch = 0;
        scanf(" %c", &ch);
        if (ch != 'y' && ch != 'Y')
            break;
    }

    puts("\n  Thanks for playing!\n");
    return 0;
}
