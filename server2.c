#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>

#include "server2.h"

#define MAX_GAMES 10

static void start_game(Game *active_games, int max_games, User *p1, User *p2)
{
   int slot = -1;
   for(int g = 0; g < max_games; g++)
   {
      if(!active_games[g].is_active)
      {
         slot = g;
         break;
      }
   }
   if(slot == -1)
   {
      write_client(p1->sockfd, "Cannot start game: server game limit reached.\n");
      write_client(p2->sockfd, "Cannot start game: server game limit reached.\n");
      return;
   }

   initGame(&active_games[slot], p1, p2);

   char start_msg[BUF_SIZE];
   snprintf(start_msg, sizeof(start_msg), "*** Game started! %s (Player 1) vs %s (Player 2) ***\n", p1->username, p2->username);
   write_client(p1->sockfd, start_msg);
   write_client(p2->sockfd, start_msg);

   char board_msg[BUF_SIZE];
   getBoardString(&active_games[slot], board_msg, sizeof(board_msg));
   write_client(p1->sockfd, board_msg);
   write_client(p2->sockfd, board_msg);

   write_client(p1->sockfd, "It's your turn, Player 1! Type 'move <0-5>'\n");
   write_client(p2->sockfd, "Waiting for Player 1 to move...\n");
}

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

static void handle_online_games_cmd(User *online_users, int actual, int sender_idx,
                                    Game *active_games, int max_games)
{
   User *viewer = &online_users[sender_idx];
   char response[BUF_SIZE];
   response[0] = '\0';
   int found = 0;

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
   write_client(viewer->sockfd, response);
}

static void handle_watch_game_cmd(User *online_users, int actual, int sender_idx,
                                  const char *buffer, Game *active_games, int max_games)
{
   User *viewer = &online_users[sender_idx];
   const char *p = buffer + 10;
   while(*p == ' ' || *p == '\t') p++;
   if(*p == '-') p++;

   char *end = NULL;
   long requested_id = strtol(p, &end, 10);
   if(end == p || requested_id < 0)
   {
      write_client(viewer->sockfd, "Usage: watch game -<id>\n");
      return;
   }

   for(int g = 0; g < max_games; g++)
   {
      Game *game = &active_games[g];
      if(!game->is_active || game->id != requested_id)
         continue;
      if(!can_view_game(online_users, actual, game, viewer))
      {
         write_client(viewer->sockfd, "You cannot watch this private game.\n");
         return;
      }
      if(game->p1.sockfd == viewer->sockfd || game->p2.sockfd == viewer->sockfd)
      {
         write_client(viewer->sockfd, "You are already playing this game.\n");
         return;
      }
      for(int w = 0; w < game->watcher_count; w++)
      {
         if(game->watchers[w] == viewer->sockfd)
         {
            write_client(viewer->sockfd, "You are already watching this game.\n");
            return;
         }
      }
      if(game->watcher_count == MAX_GAME_WATCHERS)
      {
         write_client(viewer->sockfd, "This game has reached its watcher limit.\n");
         return;
      }

      game->watchers[game->watcher_count++] = viewer->sockfd;
      write_client(viewer->sockfd, "Now watching game ");
      char confirmation[64];
      snprintf(confirmation, sizeof(confirmation), "%d.\n", game->id);
      write_client(viewer->sockfd, confirmation);
      char board_msg[BUF_SIZE];
      getBoardString(game, board_msg, sizeof(board_msg));
      write_client(viewer->sockfd, board_msg);
      return;
   }
   write_client(viewer->sockfd, "Game not found or no longer active.\n");
}

static void send_board_to_watchers(Game *game, const char *message)
{
   for(int i = 0; i < game->watcher_count; i++)
      write_client(game->watchers[i], message);
}

