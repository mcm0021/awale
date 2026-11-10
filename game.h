#ifndef AWALE_GAME_H
#define AWALE_GAME_H

#include "user.h"

typedef struct Game {
    int id;
    User p1;
    User p2;
    int p1_board[6];
    int p2_board[6];
    int p1_score;
    int p2_score;
    int moves[100];
} Game;

void initGame(Game *game, User *p1, User *p2);
void p1Move(Game *game, int move);
void p2Move(Game *game, int move);
void displayBoard(Game *game);

#endif