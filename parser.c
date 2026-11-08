#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "server2.h"
#include "user.h"

#define MAX_ARGS 100
#define REGISTER_COMMAND "register"
#define ADD_BIO_COMMAND "add_bio"
#define ADD_FRIEND_COMMAND "add_friend"
#define CHANGE_MODE_COMMAND "change_mode"
#define SEND_MESSAGE_COMMAND "send_message"
#define GET_ONLINE_USERS_COMMAND "get_online_users"
#define OBSERVE_COMMAND "observe"

#define CHALLENGE_COMMAND "challenge"
#define ACCEPT_CHALLENGE_COMMAND "accept_challenge"

#define NEXT_MOVE_COMMAND "next_move"
#define SAVE_GAME_COMMAND "save_game"
#define QUIT_COMMAND "quit"

static const char* ALL_COMMANDS[] = {
    REGISTER_COMMAND, ADD_BIO_COMMAND, ADD_FRIEND_COMMAND,
    CHANGE_MODE_COMMAND, SEND_MESSAGE_COMMAND, GET_ONLINE_USERS_COMMAND,
    OBSERVE_COMMAND, CHALLENGE_COMMAND, ACCEPT_CHALLENGE_COMMAND,
    NEXT_MOVE_COMMAND, SAVE_GAME_COMMAND, QUIT_COMMAND
};

typedef const char* (*CommandHandler)(ServerContext* ctx, int argc, char** args);

typedef struct {
    const char* name;
    CommandHandler handler;
    const char* usage;
} CommandMapping;

int parse_arguments(char* line, char** args, int max_args);
const char* dispatch_command(ServerContext* ctx, char* line);

const char* handle_register(ServerContext* ctx, int argc, char** args) {
    if (argc < 2) {
        return "Error: Username is required. Usage: register <username>\n";
    }

    const char* requested_username = args[1];

    if (strlen(requested_username) >= MAX_USERNAME_LEN) {
        return "Error: Username is too long.\n";
    }

    if (ctx->user_count >= MAX_USERS) {
        return "Error: Server user capacity reached.\n";
    }

    for (int i = 0; i < ctx->user_count; i++) {
        if (strcmp(ctx->online_users[i].username, requested_username) == 0) {
            return "Error: Username already exists.\n";
        }
    }

    User* new_user = &ctx->online_users[ctx->user_count];
    memset(new_user, 0, sizeof(User));
    strncpy(new_user->username, requested_username, MAX_USERNAME_LEN - 1);
    new_user->username[MAX_USERNAME_LEN - 1] = '\0'; // Ensure safety
    new_user->bio[0] = '\0'; // Empty initial bio

    ctx->user_count++;

    return "User registered successfully.\n";
}

static const CommandMapping COMMAND_TABLE[] = {
    { REGISTER_COMMAND,  handle_register,  "register <username>" },
};

#define NUM_COMMANDS (sizeof(COMMAND_TABLE) / sizeof(COMMAND_TABLE[0]))

int handle_command(ServerContext* ctx, const char* command_line, char* response_buffer, size_t buffer_size, char* send_to_usernames, size_t send_to_usernames_size) {
    (void)send_to_usernames;
    (void)send_to_usernames_size;
    (void)ALL_COMMANDS;

    if (command_line == NULL || response_buffer == NULL || buffer_size == 0) {
        return -1;
    }

    char cmd_copy[BUF_SIZE];
    strncpy(cmd_copy, command_line, sizeof(cmd_copy) - 1);
    cmd_copy[sizeof(cmd_copy) - 1] = '\0';

    const char* result = dispatch_command(ctx, cmd_copy);

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
const char* dispatch_command(ServerContext* ctx, char* line) {
    char* args[MAX_ARGS];
    int argc = parse_arguments(line, args, MAX_ARGS);

    if (argc == 0) {
        return NULL;
    }

    for (size_t i = 0; i < NUM_COMMANDS; i++) {
        if (strcmp(args[0], COMMAND_TABLE[i].name) == 0) {
            return COMMAND_TABLE[i].handler(ctx, argc, args);
        }
    }

    static char error_buf[128];
    snprintf(error_buf, sizeof(error_buf), "Unknown command: %s\n", args[0]);
    return error_buf;
}
