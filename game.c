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
    // Implement the logic for player 1's move
    // Update the board and scores accordingly
}

void p2Move(Game* g, int move) {
    // Implement the logic for player 2's move
    // Update the board and scores accordingly
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
