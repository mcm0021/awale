#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "server2.h"
#include "user.h"
#include <stdint.h>

#define MAX_ARGS 100
#define ADD_BIO_COMMAND "add_bio"
#define BIO_COMMAND "bio"
#define ADD_FRIEND_COMMAND "add_friend"
#define CHANGE_MODE_COMMAND "change_mode"
#define CHAT_COMMAND "chat:"
#define GET_ONLINE_USERS_COMMAND "get_online_users"
#define LEADERBOARD_COMMAND "leaderboard"

#define CHALLENGE_COMMAND "challenge"
#define ACCEPT_COMMAND "accept"
#define REFUSE_COMMAND "refuse"

#define ONLINE_GAMES_COMMAND "online"
#define WATCH_COMMAND "watch"
#define SAVED_GAMES_COMMAND "saved"
#define REVIEW_COMMAND "review"
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
      if(strcmp(owner->friends[i], viewer->username) == 0)
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

static int is_in_game(ServerContext *ctx, SOCKET sockfd)
{
   for(int g = 0; g < ctx->game_capacity; g++)
   {
      Game *game = &ctx->active_games[g];
      if(game->is_active && (game->p1.sockfd == sockfd || game->p2.sockfd == sockfd))
         return 1;
   }
   return 0;
}

static int find_user(ServerContext *ctx, const char *username)
{
   for(int j = 0; j < ctx->user_count; j++)
   {
      if(strcmp(ctx->online_users[j].username, username) == 0)
         return j;
   }
   return -1;
}

/* Removes this socket from the spectators of every game. */
static void stop_watching(ServerContext *ctx, SOCKET sockfd)
{
   for(int g = 0; g < ctx->game_capacity; g++)
   {
      Game *game = &ctx->active_games[g];
      for(int w = 0; w < game->watcher_count; w++)
      {
         if(game->watchers[w] == sockfd)
         {
            memmove(&game->watchers[w], &game->watchers[w + 1],
                    (game->watcher_count - w - 1) * sizeof(int));
            game->watcher_count--;
            w--;
         }
      }
   }
}

/* Called when a user quits or disconnects: the game ends (no leaderboard points),
   the user stops watching, its challenges are cancelled, and it is removed
   from the online users. */
void remove_user(ServerContext *ctx, int index)
{
   User user = ctx->online_users[index];
   closesocket(user.sockfd);

   for(int g = 0; g < ctx->game_capacity; g++)
   {
      Game *game = &ctx->active_games[g];
      if(!game->is_active || (game->p1.sockfd != user.sockfd && game->p2.sockfd != user.sockfd))
         continue;
      SOCKET winner = (game->p1.sockfd == user.sockfd) ? game->p2.sockfd : game->p1.sockfd;
      write_client(winner, "*** Opponent left the game. Game over (no points). ***\n");
      send_board_to_watchers(game, "*** A player disconnected. Game over. ***\n");
      game->is_active = 0;
   }
   stop_watching(ctx, user.sockfd);

   /* cancel challenges sent by this user */
   for(int u = 0; u < ctx->user_count; u++)
   {
      if(strcmp(ctx->online_users[u].pending_challenge_from, user.username) == 0)
         ctx->online_users[u].pending_challenge_from[0] = 0;
   }
   for(int s = 0; s < 10; s++)
      free(user.saved_games[s]);

   remove_client(ctx->online_users, index, &ctx->user_count);

   char msg[BUF_SIZE];
   snprintf(msg, sizeof(msg), "%s disconnected !", user.username);
   send_message_to_all_clients(ctx->online_users, user, ctx->user_count, msg, 1);
}

const char* handle_add_bio(ServerContext* ctx, int argc, char** args, size_t user_index, size_t* sent_to) {
    *sent_to = user_index;
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

    int idx = find_user(ctx, args[1]);
    if (idx == -1) {
        return "Error: User not found.\n";
    }

    User* user = &ctx->online_users[idx];
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
    *sent_to = user_index;
    if (argc < 2) {
        return "Error: Friend username is required. Usage: add_friend <username>\n";
    }

    if (find_user(ctx, args[1]) == -1) {
        return "Error: User not found.\n";
    }
    if (!addFriend(&ctx->online_users[user_index], args[1])) {
        return "Error: Friend list is full.\n";
    }
    return "Friend added successfully.\n";
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
        return "Mode changed to public.\n";
    } else if (strcmp(mode, "private") == 0) {
        user->isprivate = true;
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
    *sent_to = SIZE_MAX; /* broadcast */
    return message;
}

