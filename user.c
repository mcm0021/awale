#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
// somewhere we store sockfd to user* map? and a separate list (set) of users? just something to display all users also

void registerUser(User* user, int sockfd) {
    user->sockfd = sockfd;
    user->isprivate = false;
    memset(user->username, 0, sizeof(user->username));
    memset(user->bio, 0, sizeof(user->bio));
    memset(user->saved_games, 0, sizeof(user->saved_games));
    memset(user->friends, 0, sizeof(user->friends));
}

void setUsername(User* user, const char* username) {
    strncpy(user->username, username, sizeof(user->username) - 1);
    user->username[sizeof(user->username) - 1] = '\0';
}

void setBio(User* user, const char* bio) {
    strncpy(user->bio, bio, sizeof(user->bio) - 1);
    user->bio[sizeof(user->bio) - 1] = '\0';
}

void addFriend(User* user, User* friend) {
    if (!user->isprivate){
        return; // cannot add friends if the user is public
    }
    for (int i = 0; i < 10; i++) {
        if (user->friends[i] == NULL) {
            user->friends[i] = friend;
            break;
        }
    }
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
