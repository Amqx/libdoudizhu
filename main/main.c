/**
 * @file main.c
 * @brief CLI Dou Di Zhu — human player vs two bots
 * @author Jonathan
 * @date 08-Mar-26
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Fix console output in Windows systems
#ifdef _WIN32
#include <Windows.h>
#endif

#include "game.h"
#include "bot.h"
#include "eval.h"

#define HUMAN       0   /* Human is always player 0 */
#define MAX_LEGAL   512

/* ============================================================================
 * Display helpers
 * ========================================================================== */

static const char *rank_str(int r) {
    static const char *t[] = {
        "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K", "A", "2", "sJ", "bJ"
    };
    return (r >= 0 && r < 15) ? t[r] : "?";
}

static void print_card(Card c) {
    static const char *suits[] = {"\xe2\x99\xa0", "\xe2\x99\xa5", "\xe2\x99\xa6", "\xe2\x99\xa3"}; /* ♠♥♦♣ */
    if (c == 52) fputs("sJ", stdout);
    else if (c == 53) fputs("bJ", stdout);
    else printf("%s%s", rank_str(CARD_RANK(c)), suits[CARD_SUIT(c)]);
}

/* Sort cards by rank into a temporary buffer and print. */
static void sort_cards_into(Card *dst, const Card *src, int n) {
    memcpy(dst, src, (size_t) n * sizeof(Card));
    for (int i = 1; i < n; i++) {
        Card key = dst[i];
        int j = i - 1;
        while (j >= 0 && CARD_RANK(dst[j]) > CARD_RANK(key)) {
            dst[j + 1] = dst[j];
            j--;
        }
        dst[j + 1] = key;
    }
}

static void print_hand_sorted(const Hand *h) {
    Card tmp[GAME_MAX_HAND_SIZE];
    sort_cards_into(tmp, h->cards, h->count);
    for (int i = 0; i < h->count; i++) {
        if (i) putchar(' ');
        print_card(tmp[i]);
    }
}

static void print_card_with_gap(Card c, int *need_gap) {
    if (*need_gap) putchar(' ');
    print_card(c);
    *need_gap = 1;
}

static void print_rank_group(const Card *sorted, int n, int rank, int copies,
                             int used[RANK_COUNT_SIZE], int *need_gap) {
    int seen = 0;
    for (int i = 0; i < n && copies > 0; i++) {
        if (CARD_RANK(sorted[i]) != rank) continue;
        if (seen < used[rank]) {
            seen++;
            continue;
        }
        print_card_with_gap(sorted[i], need_gap);
        used[rank]++;
        copies--;
    }
}

static void print_move_cards_by_structure(const Move *m) {
    Card tmp[MOVE_MAX_CARDS];
    int cnt[RANK_COUNT_SIZE] = {0};
    int used[RANK_COUNT_SIZE] = {0};
    int need_gap = 0;

    sort_cards_into(tmp, m->cards, m->count);
    moves_count_ranks(tmp, m->count, cnt);

    switch (m->type) {
        case MOVE_TRIPLE_SINGLE:
            print_rank_group(tmp, m->count, m->rank, 3, used, &need_gap);
            for (int r = 0; r < RANK_COUNT_SIZE; r++)
                print_rank_group(tmp, m->count, r, cnt[r] - used[r], used, &need_gap);
            return;
        case MOVE_TRIPLE_PAIR:
            print_rank_group(tmp, m->count, m->rank, 3, used, &need_gap);
            for (int r = 0; r < RANK_COUNT_SIZE; r++)
                print_rank_group(tmp, m->count, r, cnt[r] - used[r], used, &need_gap);
            return;
        case MOVE_TRIPLE_STRAIGHT_SINGLES:
        case MOVE_TRIPLE_STRAIGHT_PAIRS:
            for (int r = m->rank; r < m->rank + m->length; r++)
                print_rank_group(tmp, m->count, r, 3, used, &need_gap);
            for (int r = 0; r < RANK_COUNT_SIZE; r++)
                print_rank_group(tmp, m->count, r, cnt[r] - used[r], used, &need_gap);
            return;
        case MOVE_FOUR_TWO_SINGLES:
        case MOVE_FOUR_TWO_PAIRS:
            print_rank_group(tmp, m->count, m->rank, 4, used, &need_gap);
            for (int r = 0; r < RANK_COUNT_SIZE; r++)
                print_rank_group(tmp, m->count, r, cnt[r] - used[r], used, &need_gap);
            return;
        default:
            for (int i = 0; i < m->count; i++)
                print_card_with_gap(tmp[i], &need_gap);
            return;
    }
}

static void print_move_inline(const Move *m) {
    if (m->type == MOVE_PASS) {
        fputs("[Pass]", stdout);
        return;
    }
    printf("[%s]", moves_type_name(m->type));
    putchar(' ');
    print_move_cards_by_structure(m);
}

static void hr(void) { puts("─────────────────────────────────────────────────────────"); }

static void banner(const char *s) {
    putchar('\n');
    hr();
    printf("  %s\n", s);
    hr();
}

static const char *pname(int p) {
    if (p == HUMAN) return "You";
    if (p == 1) return "Bot 1";
    return "Bot 2";
}

