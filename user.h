#ifndef AWALE_USER_H
#define AWALE_USER_H

#include <stdbool.h>

typedef struct Game Game;

typedef struct User {
    int sockfd;
    char username[10];
    char bio[10];
    Game *saved_games[10];
    bool isprivate;
    struct User *friends[10];
} User;

void registerUser(User *user, int sockfd);
void setUsername(User *user, const char *username);
void setBio(User *user, const char *bio);
void addFriend(User *user, User *friend);
void makePrivate(User *user);
void makePublic(User *user);
void saveGame(User *user, Game *game);

#endif