static void handle_challenge_cmd(User *online_users, int actual, int sender_idx, const char *buffer, Game *active_games, int max_games)
{
   User *sender = &online_users[sender_idx];
   char target[MAX_USERNAME_LEN];
   const char *p = buffer + 9;
   while(*p == ' ' || *p == '\t') p++;
   if(*p == '-') p++; /* supports challenge -target or challenge target */
   while(*p == ' ' || *p == '\t') p++;
   strncpy(target, p, sizeof(target) - 1);
   target[sizeof(target) - 1] = 0;
   char *nl = strpbrk(target, "\r\n ");
   if(nl) *nl = 0;

   if(strlen(target) == 0)
   {
      write_client(sender->sockfd, "Usage: challenge <username> (e.g. challenge Bob or challenge -Bob)\n");
      return;
   }
   if(strcmp(sender->username, target) == 0)
   {
      write_client(sender->sockfd, "You cannot challenge yourself!\n");
      return;
   }

   /* Check if sender is already in a game */
   for(int g = 0; g < max_games; g++)
   {
      if(active_games[g].is_active && (active_games[g].p1.sockfd == sender->sockfd || active_games[g].p2.sockfd == sender->sockfd))
      {
         write_client(sender->sockfd, "You are already playing a game!\n");
         return;
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
      return;
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
         return;
      }
   }

   /* If opponent already challenged sender, start the game immediately */
   if(strcmp(sender->pending_challenge_from, target) == 0)
   {
      start_game(active_games, max_games, opponent, sender);
      sender->pending_challenge_from[0] = 0;
      opponent->pending_challenge_from[0] = 0;
      return;
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
}

static void handle_accept_cmd(User *online_users, int actual, int sender_idx, const char *buffer, Game *active_games, int max_games)
{
   User *sender = &online_users[sender_idx];
   char target[MAX_USERNAME_LEN];
   const char *p = buffer + 6;
   while(*p == ' ' || *p == '\t') p++;
   if(*p == '-') p++;
   while(*p == ' ' || *p == '\t') p++;
   strncpy(target, p, sizeof(target) - 1);
   target[sizeof(target) - 1] = 0;
   char *nl = strpbrk(target, "\r\n ");
   if(nl) *nl = 0;

   if(strlen(target) == 0 && strlen(sender->pending_challenge_from) > 0)
   {
      strncpy(target, sender->pending_challenge_from, sizeof(target) - 1);
      target[sizeof(target) - 1] = 0;
   }

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
      return;
   }

   User *challenger = &online_users[challenger_idx];
   start_game(active_games, max_games, challenger, sender);
   sender->pending_challenge_from[0] = 0;
   challenger->pending_challenge_from[0] = 0;
}

static void handle_move_cmd(User *online_users, int actual, int sender_idx, const char *buffer, Game *active_games, int max_games)
{
   (void)actual;
   User *sender = &online_users[sender_idx];
   const char *p = (strncmp(buffer, "next_move", 9) == 0) ? buffer + 9 : buffer + 4;
   while(*p == ' ' || *p == '\t') p++;
   if(*p == '-') p++;
   int pit = atoi(p);

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
      return;
   }

   Game *g = &active_games[game_idx];
   if(g->current_player != player_num)
   {
      write_client(sender->sockfd, "It is not your turn! Please wait for your opponent.\n");
      return;
   }

   if(pit < 0 || pit > 5)
   {
      write_client(sender->sockfd, "Invalid pit. Please choose 0 to 5 (e.g. 'move 2').\n");
      return;
   }

   int seeds = (player_num == 1) ? g->p1_board[pit] : g->p2_board[pit];
   if(seeds == 0)
   {
      write_client(sender->sockfd, "Selected pit is empty. Choose another pit (0-5).\n");
      return;
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
}

void init(void)
{
#ifdef WIN32
   WSADATA wsa;
   int err = WSAStartup(MAKEWORD(2, 2), &wsa);
   if(err < 0)
   {
      puts("WSAStartup failed !");
      exit(EXIT_FAILURE);
   }
#endif
}

void end(void)
{
#ifdef WIN32
   WSACleanup();
#endif
}

