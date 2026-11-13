#include <stdio.h>
#include <string.h>
#include "game.h"

void initGame(Game* g, User* p1, User* p2) {
    static int next_id = 1;
    g->id = next_id++;
    g->p1 = *p1;
    g->p2 = *p2;
    resetBoard(g);
    g->is_active = 1;
    g->watcher_count = 0;
    g->move_count = 0;
    memset(g->moves, 0, sizeof(g->moves));
    memset(g->watchers, 0, sizeof(g->watchers));
}

/* Starting position: 4 seeds per pit, no score, Player 1 to move. */
void resetBoard(Game* g) {
    for (int i = 0; i < 6; i++) {
        g->p1_board[i] = 4; // Initial seeds for player 1
        g->p2_board[i] = 4; // Initial seeds for player 2
    }
    g->p1_score = 0;
    g->p2_score = 0;
    g->current_player = 1;
}

static int sideSeeds(int* side) {
    int total = 0;
    for (int i = 0; i < 6; i++) total += side[i];
    return total;
}

/* Captures backwards from last_pit while pits hold 2 or 3 seeds.
   If that would take all of the opponent's seeds, nothing is captured (grand slam rule). */
static void capture(int* side, int last_pit, int* score) {
    int captured = 0;
    int pit = last_pit;
    while (pit >= 0 && (side[pit] == 2 || side[pit] == 3)) {
        captured += side[pit];
        pit--;
    }
    if (captured == 0 || captured == sideSeeds(side)) return;
    for (int i = last_pit; i > pit; i--) side[i] = 0;
    *score += captured;
}

/* A move is legal if the pit is valid and not empty, and, when the opponent
   has no seeds, the move gives them at least one seed. */
int isLegalMove(Game* g, int player, int move) {
    int* own = (player == 1) ? g->p1_board : g->p2_board;
    if (move < 0 || move >= 6 || own[move] == 0) return 0;
    int* opp = (player == 1) ? g->p2_board : g->p1_board;
    if (sideSeeds(opp) > 0) return 1;

    Game copy = *g;
    if (player == 1) p1Move(&copy, move);
    else p2Move(&copy, move);
    return sideSeeds(player == 1 ? copy.p2_board : copy.p1_board) > 0;
}

void p1Move(Game* g, int move) {
    if (g == NULL) {
        return;
    }
    if (move < 0 || move >= 6) {
        printf("Invalid move. Please choose a valid pit (0-5).\n");
        return;
    }
    if (g->p1_board[move] == 0) {
        printf("Selected pit is empty. Choose a different pit.\n");
        return;
    }

    int seeds = g->p1_board[move];
    g->p1_board[move] = 0;

    int last_side = -1; // 1 for p1_board, 2 for p2_board
    int last_pit = -1;

    while (seeds > 0) {
        // Sow in player 1's pits after the starting pit (move + 1 to 5)
        for (int i = move + 1; i < 6 && seeds > 0; i++) {
            g->p1_board[i]++;
            seeds--;
            last_side = 1;
            last_pit = i;
        }
        // Sow in player 2's pits (0 to 5)
        for (int i = 0; i < 6 && seeds > 0; i++) {
            g->p2_board[i]++;
            seeds--;
            last_side = 2;
            last_pit = i;
        }
        // Sow in player 1's pits before the starting pit (0 to move - 1)
        for (int i = 0; i < move && seeds > 0; i++) {
            g->p1_board[i]++;
            seeds--;
            last_side = 1;
            last_pit = i;
        }
    }

    // Capture logic (Awale / Oware):
    // Capture only occurs if the last seed was sown in opponent's territory (p2_board)
    if (last_side == 2) {
        capture(g->p2_board, last_pit, &g->p1_score);
    }
}

