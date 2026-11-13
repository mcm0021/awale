#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
// somewhere we store sockfd to user* map? and a separate list (set) of users? just something to display all users also

void registerUser(User* user, int sockfd) {
    user->sockfd = sockfd;
    user->isprivate = false;
    user->score = 0;
    memset(user->username, 0, sizeof(user->username));
    memset(user->bio, 0, sizeof(user->bio));
    user->bio_lines = 0;
    memset(user->saved_games, 0, sizeof(user->saved_games));
    memset(user->friends, 0, sizeof(user->friends));
    memset(user->pending_challenge_from, 0, sizeof(user->pending_challenge_from));
}

void setUsername(User* user, const char* username) {
    strncpy(user->username, username, sizeof(user->username) - 1);
    user->username[sizeof(user->username) - 1] = '\0';
}

/* Appends one line to the bio. Returns 0 if the bio already has 10 lines. */
int addBioLine(User* user, const char* line) {
    if (user->bio_lines >= BIO_LINES) {
        return 0;
    }
    strncpy(user->bio[user->bio_lines], line, BIO_LINE_LEN - 1);
    user->bio[user->bio_lines][BIO_LINE_LEN - 1] = '\0';
    user->bio_lines++;
    return 1;
}

/* Returns 0 if the friend list is full. */
int addFriend(User* user, const char* name) {
    for (int i = 0; i < 10; i++) {
        if (user->friends[i][0] == '\0') {
            strncpy(user->friends[i], name, MAX_USERNAME_LEN - 1);
            return 1;
        }
    }
    return 0;
}


void makePrivate(User* user) {
    user->isprivate = true;
}

void makePublic(User* user) {
    user->isprivate = false;
}

void saveGame(User* user, Game* g) {
    for (int i = 0; i < 10; i++) {
        if (user->saved_games[i] == NULL) {
            user->saved_games[i] = malloc(sizeof(*g));
            if (user->saved_games[i] == NULL) {
                return;
            }
            *user->saved_games[i] = *g;
            break;
        }
    }
}