void app(void)
{
   SOCKET sock = init_connection();
   char buffer[BUF_SIZE];
   /* the index for the array */
   int actual = 0;
   int max = sock;
   /* an array for all clients */
   User online_users[MAX_CLIENTS];
   Game active_games[MAX_GAMES];
   memset(active_games, 0, sizeof(active_games));
   ServerContext ctx = {
      .online_users = online_users,
      .user_capacity = MAX_CLIENTS,
      .user_count = 0,
      .active_games = active_games,
      .game_capacity = MAX_GAMES,
      .game_count = 0
   };

   fd_set rdfs;

   while(1)
   {
      int i = 0;
      FD_ZERO(&rdfs);

      /* add STDIN_FILENO */
      FD_SET(STDIN_FILENO, &rdfs);

      /* add the connection socket */
      FD_SET(sock, &rdfs);

      /* add socket of each client */
      for(i = 0; i < actual; i++)
      {
         FD_SET(online_users[i].sockfd, &rdfs);
      }

      if(select(max + 1, &rdfs, NULL, NULL, NULL) == -1)
      {
         perror("select()");
         exit(errno);
      }

      /* something from standard input : i.e keyboard */
      if(FD_ISSET(STDIN_FILENO, &rdfs))
      {
         /* stop process when type on keyboard */
         break;
      }
      else if(FD_ISSET(sock, &rdfs))
      {
         /* new client */
         SOCKADDR_IN csin = { 0 };
         socklen_t sinsize = sizeof csin;
         int csock = accept(sock, (SOCKADDR *)&csin, &sinsize);
         if(csock == SOCKET_ERROR)
         {
            perror("accept()");
            continue;
         }

         /* after connecting the client sends its name */
         if(read_client(csock, buffer) <= 0)
         {
            /* disconnected */
            closesocket(csock);
            continue;
         }

         if(actual >= MAX_CLIENTS)
         {
            const char *msg = "Server full.\n";
            write_client(csock, msg);
            closesocket(csock);
            continue;
         }

         /* what is the new maximum fd ? */
         max = csock > max ? csock : max;

         FD_SET(csock, &rdfs);

         registerUser(&online_users[actual], csock);
         setUsername(&online_users[actual], buffer);
         actual++;
      }
      else
      {
         int i = 0;
         for(i = 0; i < actual; i++)
         {
            /* a client is talking */
            if(FD_ISSET(online_users[i].sockfd, &rdfs))
            {
               User user = online_users[i];
               int c = read_client(online_users[i].sockfd, buffer);
               /* client disconnected */
               if(c == 0)
               {
                  closesocket(online_users[i].sockfd);

                  /* If user was in an active game, notify opponent and end game */
                  for(int g = 0; g < MAX_GAMES; g++)
                  {
                     if(active_games[g].is_active)
                     {
                        if(active_games[g].p1.sockfd == user.sockfd)
                        {
                           write_client(active_games[g].p2.sockfd, "*** Opponent disconnected! You win by forfeit. ***\n");
                           active_games[g].is_active = 0;
                        }
                        else if(active_games[g].p2.sockfd == user.sockfd)
                        {
                           write_client(active_games[g].p1.sockfd, "*** Opponent disconnected! You win by forfeit. ***\n");
                           active_games[g].is_active = 0;
                        }
                     }
                     for(int w = 0; w < active_games[g].watcher_count; w++)
                     {
                        if(active_games[g].watchers[w] == user.sockfd)
                        {
                           memmove(&active_games[g].watchers[w],
                                   &active_games[g].watchers[w + 1],
                                   (active_games[g].watcher_count - w - 1) * sizeof(int));
                           active_games[g].watcher_count--;
                           w--;
                        }
                     }
                  }

                  remove_client(online_users, i, &actual);
                  strncpy(buffer, user.username, BUF_SIZE - 1);
                  buffer[BUF_SIZE - 1] = 0;
                  strncat(buffer, " disconnected !", BUF_SIZE - strlen(buffer) - 1);
                  send_message_to_all_clients(online_users, user, actual, buffer, 1);
               }
               else
               {
                  /* Handle command */
                  ctx.user_count = actual;
                  size_t sent_to = SIZE_MAX; // Initialize sent_to to an invalid value
                  int handled = 0;

                  if (strncmp(buffer, "challenge", 9) == 0 &&
                      (buffer[9] == '\0' || buffer[9] == ' ' || buffer[9] == '\t')) {
                     handle_challenge_cmd(online_users, actual, i, buffer, active_games, MAX_GAMES);
                     handled = 1;
                  } else if (strncmp(buffer, "accept", 6) == 0 &&
                             (buffer[6] == '\0' || buffer[6] == ' ' || buffer[6] == '\t')) {
                     handle_accept_cmd(online_users, actual, i, buffer, active_games, MAX_GAMES);
                     handled = 1;
                  } else if ((strncmp(buffer, "move", 4) == 0 &&
                              (buffer[4] == '\0' || buffer[4] == ' ' || buffer[4] == '\t')) ||
                             (strncmp(buffer, "next_move", 9) == 0 &&
                              (buffer[9] == '\0' || buffer[9] == ' ' || buffer[9] == '\t'))) {
                     handle_move_cmd(online_users, actual, i, buffer, active_games, MAX_GAMES);
                     handled = 1;
                  } else if (strcmp(buffer, "online games") == 0) {
                     handle_online_games_cmd(online_users, actual, i, active_games, MAX_GAMES);
                     handled = 1;
                  } else if (strncmp(buffer, "watch game", 10) == 0 &&
                             (buffer[10] == '\0' || buffer[10] == ' ' || buffer[10] == '\t')) {
                     handle_watch_game_cmd(online_users, actual, i, buffer, active_games, MAX_GAMES);
                     handled = 1;
                  }

                  if (!handled) {
                     handle_command(&ctx, buffer, buffer, BUF_SIZE, i, &sent_to);

                     printf("Command from %s: %s\n", user.username, buffer);

                     if (sent_to == SIZE_MAX) {
                         send_message_to_all_clients(online_users, user, actual, buffer, 0);
                     } else {
                         write_client(online_users[sent_to].sockfd, buffer);
                     }
                  }
               }
               break;
            }
         }
      }
   }

   clear_clients(online_users, actual);
   end_connection(sock);
}

