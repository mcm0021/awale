#include <stdio.h>
#include <stdlib.h>

#define HOLES 12
#define INITIAL_SEEDS 4

int board[HOLES];
int scorePlayer1 = 0; 
int scorePlayer2 = 0;
int currentPlayer = 1;

void initBoard() {
    for (int i = 0; i < HOLES; i++) {
        board[i] = INITIAL_SEEDS;
    }
}

void printBoard() {
    printf("\n=========================================\n");
    printf("Player 2 (Score: %d)\n", scorePlayer2);
    printf("  11   10    9    8    7    6  (Indices)\n");
    printf("-----------------------------------------\n  ");
    for (int i = 11; i >= 6; i--) {
        printf("[%2d] ", board[i]);
    }
    printf("\n  ");
    for (int i = 0; i <= 5; i++) {
        printf("[%2d] ", board[i]);
    }
    printf("\n   0    1    2    3    4    5  (Indices)\n");
    printf("Player 1 (Score: %d)\n", scorePlayer1);
    printf("=========================================\n");
}

int playMove(int player, int startHole) {
    int startIdx, endIdx;
    if (player == 1) {
        if (startHole < 0 || startHole > 5) return 0;
        startIdx = startHole;
    } else {
        if (startHole < 6 || startHole > 11) return 0;
        startIdx = startHole;
    }

    int seeds = board[startIdx];
    if (seeds == 0) return 0; 

    board[startIdx] = 0;
    int current = startIdx;

    while (seeds > 0) {
        current = (current + 1) % HOLES;
        if (current == startIdx) {
            current = (current + 1) % HOLES;
        }
        board[current]++;
        seeds--;
    }

    endIdx = current;

    int captured = 0;
    while (1) {
        if (player == 1 && endIdx >= 6 && endIdx <= 11) {
            if (board[endIdx] == 2 || board[endIdx] == 3) {
                captured += board[endIdx];
                board[endIdx] = 0;
                endIdx--;
            } else {
                break;
            }
        } else if (player == 2 && endIdx >= 0 && endIdx <= 5) {
            if (board[endIdx] == 2 || board[endIdx] == 3) {
                captured += board[endIdx];
                board[endIdx] = 0;
                endIdx--;
            } else {
                break;
            }
        } else {
            break;
        }
    }

    if (player == 1) scorePlayer1 += captured;
    else scorePlayer2 += captured;

    return 1;
}

int isGameOver() {
    if (scorePlayer1 >= 25 || scorePlayer2 >= 25) return 1;
    return 0;
}

int main() {
    initBoard();

    while (!isGameOver()) {
        printBoard();
        printf("Turn player %d.\n", currentPlayer);
        
        int choice;
        if (currentPlayer == 1) {
            printf("Pick a spot (0-5): ");
        } else {
            printf("Pick a spot (6-11): ");
        }

        if (scanf("%d", &choice) != 1) {
            break;
        }

        if (playMove(currentPlayer, choice)) {
            currentPlayer = (currentPlayer == 1) ? 2 : 1;
        } else {
            printf("Invalid move. Try again!\n");
        }
    }

    printBoard();
    printf("Game over!\n");
    if (scorePlayer1 > scorePlayer2) printf("Player 1 wins!\n");
    else if (scorePlayer2 > scorePlayer1) printf("Player 2 wins!\n");
    else printf("Tie!\n");

    return 0;
}
