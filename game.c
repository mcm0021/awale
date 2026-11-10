#include <stdio.h>
#include <string.h>
#include "game.h"

void initGame(Game* g, User* p1, User* p2) {
    g->id = 0; // You can set a unique ID for the game
    g->p1 = *p1;
    g->p2 = *p2;
    for (int i = 0; i < 6; i++) {
        g->p1_board[i] = 4; // Initial seeds for player 1
        g->p2_board[i] = 4; // Initial seeds for player 2
    }
    g->p1_score = 0;
    g->p2_score = 0;
    memset(g->moves, 0, sizeof(g->moves));
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
        // Sow in player 2's pits (5 down to 0)
        for (int i = 5; i >= 0 && seeds > 0; i--) {
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
        while (last_pit < 6 && (g->p2_board[last_pit] == 2 || g->p2_board[last_pit] == 3)) {
            g->p1_score += g->p2_board[last_pit];
            g->p2_board[last_pit] = 0;
            last_pit++;
        }
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
        // Sow in player 1's pits (5 down to 0)
        for (int i = 5; i >= 0 && seeds > 0; i--) {
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
        while (last_pit < 6 && (g->p1_board[last_pit] == 2 || g->p1_board[last_pit] == 3)) {
            g->p2_score += g->p1_board[last_pit];
            g->p1_board[last_pit] = 0;
            last_pit++;
        }
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
