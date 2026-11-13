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
#define CHAT_COMMAND "chat:"
#define GET_ONLINE_USERS_COMMAND "get_online_users"
#define ONLINE_GAMES_COMMAND "online_games"
#define WATCH_COMMAND "watch"

#define CHALLENGE_COMMAND "challenge"
#define ACCEPT_CHALLENGE_COMMAND "accept"

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


static int is_friend_of(User *owner, User *viewer)
{
   if(owner->sockfd == viewer->sockfd)
      return 1;

   for(int i = 0; i < 10; i++)
   {
      if(owner->friends[i] != NULL && owner->friends[i]->sockfd == viewer->sockfd)
         return 1;
   }
   return 0;
}

static int can_view_game(User *online_users, int actual, Game *game, User *viewer)
{
   User *players[2] = { NULL, NULL };
   for(int i = 0; i < actual; i++)
   {
      if(online_users[i].sockfd == game->p1.sockfd)
         players[0] = &online_users[i];
      if(online_users[i].sockfd == game->p2.sockfd)
         players[1] = &online_users[i];
   }

   for(int i = 0; i < 2; i++)
   {
      if(players[i] != NULL && players[i]->isprivate && !is_friend_of(players[i], viewer))
         return 0;
   }
   return 1;
}

static void send_board_to_watchers(Game *game, const char *message)
{
   for(int i = 0; i < game->watcher_count; i++)
      write_client(game->watchers[i], message);
}

const char* handle_add_bio(ServerContext* ctx, int argc, char** args, size_t user_index, size_t* sent_to) {
    *sent_to = user_index;
    if (argc < 2) {
        return "Error: Bio is required. Usage: add_bio <bio>\n";
    }

    User* user = &ctx->online_users[user_index];
    user->bio[0] = '\0'; // Clear existing bio
    static char new_bio[BUF_SIZE];
    new_bio[0] = '\0';
    for (int i = 1; i < argc; i++) {
        if (i > 1) {
            strncat(new_bio, " ", sizeof(new_bio) - strlen(new_bio) - 1);
        }
        strncat(new_bio, args[i], sizeof(new_bio) - strlen(new_bio) - 1);
    }
    strncat(user->bio, new_bio, sizeof(user->bio) - 1);
    return "Bio updated successfully.\n";
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

        snprintf(response + used, sizeof(response) - used, "%s: %s\n",
                 ctx->online_users[i].username, ctx->online_users[i].bio);
    }

    *sent_to = user_index; 
    return response;
}

const char* handle_add_friend(ServerContext* ctx, int argc, char** args, size_t user_index, size_t* sent_to) {
    *sent_to = user_index;
    if (argc < 2) {
        return "Error: Friend username is required. Usage: add_friend <username>\n";
    }

    User* user = &ctx->online_users[user_index];
    const char* friend_username = args[1];

    for (int i = 0; i < ctx->user_count; i++) {
        if (strcmp(ctx->online_users[i].username, friend_username) == 0) {
            for (int j = 0; j < 10; j++) {
                if (user->friends[j] == NULL) {
                    user->friends[j] = &ctx->online_users[i];
                    *sent_to = user_index; 
                    return "Friend added successfully.\n";
                }
            }
            return "Error: Friend list is full.\n";
        }
    }

    return "Error: User not found.\n";
}