void clear_clients(User *clients, int actual)
{
   int i = 0;
   for(i = 0; i < actual; i++)
   {
      closesocket(clients[i].sockfd);
   }
}

void remove_client(User *clients, int to_remove, int *actual)
{
   /* we remove the client in the array */
   memmove(clients + to_remove, clients + to_remove + 1, (*actual - to_remove - 1) * sizeof(User));
   /* number client - 1 */
   (*actual)--;
}

void send_message_to_all_clients(User *clients, User sender, int actual, const char *buffer, char from_server)
{
   int i = 0;
   char message[BUF_SIZE];
   for(i = 0; i < actual; i++)
   {
      /* we don't send message to the sender */
      if(sender.sockfd != clients[i].sockfd)
      {
         message[0] = 0;
         if(from_server == 0)
         {
            strncpy(message, sender.username, BUF_SIZE - 1);
            message[BUF_SIZE - 1] = 0;
            strncat(message, ": ", sizeof message - strlen(message) - 1);
         }
         strncat(message, buffer, sizeof message - strlen(message) - 1);
         strncat(message, "\n", sizeof message - strlen(message) - 1);
         write_client(clients[i].sockfd, message);
      }
   }
}

int init_connection(void)
{
   SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
   SOCKADDR_IN sin = { 0 };

   if(sock == INVALID_SOCKET)
   {
      perror("socket()");
      exit(errno);
   }

   sin.sin_addr.s_addr = htonl(INADDR_ANY);
   sin.sin_port = htons(PORT);
   sin.sin_family = AF_INET;

   if(bind(sock,(SOCKADDR *) &sin, sizeof sin) == SOCKET_ERROR)
   {
      perror("bind()");
      exit(errno);
   }

   if(listen(sock, MAX_CLIENTS) == SOCKET_ERROR)
   {
      perror("listen()");
      exit(errno);
   }

   return sock;
}

void end_connection(int sock)
{
   closesocket(sock);
}

int read_client(SOCKET sock, char *buffer)
{
   int n = 0;

   if((n = recv(sock, buffer, BUF_SIZE - 1, 0)) < 0)
   {
      perror("recv()");
      /* if recv error we disonnect the client */
      n = 0;
   }

   buffer[n] = 0;

   return n;
}

void write_client(SOCKET sock, const char *buffer)
{
   if(send(sock, buffer, strlen(buffer), 0) < 0)
   {
      perror("send()");
      exit(errno);
   }
}

int main(int argc, char **argv)
{
   (void)argc;
   (void)argv;

   init();

   app();

   end();

   return EXIT_SUCCESS;
}