void p2Move(Game* g, int move) {
    if (g == NULL) {
        return;
    }
    if (move < 0 || move >= 6) {
        printf("Invalid move. Please choose a valid pit (0-5).\n");
        return;
    }
    if (g->p2_board[move] == 0) {
        printf("Selected pit is empty. Choose a different pit.\n");
        return;
    }

    int seeds = g->p2_board[move];
    g->p2_board[move] = 0;

    int last_side = -1; // 1 for p1_board, 2 for p2_board
    int last_pit = -1;

    while (seeds > 0) {
        // Sow in player 2's pits after the starting pit (move + 1 to 5)
        for (int i = move + 1; i < 6 && seeds > 0; i++) {
            g->p2_board[i]++;
            seeds--;
            last_side = 2;
            last_pit = i;
        }
        // Sow in player 1's pits (0 to 5)
        for (int i = 0; i < 6 && seeds > 0; i++) {
            g->p1_board[i]++;
            seeds--;
            last_side = 1;
            last_pit = i;
        }
        // Sow in player 2's pits before the starting pit (0 to move - 1)
        for (int i = 0; i < move && seeds > 0; i++) {
            g->p2_board[i]++;
            seeds--;
            last_side = 2;
            last_pit = i;
        }
    }

    // Capture logic (Awale / Oware):
    // Capture only occurs if the last seed was sown in opponent's territory (p1_board)
    if (last_side == 1) {
        capture(g->p1_board, last_pit, &g->p2_score);
    }
}

void displayBoard(Game* g) {
    printf("Player 1 Board: ");
    for (int i = 0; i < 6; i++) {
        printf("%d ", g->p1_board[i]);
    }
    printf("\nPlayer 2 Board: ");
    for (int i = 0; i < 6; i++) {
        printf("%d ", g->p2_board[i]);
    }
    printf("\nScores - Player 1: %d, Player 2: %d\n", g->p1_score, g->p2_score);
}

void getBoardString(Game* g, char* buffer, size_t size) {
    if (g == NULL || buffer == NULL || size == 0) return;

    snprintf(buffer, size,
        "\n=============================================================\n"
        "Game #%d | Turn: Player %d (%s)\n"
        "Player 2: %s [Score: %d]\n"
        "  Pits:    (5)  (4)  (3)  (2)  (1)  (0)\n"
        "  Seeds:  [%2d] [%2d] [%2d] [%2d] [%2d] [%2d]\n"
        "-------------------------------------------------------------\n"
        "  Seeds:  [%2d] [%2d] [%2d] [%2d] [%2d] [%2d]\n"
        "  Pits:    (0)  (1)  (2)  (3)  (4)  (5)\n"
        "Player 1: %s [Score: %d]\n"
        "=============================================================\n",
        g->id,
        g->current_player,
        (g->current_player == 1 ? g->p1.username : g->p2.username),
        g->p2.username, g->p2_score,
        g->p2_board[5], g->p2_board[4], g->p2_board[3],
        g->p2_board[2], g->p2_board[1], g->p2_board[0],
        g->p1_board[0], g->p1_board[1], g->p1_board[2],
        g->p1_board[3], g->p1_board[4], g->p1_board[5],
        g->p1.username, g->p1_score
    );
}

int isGameOver(Game* g) {
    if (g == NULL || !g->is_active) return 0;
    if (g->p1_score >= 25) return 1;
    if (g->p2_score >= 25) return 2;
    if (g->p1_score == 24 && g->p2_score == 24) return 3;

    /* The player to move has no legal move (they cannot feed the opponent):
       the game ends and each player keeps the seeds on their own side. */
    int can_move = 0;
    for (int i = 0; i < 6; i++) {
        if (isLegalMove(g, g->current_player, i)) can_move = 1;
    }
    if (!can_move) {
        g->p1_score += sideSeeds(g->p1_board);
        g->p2_score += sideSeeds(g->p2_board);
        memset(g->p1_board, 0, sizeof(g->p1_board));
        memset(g->p2_board, 0, sizeof(g->p2_board));
    }

    int remaining = 0;
    for (int i = 0; i < 6; i++) {
        remaining += g->p1_board[i] + g->p2_board[i];
    }
    if (remaining < 6) {
        if (g->p1_score > g->p2_score) return 1;
        if (g->p2_score > g->p1_score) return 2;
        return 3;
    }
    return 0;
}
