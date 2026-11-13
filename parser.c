#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "server2.h"
#include "user.h"
#include <stdint.h>

#define MAX_ARGS 100
#define REGISTER_COMMAND "register"
#define ADD_BIO_COMMAND "add_bio"
#define ADD_FRIEND_COMMAND "add_friend"
#define CHANGE_MODE_COMMAND "change_mode"
#define BIO_COMMAND "bio"
#define LEADERBOARD_COMMAND "leaderboard"
#define CHAT_COMMAND "chat:"
#define GET_ONLINE_USERS_COMMAND "get_online_users"
#define OBSERVE_COMMAND "observe"

#define CHALLENGE_COMMAND "challenge"
#define ACCEPT_CHALLENGE_COMMAND "accept_challenge"

#define NEXT_MOVE_COMMAND "next_move"
#define SAVE_GAME_COMMAND "save_game"
#define QUIT_COMMAND "quit"

typedef const char* (*CommandHandler)(ServerContext* ctx, int argc, char** args, size_t user_index, size_t* sent_to);

typedef struct {
    const char* name;
    CommandHandler handler;
    const char* usage;
} CommandMapping;

int parse_arguments(char* line, char** args, int max_args);
const char* dispatch_command(ServerContext* ctx, char* line, size_t user_index, size_t* sent_to);

const char* handle_add_bio(ServerContext* ctx, int argc, char** args, size_t user_index, size_t* sent_to) {
    if (argc < 2) {
        return "Error: Bio is required. Usage: add_bio <bio>\n";
    }

    /* each add_bio adds one line (all the words after the command) */
    char line[BIO_LINE_LEN];
    line[0] = '\0';
    for (int i = 1; i < argc; i++) {
        if (i > 1) {
            strncat(line, " ", sizeof(line) - strlen(line) - 1);
        }
        strncat(line, args[i], sizeof(line) - strlen(line) - 1);
    }

    *sent_to = user_index;
    if (!addBioLine(&ctx->online_users[user_index], line)) {
        return "Error: Your bio already has 10 lines.\n";
    }
    return "Bio line added.\n";
}

const char* handle_bio(ServerContext* ctx, int argc, char** args, size_t user_index, size_t* sent_to) {
    *sent_to = user_index;
    if (argc < 2) {
        return "Error: Usage: bio <username>\n";
    }

    for (int i = 0; i < ctx->user_count; i++) {
        User* user = &ctx->online_users[i];
        if (strcmp(user->username, args[1]) == 0) {
            static char response[BUF_SIZE];
            snprintf(response, sizeof(response), "Bio of %s:\n", user->username);
            if (user->bio_lines == 0) {
                strncat(response, "(empty)\n", sizeof(response) - strlen(response) - 1);
            }
            for (int l = 0; l < user->bio_lines; l++) {
                strncat(response, user->bio[l], sizeof(response) - strlen(response) - 1);
                strncat(response, "\n", sizeof(response) - strlen(response) - 1);
            }
            return response;
        }
    }
    return "Error: User not found.\n";
}

const char* get_online_users(ServerContext* ctx, int argc, char** args, size_t user_index, size_t* sent_to) {
    (void)argc;
    (void)args;
    static char response[BUF_SIZE];
    response[0] = '\0';

    for (int i = 0; i < ctx->user_count; i++) {
        size_t used = strlen(response);
        if (used >= sizeof(response) - 1) {
            break;
        }

        snprintf(response + used, sizeof(response) - used, "%s\n",
                 ctx->online_users[i].username);
    }

    *sent_to = user_index; 
    return response;
}

const char* handle_add_friend(ServerContext* ctx, int argc, char** args, size_t user_index, size_t* sent_to) {
    if (argc < 2) {
        return "Error: Friend username is required. Usage: add_friend <username>\n";
    }

    User* user = &ctx->online_users[user_index];
    const char* friend_username = args[1];
    *sent_to = user_index;

    for (int i = 0; i < ctx->user_count; i++) {
        if (strcmp(ctx->online_users[i].username, friend_username) == 0) {
            if (addFriend(user, friend_username)) {
                return "Friend added successfully.\n";
            }
            return "Error: Friend list is full.\n";
        }
    }

    return "Error: User not found.\n";
}

const char* handle_change_mode(ServerContext* ctx, int argc, char** args, size_t user_index, size_t* sent_to) {
    if (argc < 2) {
        return "Error: Mode is required. Usage: change_mode <public|private>\n";
    }

    User* user = &ctx->online_users[user_index];
    const char* mode = args[1];

    if (strcmp(mode, "public") == 0) {
        user->isprivate = false;
        *sent_to = user_index; 
        return "Mode changed to public.\n";
    } else if (strcmp(mode, "private") == 0) {
        user->isprivate = true;
        *sent_to = user_index; 
        return "Mode changed to private.\n";
    } else {
        return "Error: Invalid mode. Use 'public' or 'private'.\n";
    }
}