const char* handle_change_mode(ServerContext* ctx, int argc, char** args, size_t user_index, size_t* sent_to) {
    *sent_to = user_index;
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

static const char *handle_online_games_cmd(ServerContext *ctx, int argc, char **args,
                                           size_t user_index, size_t *sent_to)
{
   (void)argc;
   (void)args;
   User *online_users = ctx->online_users;
   int actual = ctx->user_count;
   int sender_idx = (int)user_index;
   Game *active_games = ctx->active_games;
   int max_games = ctx->game_capacity;
   User *viewer = &online_users[sender_idx];
   static char response[BUF_SIZE];
   response[0] = '\0';
   int found = 0;
   *sent_to = user_index;

   for(int g = 0; g < max_games; g++)
   {
      if(active_games[g].is_active && can_view_game(online_users, actual, &active_games[g], viewer))
      {
         size_t used = strlen(response);
         snprintf(response + used, sizeof(response) - used,
                  "Game %d: %s vs %s\n", active_games[g].id,
                  active_games[g].p1.username, active_games[g].p2.username);
         found = 1;
      }
   }

   if(!found)
      snprintf(response, sizeof(response), "No viewable games are currently in progress.\n");
   return response;
}

static const char *handle_watch_game_cmd(ServerContext *ctx, int argc, char **args,
                                         size_t user_index, size_t *sent_to)
{
   User *online_users = ctx->online_users;
   int actual = ctx->user_count;
   int sender_idx = (int)user_index;
   Game *active_games = ctx->active_games;
   int max_games = ctx->game_capacity;
   User *viewer = &online_users[sender_idx];
   *sent_to = user_index;
   if(argc < 3 || strcmp(args[1], "game") != 0)
      return "Usage: watch game <id>\n";

   char *p = args[2];
   char *end = NULL;
   long requested_id = strtol(p, &end, 10);
   if(end == p || *end != '\0' || requested_id < 0)
      return "Usage: watch game <id>\n";

   for(int g = 0; g < max_games; g++)
   {
      Game *game = &active_games[g];
      if(!game->is_active || game->id != requested_id)
         continue;
      if(!can_view_game(online_users, actual, game, viewer))
         return "You cannot watch this private game.\n";
      if(game->p1.sockfd == viewer->sockfd || game->p2.sockfd == viewer->sockfd)
         return "You are already playing this game.\n";
      for(int w = 0; w < game->watcher_count; w++)
      {
         if(game->watchers[w] == viewer->sockfd)
            return "You are already watching this game.\n";
      }
      if(game->watcher_count == MAX_GAME_WATCHERS)
         return "This game has reached its watcher limit.\n";

      game->watchers[game->watcher_count++] = viewer->sockfd;
      static char response[BUF_SIZE];
      char board_msg[BUF_SIZE];
      snprintf(response, sizeof(response), "Now watching game %d.\n", game->id);
      getBoardString(game, board_msg, sizeof(board_msg));
      strncat(response, board_msg, sizeof(response) - strlen(response) - 1);
      return response;
   }
   return "Game not found or no longer active.\n";
}


static const char *handle_challenge_cmd(ServerContext *ctx, int argc, char **args,
                                        size_t user_index, size_t *sent_to) {
   User *online_users = ctx->online_users;
   int actual = ctx->user_count;
   int sender_idx = (int)user_index;
   Game *active_games = ctx->active_games;
   int max_games = ctx->game_capacity;
   User *sender = &online_users[sender_idx];
   char target[MAX_USERNAME_LEN];
   *sent_to = sender->sockfd;

   if(argc < 2)
   {
      write_client(sender->sockfd, "Usage: challenge <username>\n");
      return "";
   }

   const char *target_arg = args[1];
   if(target_arg[0] == '-') target_arg++;
   strncpy(target, target_arg, sizeof(target) - 1);
   target[sizeof(target) - 1] = 0;

   if(strlen(target) == 0)
   {
      write_client(sender->sockfd, "Usage: challenge <username>\n");
      return "";
   }
   if(strcmp(sender->username, target) == 0)
   {
      write_client(sender->sockfd, "You cannot challenge yourself!\n");
      return "";
   }

   /* Check if sender is already in a game */
   for(int g = 0; g < max_games; g++)
   {
      if(active_games[g].is_active && (active_games[g].p1.sockfd == sender->sockfd || active_games[g].p2.sockfd == sender->sockfd))
      {
         write_client(sender->sockfd, "You are already playing a game!\n");
         return "";
      }
   }

   int target_idx = -1;
   for(int j = 0; j < actual; j++)
   {
      if(strcmp(online_users[j].username, target) == 0)
      {
         target_idx = j;
         break;
      }
   }

   if(target_idx == -1)
   {
      char err[BUF_SIZE];
      snprintf(err, sizeof(err), "User '%s' is not online.\n", target);
      write_client(sender->sockfd, err);
      return "";
   }

   User *opponent = &online_users[target_idx];

   /* Check if opponent is already in a game */
   for(int g = 0; g < max_games; g++)
   {
      if(active_games[g].is_active && (active_games[g].p1.sockfd == opponent->sockfd || active_games[g].p2.sockfd == opponent->sockfd))
      {
         char err[BUF_SIZE];
         snprintf(err, sizeof(err), "User '%s' is currently busy in another game.\n", target);
         write_client(sender->sockfd, err);
         return "";
      }
   }

   /* If opponent already challenged sender, start the game immediately */
   if(strcmp(sender->pending_challenge_from, target) == 0)
   {
      start_game(active_games, max_games, opponent, sender);
      sender->pending_challenge_from[0] = 0;
      opponent->pending_challenge_from[0] = 0;
      return "";
   }

   /* Record pending challenge */
   strncpy(opponent->pending_challenge_from, sender->username, MAX_USERNAME_LEN - 1);
   opponent->pending_challenge_from[MAX_USERNAME_LEN - 1] = 0;

   char notify[BUF_SIZE];
   snprintf(notify, sizeof(notify), "*** %s has challenged you to an Awale game! Type 'accept %s' (or 'challenge %s') to play. ***\n", sender->username, sender->username, sender->username);
   write_client(opponent->sockfd, notify);

   char ack[BUF_SIZE];
   snprintf(ack, sizeof(ack), "*** Challenge sent to %s. Waiting for response... ***\n", target);
   write_client(sender->sockfd, ack);
   return "";
}

static const char *handle_accept_cmd(ServerContext *ctx, int argc, char **args,
                                     size_t user_index, size_t *sent_to)
{
   User *online_users = ctx->online_users;
   int actual = ctx->user_count;
   int sender_idx = (int)user_index;
   Game *active_games = ctx->active_games;
   int max_games = ctx->game_capacity;
   User *sender = &online_users[sender_idx];
   char target[MAX_USERNAME_LEN];
   *sent_to = user_index;

   if(argc < 2)
   {
      write_client(sender->sockfd, "Usage: accept <username>\n");
      return "";
   }

   const char *target_arg = args[1];
   if(target_arg[0] == '-') target_arg++;
   strncpy(target, target_arg, sizeof(target) - 1);
   target[sizeof(target) - 1] = 0;

   int challenger_idx = -1;
   for(int j = 0; j < actual; j++)
   {
      if(strcmp(online_users[j].username, target) == 0)
      {
         challenger_idx = j;
         break;
      }
   }

   if(challenger_idx == -1 || strcmp(sender->pending_challenge_from, target) != 0)
   {
      write_client(sender->sockfd, "No pending challenge found from that user.\n");
      return "";
   }

   User *challenger = &online_users[challenger_idx];
   start_game(active_games, max_games, challenger, sender);
   sender->pending_challenge_from[0] = 0;
   challenger->pending_challenge_from[0] = 0;
   return "";
}

static const char *handle_move_cmd(ServerContext *ctx, int argc, char **args,
                                   size_t user_index, size_t *sent_to)
{
   User *online_users = ctx->online_users;
   int sender_idx = (int)user_index;
   Game *active_games = ctx->active_games;
   int max_games = ctx->game_capacity;
   User *sender = &online_users[sender_idx];
   *sent_to = user_index;

   if(argc < 2)
   {
      write_client(sender->sockfd, "Usage: next_move <pit_number>\n");
      return "";
   }

   char *end = NULL;
   long pit_value = strtol(args[1], &end, 10);
   if(*args[1] == '\0' || *end != '\0' || pit_value < 0 || pit_value > 5)
   {
      write_client(sender->sockfd, "Invalid pit. Please choose 0 to 5 (e.g. 'next_move 2').\n");
      return "";
   }
   int pit = (int)pit_value;

   int game_idx = -1;
   int player_num = 0;
   for(int g = 0; g < max_games; g++)
   {
      if(active_games[g].is_active)
      {
         if(active_games[g].p1.sockfd == sender->sockfd)
         {
            game_idx = g;
            player_num = 1;
            break;
         }
         else if(active_games[g].p2.sockfd == sender->sockfd)
         {
            game_idx = g;
            player_num = 2;
            break;
         }
      }
   }

   if(game_idx == -1)
   {
      write_client(sender->sockfd, "You are not currently in an active game.\n");
      return "";
   }

   Game *g = &active_games[game_idx];
   if(g->current_player != player_num)
   {
      write_client(sender->sockfd, "It is not your turn! Please wait for your opponent.\n");
      return "";
   }

   int seeds = (player_num == 1) ? g->p1_board[pit] : g->p2_board[pit];
   if(seeds == 0)
   {
      write_client(sender->sockfd, "Selected pit is empty. Choose another pit (0-5).\n");
      return "";
   }

   if(player_num == 1)
   {
      p1Move(g, pit);
      g->current_player = 2;
   }
   else
   {
      p2Move(g, pit);
      g->current_player = 1;
   }

   char board_msg[BUF_SIZE];
   getBoardString(g, board_msg, sizeof(board_msg));
   write_client(g->p1.sockfd, board_msg);
   write_client(g->p2.sockfd, board_msg);
   send_board_to_watchers(g, board_msg);

   int over = isGameOver(g);
   if(over != 0)
   {
      char end_msg[BUF_SIZE];
      if(over == 1)
         snprintf(end_msg, sizeof(end_msg), "*** Game Over! %s (Player 1) wins! (%d - %d) ***\n", g->p1.username, g->p1_score, g->p2_score);
      else if(over == 2)
         snprintf(end_msg, sizeof(end_msg), "*** Game Over! %s (Player 2) wins! (%d - %d) ***\n", g->p2.username, g->p2_score, g->p1_score);
      else
         snprintf(end_msg, sizeof(end_msg), "*** Game Over! It's a tie! (%d - %d) ***\n", g->p1_score, g->p2_score);

      write_client(g->p1.sockfd, end_msg);
      write_client(g->p2.sockfd, end_msg);
      send_board_to_watchers(g, end_msg);
      g->is_active = 0;
   }
   else
   {
      SOCKET next_sock = (g->current_player == 1) ? g->p1.sockfd : g->p2.sockfd;
      SOCKET wait_sock = (g->current_player == 1) ? g->p2.sockfd : g->p1.sockfd;
      write_client(next_sock, "It's your turn! Type 'move <0-5>'\n");
      write_client(wait_sock, "Waiting for opponent to move...\n");
   }
   return "";
}

static const char* handle_quit_cmd(ServerContext* ctx, int argc, char** args, size_t user_index, size_t* sent_to) {
    (void)ctx;
    (void)argc;
    (void)args;
    *sent_to = user_index;
    return "*** You have quit the game. Goodbye! ***\n";
}

static const char* handle_save_game_cmd(ServerContext* ctx, int argc, char** args,
                                        size_t user_index, size_t* sent_to) {
    (void)ctx;
    (void)argc;
    (void)args;
    *sent_to = user_index;
    return "Save game is not implemented yet.\n";
}

static const CommandMapping COMMAND_TABLE[] = {
    { ADD_BIO_COMMAND,  handle_add_bio,  "add_bio <bio>" },
    { GET_ONLINE_USERS_COMMAND, get_online_users, "get_online_users" },
    { ADD_FRIEND_COMMAND, handle_add_friend, "add_friend <username>" },
    { CHANGE_MODE_COMMAND, handle_change_mode, "change_mode <public|private>" },
    { CHAT_COMMAND, handle_send_message, "chat: <message>" },
    { CHALLENGE_COMMAND, handle_challenge_cmd, "challenge <username>" },
    { ACCEPT_CHALLENGE_COMMAND, handle_accept_cmd, "accept <username>" },
    { NEXT_MOVE_COMMAND, handle_move_cmd, "next_move <pit_number>" },
    { ONLINE_GAMES_COMMAND, handle_online_games_cmd, "online_games" },
    { WATCH_COMMAND, handle_watch_game_cmd, "watch game <id>" },
    { SAVE_GAME_COMMAND, handle_save_game_cmd, "save_game" },
    { QUIT_COMMAND, handle_quit_cmd, "quit" } 
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
