#ifndef AWALE_GAME_H
#define AWALE_GAME_H

#include <stddef.h>
#include "user.h"

#define MAX_GAME_WATCHERS 100
#define MAX_MOVES 500

typedef struct Game {
    int id;
    User p1;
    User p2;
    int p1_board[6];
    int p2_board[6];
    int p1_score;
    int p2_score;
    int moves[MAX_MOVES];
    int move_count;
    int current_player;
    int is_active;
    int watchers[MAX_GAME_WATCHERS];
    int watcher_count;
} Game;

void initGame(Game *game, User *p1, User *p2);
void resetBoard(Game *game);
int isLegalMove(Game *game, int player, int move);
void p1Move(Game *game, int move);
void p2Move(Game *game, int move);
void displayBoard(Game *game);
void getBoardString(Game *game, char *buffer, size_t size);
int isGameOver(Game *game);

#endif
