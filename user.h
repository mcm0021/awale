#ifndef AWALE_USER_H
#define AWALE_USER_H

#include <stdbool.h>

#define MAX_USERS 100
#define MAX_USERNAME_LEN 50
#define BIO_LINES 10
#define BIO_LINE_LEN 81

typedef struct Game Game;

typedef struct User {
    int sockfd;
    char username[MAX_USERNAME_LEN];
    char bio[BIO_LINES][BIO_LINE_LEN]; /* up to 10 ASCII lines */
    int bio_lines;
    Game *saved_games[10];
    bool isprivate;
    char friends[10][MAX_USERNAME_LEN]; /* stored by name: the user array is compacted on disconnect */
    char pending_challenge_from[MAX_USERNAME_LEN];
    int score; /* leaderboard: +1 per win, -1 per loss */
} User;

void registerUser(User *user, int sockfd);
void setUsername(User *user, const char *username);
int addBioLine(User *user, const char *line);
int addFriend(User *user, const char *name);
void makePrivate(User *user);
void makePublic(User *user);
void saveGame(User *user, Game *game);

#endif
