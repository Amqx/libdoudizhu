/**
 * @file display.c
 * @brief Implementations for terminal-based card rendering and UI utilities.
 */

#include "display.h"
#include <stdio.h>
#include <string.h>

/**
 * @brief Helper for rank string lookup.
 * @param r Rank index.
 * @return String representation of the rank.
 */
static const char* rankStr(const int r) {
    const char* t[] = {"3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K", "A", "2", "sJ", "bJ"};
    return (r >= 0 && r < 15) ? t[r] : "?";
}

void printCard(const Card c) {
    const char* suits[] = {"\xe2\x99\xa0", "\xe2\x99\xa5", "\xe2\x99\xa6", "\xe2\x99\xa3"}; /* ♠♥♦♣ */
    if (c == 52)
        fputs("sJ", stdout);
    else if (c == 53)
        fputs("bJ", stdout);
    else
        printf("%s%s", rankStr(CARD_RANK(c)), suits[CARD_SUIT(c)]);
}

/* Stable insertion sort for small card arrays. Sorts by rank only. */
void sortCardsInto(Card dst[], const Card src[], const int n) {
    memcpy(dst, src, (size_t) n * sizeof(Card));
    for (int i = 1; i < n; i++) {
        const Card key = dst[i];
        int j = i - 1;
        while (j >= 0 && CARD_RANK(dst[j]) > CARD_RANK(key)) {
            dst[j + 1] = dst[j];
            j--;
        }
        dst[j + 1] = key;
    }
}

void printHandSorted(const Hand* h) {
    Card tmp[GAME_MAX_HAND_SIZE];
    sortCardsInto(tmp, h->cards, h->count);
    for (int i = 0; i < h->count; i++) {
        if (i)
            putchar(' ');
        printCard(tmp[i]);
    }
}

void printCardWithGap(const Card c, int* need_gap) {
    if (*need_gap)
        putchar(' ');
    printCard(c);
    *need_gap = 1;
}

// Selectively prints a specific number of copies of a rank from a pre-sorted list.
void printRankGroup(const Card sorted[], const int n, const int rank, int copies, int used[RANK_COUNT_SIZE],
                    int* need_gap) {
    int seen = 0;
    for (int i = 0; i < n && copies > 0; i++) {
        if (CARD_RANK(sorted[i]) != rank)
            continue;
        if (seen < used[rank]) {
            seen++;
            continue;
        }
        printCardWithGap(sorted[i], need_gap);
        used[rank]++;
        copies--;
    }
}

/**
 * @brief Prints move cards according to their structural roles (e.g., triplets before kickers).
 * @param m Move to print.
 * For simple cards (singles, doubles, triples, bombs), they are printed in order.
 * For complex moves (3+1, 4+1, 3+2, 4+2, etc.), the triple/ quad is printed first, then the attched cards.
 */
void printMoveCardsByStructure(const Move* m) {
    Card tmp[MOVE_MAX_CARDS];
    int cnt[RANK_COUNT_SIZE] = {0};
    int used[RANK_COUNT_SIZE] = {0};
    int need_gap = 0;

    sortCardsInto(tmp, m->cards, m->count);
    movesCountRanks(tmp, m->count, cnt);

    switch (m->type) {
        case MOVE_TRIPLE_SINGLE:
        case MOVE_TRIPLE_PAIR:
            // Print the triplet first
            printRankGroup(tmp, m->count, m->rank, 3, used, &need_gap);

            // Then all remaining cards
            for (int r = 0; r < RANK_COUNT_SIZE; r++)
                printRankGroup(tmp, m->count, r, cnt[r] - used[r], used, &need_gap);

            return;

        case MOVE_TRIPLE_STRAIGHT_SINGLES:
        case MOVE_TRIPLE_STRAIGHT_PAIRS:
            // Print the sequence of triplets first
            for (int r = m->rank; r < m->rank + m->length; r++)
                printRankGroup(tmp, m->count, r, 3, used, &need_gap);

            // Then print all kickers
            for (int r = 0; r < RANK_COUNT_SIZE; r++)
                printRankGroup(tmp, m->count, r, cnt[r] - used[r], used, &need_gap);
            return;

        case MOVE_FOUR_TWO_SINGLES:
        case MOVE_FOUR_TWO_PAIRS:
            // Print the quad first
            printRankGroup(tmp, m->count, m->rank, 4, used, &need_gap);

            // Then the kickers
            for (int r = 0; r < RANK_COUNT_SIZE; r++)
                printRankGroup(tmp, m->count, r, cnt[r] - used[r], used, &need_gap);

            return;

        default:
            // For simple types, just print sorted by rank
            for (int i = 0; i < m->count; i++)
                printCardWithGap(tmp[i], &need_gap);
    }
}

void printMoveInline(const Move* m) {
    if (m->type == MOVE_PASS) {
        fputs("[Pass]", stdout);
        return;
    }
    printf("[%s]", movesTypeName(m->type));
    putchar(' ');
    printMoveCardsByStructure(m);
}

void hr(void) { puts("─────────────────────────────────────────────────────────"); }

void banner(const char* s) {
    putchar('\n');
    hr();
    printf("  %s\n", s);
    hr();
}

const char* pname(int p) {
    if (p == 0)
        return "You";
    if (p == 1)
        return "Bot 1";
    return "Bot 2";
}

void printStatus(const GameState* g) {
    putchar('\n');
    for (int p = 0; p < GAME_NUM_PLAYERS; p++) {
        const char* role = gameIsPeasant(g, p) ? "Peasant" : "Landlord";
        printf("  %-8s [%-8s]  %2d card%s\n", pname(p), role, g->hands[p].count, g->hands[p].count == 1 ? "" : "s");
    }
    putchar('\n');
    if (g->last_player == PLAYER_NONE) {
        puts("  Table: (clear — lead freely)");
    } else {
        printf("  Table: ");
        printMoveInline(&g->last_move);
        printf("   ← %s\n", pname(g->last_player));
    }
    putchar('\n');
}

void printLegalMoves(const Move moves[], const int n, const int is_leading) {
    putchar('\n');
    if (!is_leading)
        printf("  %4d: [Pass]\n", 0);

    MoveType cur = MOVE_INVALID;
    for (int i = 0; i < n; i++) {
        if (moves[i].type != cur) {
            cur = moves[i].type;
            printf("\n  -- %s --\n", movesTypeName(cur));
        }
        printf("  %4d: ", i + 1);
        printMoveInline(&moves[i]);
        putchar('\n');
    }
    putchar('\n');
}

void printScoreboard(const int scores[GAME_NUM_PLAYERS]) {
    banner("SCOREBOARD");
    for (int p = 0; p < GAME_NUM_PLAYERS; p++)
        printf("  %-8s  %+d\n", pname(p), scores[p]);
    putchar('\n');
}