const char* handle_send_message(ServerContext* ctx, int argc, char** args, size_t user_index, size_t* sent_to) {
    (void)ctx;
    if (argc < 2) {
        *sent_to = user_index;
        return "Usage: chat: <message>\n";
    }

    static char message[BUF_SIZE];
    message[0] = '\0';
    for (int i = 1; i < argc; i++) {
        if (i > 1) {
            strncat(message, " ", sizeof(message) - strlen(message) - 1);
        }
        strncat(message, args[i], sizeof(message) - strlen(message) - 1);
    }
    *sent_to = SIZE_MAX; 
    return message;
}

/* Lists online users from the highest score to the lowest. */
const char* handle_leaderboard(ServerContext* ctx, int argc, char** args, size_t user_index, size_t* sent_to) {
    (void)argc;
    (void)args;
    static char response[BUF_SIZE];
    int order[MAX_USERS];
    *sent_to = user_index;

    for (int i = 0; i < ctx->user_count; i++) {
        order[i] = i;
    }
    /* simple selection sort on the score */
    for (int i = 0; i < ctx->user_count; i++) {
        for (int j = i + 1; j < ctx->user_count; j++) {
            if (ctx->online_users[order[j]].score > ctx->online_users[order[i]].score) {
                int tmp = order[i];
                order[i] = order[j];
                order[j] = tmp;
            }
        }
    }

    snprintf(response, sizeof(response), "Leaderboard:\n");
    for (int i = 0; i < ctx->user_count; i++) {
        size_t used = strlen(response);
        snprintf(response + used, sizeof(response) - used, "%d. %s %+d\n",
                 i + 1, ctx->online_users[order[i]].username, ctx->online_users[order[i]].score);
    }
    return response;
}

static const CommandMapping COMMAND_TABLE[] = {
    { ADD_BIO_COMMAND,  handle_add_bio,  "add_bio <bio line>" },
    { BIO_COMMAND, handle_bio, "bio <username>" },
    { GET_ONLINE_USERS_COMMAND, get_online_users, "get_online_users" },
    { ADD_FRIEND_COMMAND, handle_add_friend, "add_friend <username>" },
    { CHANGE_MODE_COMMAND, handle_change_mode, "change_mode <public|private>" },
    { CHAT_COMMAND, handle_send_message, "chat: <message>" },
    { LEADERBOARD_COMMAND, handle_leaderboard, "leaderboard" }
};

#define NUM_COMMANDS (sizeof(COMMAND_TABLE) / sizeof(COMMAND_TABLE[0]))

int handle_command(ServerContext* ctx, const char* command_line, char* response_buffer, size_t buffer_size, size_t user_index, size_t* sent_to) {

    if (command_line == NULL || response_buffer == NULL || buffer_size == 0) {
        return -1;
    }

    char cmd_copy[BUF_SIZE];
    strncpy(cmd_copy, command_line, sizeof(cmd_copy) - 1);
    cmd_copy[sizeof(cmd_copy) - 1] = '\0';

    const char* result = dispatch_command(ctx, cmd_copy, user_index, sent_to);

    if (result != NULL) {
        snprintf(response_buffer, buffer_size, "%s", result);
        return 0;
    } else {
        snprintf(response_buffer, buffer_size, "Error: Command execution failed.\n");
        return -1; 
    }
}

/**
 * Safely parse a line into arguments using a safe copying approach 
 * or reentrant tokenization, avoiding direct strtok modification of read-only segments.
 */
int parse_arguments(char* line, char** args, int max_args) {
    int argc = 0;
    char* ptr = line;

    while (*ptr != '\0' && argc < max_args) {
        // Skip leading whitespace
        while (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r') {
            ptr++;
        }
        if (*ptr == '\0') break;

        args[argc++] = ptr;

        // Find end of token
        while (*ptr != '\0' && *ptr != ' ' && *ptr != '\t' && *ptr != '\n' && *ptr != '\r') {
            ptr++;
        }

        if (*ptr != '\0') {
            *ptr = '\0';
            ptr++;
        }
    }
    return argc;
}

/**
 * Matches and executes the command using the dispatcher table.
 */
const char* dispatch_command(ServerContext* ctx, char* line, size_t user_index, size_t* sent_to) {
    char* args[MAX_ARGS];
    int argc = parse_arguments(line, args, MAX_ARGS);

    if (argc == 0) {
        return NULL;
    }

    for (size_t i = 0; i < NUM_COMMANDS; i++) {
        if (strcmp(args[0], COMMAND_TABLE[i].name) == 0) {
            return COMMAND_TABLE[i].handler(ctx, argc, args, user_index, sent_to);
        }
    }

    static char error_buf[128];
    snprintf(error_buf, sizeof(error_buf), "Unknown command: %s\n", args[0]);
    return error_buf;
}