/* Print the current table state and card counts for all players. */
static void print_status(const GameState *g) {
    putchar('\n');
    for (int p = 0; p < GAME_NUM_PLAYERS; p++) {
        const char *role = game_is_peasant(g, p) ? "Peasant" : "Landlord";
        printf("  %-8s [%-8s]  %2d card%s\n",
               pname(p), role,
               g->hands[p].count, g->hands[p].count == 1 ? "" : "s");
    }
    putchar('\n');
    if (g->last_player == PLAYER_NONE) {
        puts("  Table: (clear — lead freely)");
    } else {
        printf("  Table: ");
        print_move_inline(&g->last_move);
        printf("   ← %s\n", pname(g->last_player));
    }
    putchar('\n');
}

/* ============================================================================
 * Input helpers
 * ========================================================================== */

/* Read an integer in [lo, hi], re-prompting on bad input. */
static int read_int(const char *prompt, int lo, int hi) {
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

/* ============================================================================
 * Bidding
 * ========================================================================== */

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
        int v = read_int("  Your bid: ", 0, 3);
        if (v == 0) return 0;
        if (v >= lo) return v;
        printf("  Bid must be 0 (pass) or at least %d.\n", lo);
    }
}

/* ============================================================================
 * Move selection
 * ============================================================================
 *
 * Legal moves are displayed grouped by MoveType so the human can quickly
 * locate what they want. When following (not leading), option 0 is always
 * available as a pass.
 */

static void print_legal_moves(const Move *moves, int n, int is_leading) {
    putchar('\n');
    if (!is_leading) printf("  %4d: [Pass]\n", 0);

    MoveType cur = MOVE_INVALID;
    for (int i = 0; i < n; i++) {
        if (moves[i].type != cur) {
            cur = moves[i].type;
            printf("\n  -- %s --\n", moves_type_name(cur));
        }
        printf("  %4d: ", i + 1);
        print_move_inline(&moves[i]);
        putchar('\n');
    }
    putchar('\n');
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

/* ============================================================================
 * Scoreboard
 * ========================================================================== */

static void print_scoreboard(const int scores[GAME_NUM_PLAYERS]) {
    banner("SCOREBOARD");
    for (int p = 0; p < GAME_NUM_PLAYERS; p++)
        printf("  %-8s  %+d\n", pname(p), scores[p]);
    putchar('\n');
}

/* Apply standard Dou Di Zhu payout to the running scores.
 *
 * The round score S = base_bid * 2^bombs.
 *   Landlord wins: landlord +2S, each peasant -S
 *   Peasants  win: landlord -2S, each peasant +S
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

/* ============================================================================
 * Game loop
 * ========================================================================== */

static void play_round(int scores[GAME_NUM_PLAYERS]) {
    GameState g;
    game_init(&g);
    game_reset_deck(&g);
    game_shuffle(&g, (unsigned int) time(NULL));
    game_deal(&g);

    /* ------------------------------------------------------------------
     * Bidding
     * ------------------------------------------------------------------ */
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

    if (g.phase != PHASE_PLAYING) {
        puts("\n  Nobody bid — no landlord this round.");
        return; /* scores unchanged */
    }

    /* ------------------------------------------------------------------
     * Reveal kitty and roles
     * ------------------------------------------------------------------ */
    banner("LANDLORD DETERMINED");
    printf("  %s is the Landlord  (bid score: %d)\n\n", pname(g.landlord), g.base_score);
    printf("  Kitty: ");
    for (int i = 0; i < GAME_KITTY_SIZE; i++) {
        if (i) putchar(' ');
        print_card(g.kitty[i]);
    }
    puts("\n");

    if (g.landlord == HUMAN) {
        printf("  Your hand now (%d cards): ", g.hands[HUMAN].count);
        print_hand_sorted(&g.hands[HUMAN]);
        puts("\n");
    } else {
        printf("  Your hand (%d cards): ", g.hands[HUMAN].count);
        print_hand_sorted(&g.hands[HUMAN]);
        puts("\n");
    }

    /* ------------------------------------------------------------------
     * Playing
     * ------------------------------------------------------------------ */
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

            /* Only print full status after bot plays non-pass so table changes
             * are clearly visible; suppress after pass to reduce noise. */
            if (m.type != MOVE_PASS && g.phase == PHASE_PLAYING) {
                print_status(&g);
            }
        }
    }

    /* ------------------------------------------------------------------
     * Result
     * ------------------------------------------------------------------ */
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

/* ============================================================================
 * Entry point
 * ========================================================================== */

int main(void) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    puts("\n");
    puts("  ╔══════════════════════════════════════════════╗");
    puts("  ║     Dou Di Zhu  —  Fight the Landlord        ║");
    puts("  ║     You (player 0)  vs  Bot 1  &  Bot 2      ║");
    puts("  ╚══════════════════════════════════════════════╝");
    puts("\n  Rules: bid to become Landlord, then play your hand empty first.");
    puts("  Bombs (four-of-a-kind) and the Rocket (both Jokers) beat everything.\n");

    int scores[GAME_NUM_PLAYERS] = {0, 0, 0};

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