const char* handle_leaderboard(ServerContext* ctx, int argc, char** args, size_t user_index, size_t* sent_to) {
    (void)argc;
    (void)args;
    static char response[BUF_SIZE];
    int order[MAX_USERS];
    *sent_to = user_index;

    for (int i = 0; i < ctx->user_count; i++) {
        order[i] = i;
    }
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

static const char *handle_challenge_cmd(ServerContext *ctx, int argc, char **args,
                                        size_t user_index, size_t *sent_to)
{
   User *sender = &ctx->online_users[user_index];
   static char response[BUF_SIZE];
   *sent_to = user_index;

   if(argc < 2)
      return "Usage: challenge <username>\n";
   if(strcmp(sender->username, args[1]) == 0)
      return "You cannot challenge yourself!\n";
   if(is_in_game(ctx, sender->sockfd))
      return "You are already playing a game!\n";

   int target_idx = find_user(ctx, args[1]);
   if(target_idx == -1)
   {
      snprintf(response, sizeof(response), "User '%s' is not online.\n", args[1]);
      return response;
   }

   User *opponent = &ctx->online_users[target_idx];
   if(is_in_game(ctx, opponent->sockfd))
   {
      snprintf(response, sizeof(response), "User '%s' is currently busy in another game.\n", args[1]);
      return response;
   }

   /* Record pending challenge */
   strncpy(opponent->pending_challenge_from, sender->username, MAX_USERNAME_LEN - 1);
   opponent->pending_challenge_from[MAX_USERNAME_LEN - 1] = 0;

   char notify[BUF_SIZE];
   snprintf(notify, sizeof(notify), "*** %s has challenged you to an Awale game! Type 'accept %s' or 'refuse %s'. ***\n", sender->username, sender->username, sender->username);
   write_client(opponent->sockfd, notify);

   snprintf(response, sizeof(response), "*** Challenge sent to %s. Waiting for response... ***\n", args[1]);
   return response;
}

static const char *handle_accept_cmd(ServerContext *ctx, int argc, char **args,
                                     size_t user_index, size_t *sent_to)
{
   User *sender = &ctx->online_users[user_index];
   *sent_to = user_index;

   if(argc < 2)
      return "Usage: accept <username>\n";

   int challenger_idx = find_user(ctx, args[1]);
   if(challenger_idx == -1 || strcmp(sender->pending_challenge_from, args[1]) != 0)
      return "No pending challenge found from that user.\n";

   User *challenger = &ctx->online_users[challenger_idx];
   if(is_in_game(ctx, challenger->sockfd) || is_in_game(ctx, sender->sockfd))
      return "One of you is already playing a game.\n";

   sender->pending_challenge_from[0] = 0;
   /* players stop watching other games when their own game starts */
   stop_watching(ctx, challenger->sockfd);
   stop_watching(ctx, sender->sockfd);
   start_game(ctx->active_games, ctx->game_capacity, challenger, sender);
   return "";
}

static const char *handle_refuse_cmd(ServerContext *ctx, int argc, char **args,
                                     size_t user_index, size_t *sent_to)
{
   User *sender = &ctx->online_users[user_index];
   *sent_to = user_index;

   if(argc < 2)
      return "Usage: refuse <username>\n";
   if(sender->pending_challenge_from[0] == 0 || strcmp(sender->pending_challenge_from, args[1]) != 0)
      return "No pending challenge found from that user.\n";

   int challenger_idx = find_user(ctx, args[1]);
   if(challenger_idx != -1)
   {
      char msg[BUF_SIZE];
      snprintf(msg, sizeof(msg), "*** %s refused your challenge. ***\n", sender->username);
      write_client(ctx->online_users[challenger_idx].sockfd, msg);
   }
   sender->pending_challenge_from[0] = 0;
   return "Challenge refused.\n";
}

static const char *handle_move_cmd(ServerContext *ctx, int argc, char **args,
                                   size_t user_index, size_t *sent_to)
{
   User *online_users = ctx->online_users;
   int actual = ctx->user_count;
   User *sender = &online_users[user_index];
   *sent_to = user_index;

   int pit = (argc == 1 && strlen(args[0]) == 1) ? args[0][0] - '0' : -1;

   Game *g = NULL;
   int player_num = 0;
   for(int i = 0; i < ctx->game_capacity; i++)
   {
      Game *game = &ctx->active_games[i];
      if(!game->is_active)
         continue;
      if(game->p1.sockfd == sender->sockfd)
      {
         g = game;
         player_num = 1;
         break;
      }
      if(game->p2.sockfd == sender->sockfd)
      {
         g = game;
         player_num = 2;
         break;
      }
   }

   if(g == NULL)
      return "You are not currently in an active game.\n";
   if(g->current_player != player_num)
      return "It is not your turn! Please wait for your opponent.\n";
   if(pit < 0 || pit > 5)
      return "Invalid pit. Type a pit number from 0 to 5.\n";

   int seeds = (player_num == 1) ? g->p1_board[pit] : g->p2_board[pit];
   if(seeds == 0)
      return "Selected pit is empty. Choose another pit (0-5).\n";
   if(!isLegalMove(g, player_num, pit))
      return "Illegal move: your opponent has no seeds, you must give them some.\n";

   if(g->move_count < MAX_MOVES)
      g->moves[g->move_count++] = pit;

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
   if(over == 0)
   {
      SOCKET next_sock = (g->current_player == 1) ? g->p1.sockfd : g->p2.sockfd;
      SOCKET wait_sock = (g->current_player == 1) ? g->p2.sockfd : g->p1.sockfd;
      write_client(next_sock, "It's your turn! Type a pit number (0-5).\n");
      write_client(wait_sock, "Waiting for opponent to move...\n");
      return "";
   }

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

   if(over == 1)
   {
      add_score(online_users, actual, g->p1.sockfd, 1);
      add_score(online_users, actual, g->p2.sockfd, -1);
   }
   else if(over == 2)
   {
      add_score(online_users, actual, g->p2.sockfd, 1);
      add_score(online_users, actual, g->p1.sockfd, -1);
   }

   /* save the finished game for both players so they can review it later */
   for(int u = 0; u < actual; u++)
   {
      if(online_users[u].sockfd == g->p1.sockfd || online_users[u].sockfd == g->p2.sockfd)
      {
         saveGame(&online_users[u], g);
         write_client(online_users[u].sockfd, "Game saved. Type 'saved games' to list your saved games.\n");
      }
   }
   return "";
}

static const char *handle_online_games_cmd(ServerContext *ctx, int argc, char **args,
                                           size_t user_index, size_t *sent_to)
{
   User *viewer = &ctx->online_users[user_index];
   static char response[BUF_SIZE];
   response[0] = '\0';
   *sent_to = user_index;

   if(argc != 2 || strcmp(args[1], "games") != 0)
      return "Usage: online games\n";

   for(int g = 0; g < ctx->game_capacity; g++)
   {
      Game *game = &ctx->active_games[g];
      if(game->is_active && can_view_game(ctx->online_users, ctx->user_count, game, viewer))
      {
         size_t used = strlen(response);
         snprintf(response + used, sizeof(response) - used,
                  "Game %d: %s vs %s\n", game->id, game->p1.username, game->p2.username);
      }
   }

   if(response[0] == '\0')
      return "No viewable games are currently in progress.\n";
   return response;
}

static const char *handle_watch_game_cmd(ServerContext *ctx, int argc, char **args,
                                         size_t user_index, size_t *sent_to)
{
   User *viewer = &ctx->online_users[user_index];
   *sent_to = user_index;

   if(argc < 2)
      return "Usage: watch <id>\n";
   if(is_in_game(ctx, viewer->sockfd))
      return "You cannot watch a game while playing one.\n";

   char *end = NULL;
   long requested_id = strtol(args[1], &end, 10);
   if(end == args[1] || *end != '\0' || requested_id < 0)
      return "Usage: watch <id>\n";

   for(int g = 0; g < ctx->game_capacity; g++)
   {
      Game *game = &ctx->active_games[g];
      if(!game->is_active || game->id != requested_id)
         continue;
      if(!can_view_game(ctx->online_users, ctx->user_count, game, viewer))
         return "You cannot watch this private game.\n";
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

static const char *handle_saved_games_cmd(ServerContext *ctx, int argc, char **args,
                                          size_t user_index, size_t *sent_to)
{
   User *user = &ctx->online_users[user_index];
   static char response[BUF_SIZE];
   response[0] = '\0';
   *sent_to = user_index;

   if(argc != 2 || strcmp(args[1], "games") != 0)
      return "Usage: saved games\n";

   for(int i = 0; i < 10; i++)
   {
      Game *g = user->saved_games[i];
      if(g != NULL)
      {
         size_t used = strlen(response);
         snprintf(response + used, sizeof(response) - used, "Game %d: %s vs %s (%d - %d)\n",
                  g->id, g->p1.username, g->p2.username, g->p1_score, g->p2_score);
      }
   }
   if(response[0] == '\0')
      return "You have no saved games.\n";
   return response;
}

static const char *handle_review_cmd(ServerContext *ctx, int argc, char **args,
                                     size_t user_index, size_t *sent_to)
{
   User *user = &ctx->online_users[user_index];
   *sent_to = user_index;

   if(argc < 2)
      return "Usage: review <id>\n";

   int id = atoi(args[1]);
   for(int i = 0; i < 10; i++)
   {
      Game *saved = user->saved_games[i];
      if(saved == NULL || saved->id != id)
         continue;

      Game replay = *saved;
      resetBoard(&replay);
      char board_msg[BUF_SIZE];
      for(int m = 0; m <= saved->move_count; m++)
      {
         if(m > 0)
         {
            if(replay.current_player == 1)
            {
               p1Move(&replay, saved->moves[m - 1]);
               replay.current_player = 2;
            }
            else
            {
               p2Move(&replay, saved->moves[m - 1]);
               replay.current_player = 1;
            }
         }
         getBoardString(&replay, board_msg, sizeof(board_msg));
         write_client(user->sockfd, board_msg);
      }
      static char response[BUF_SIZE];
      snprintf(response, sizeof(response), "Final score: %s %d - %d %s\n",
               saved->p1.username, saved->p1_score, saved->p2_score, saved->p2.username);
      return response;
   }
   return "Saved game not found. Type 'saved games' to list them.\n";
}

static const char *handle_quit_cmd(ServerContext *ctx, int argc, char **args,
                                   size_t user_index, size_t *sent_to)
{
   (void)argc;
   (void)args;
   *sent_to = user_index;
   write_client(ctx->online_users[user_index].sockfd, "Goodbye!\n");
   remove_user(ctx, (int)user_index);
   return ""; /* the user is gone: nothing more to send */
}

static const CommandMapping COMMAND_TABLE[] = {
    { ADD_BIO_COMMAND,  handle_add_bio,  "add_bio <bio line>" },
    { BIO_COMMAND, handle_bio, "bio <username>" },
    { GET_ONLINE_USERS_COMMAND, get_online_users, "get_online_users" },
    { ADD_FRIEND_COMMAND, handle_add_friend, "add_friend <username>" },
    { CHANGE_MODE_COMMAND, handle_change_mode, "change_mode <public|private>" },
    { CHAT_COMMAND, handle_send_message, "chat: <message>" },
    { LEADERBOARD_COMMAND, handle_leaderboard, "leaderboard" },
    { CHALLENGE_COMMAND, handle_challenge_cmd, "challenge <username>" },
    { ACCEPT_COMMAND, handle_accept_cmd, "accept <username>" },
    { REFUSE_COMMAND, handle_refuse_cmd, "refuse <username>" },
    { ONLINE_GAMES_COMMAND, handle_online_games_cmd, "online games" },
    { WATCH_COMMAND, handle_watch_game_cmd, "watch <id>" },
    { SAVED_GAMES_COMMAND, handle_saved_games_cmd, "saved games" },
    { REVIEW_COMMAND, handle_review_cmd, "review <id>" },
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
        response_buffer[0] = '\0'; /* empty line: nothing to answer */
        return -1;
    }
}

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

const char* dispatch_command(ServerContext* ctx, char* line, size_t user_index, size_t* sent_to) {
    char* args[MAX_ARGS];
    int argc = parse_arguments(line, args, MAX_ARGS);
    *sent_to = user_index;

    if (argc == 0) {
        return NULL;
    }

    if (args[0][0] >= '0' && args[0][0] <= '9') {
        return handle_move_cmd(ctx, argc, args, user_index, sent_to);
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
