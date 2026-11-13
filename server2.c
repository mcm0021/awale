#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <time.h>

#include "server2.h"

#define MAX_GAMES 10

void start_game(Game *active_games, int max_games, User *p1, User *p2)
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

   /* the server randomly decides who starts: Player 1 always moves first */
   if(rand() % 2)
   {
      User *tmp = p1;
      p1 = p2;
      p2 = tmp;
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

   write_client(p1->sockfd, "You start! Type a pit number (0-5).\n");
   write_client(p2->sockfd, "Waiting for Player 1 to move...\n");
}

/* Leaderboard: adds amount to the score of the online user using this socket. */
void add_score(User *online_users, int actual, SOCKET sockfd, int amount)
{
   for(int u = 0; u < actual; u++)
   {
      if(online_users[u].sockfd == sockfd)
         online_users[u].score += amount;
   }
}

void send_board_to_watchers(Game *game, const char *message)
{
   for(int i = 0; i < game->watcher_count; i++)
      write_client(game->watchers[i], message);
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
#else
   /* writing to a client that has just disconnected must not kill the server */
   signal(SIGPIPE, SIG_IGN);
#endif
   srand((unsigned)time(NULL));
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

         /* usernames must be unique (challenges and bios use them) */
         int taken = 0;
         for(i = 0; i < actual; i++)
         {
            if(strcmp(online_users[i].username, buffer) == 0)
               taken = 1;
         }
         if(taken || buffer[0] == '\0')
         {
            write_client(csock, "Username already taken or empty. Reconnect with another name.\n");
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
                           add_score(online_users, actual, active_games[g].p2.sockfd, 1);
                           send_board_to_watchers(&active_games[g], "*** A player disconnected. Game over. ***\n");
                           active_games[g].is_active = 0;
                        }
                        else if(active_games[g].p2.sockfd == user.sockfd)
                        {
                           write_client(active_games[g].p1.sockfd, "*** Opponent disconnected! You win by forfeit. ***\n");
                           add_score(online_users, actual, active_games[g].p1.sockfd, 1);
                           send_board_to_watchers(&active_games[g], "*** A player disconnected. Game over. ***\n");
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

                  /* cancel challenges sent by this user */
                  for(int u = 0; u < actual; u++)
                  {
                     if(strcmp(online_users[u].pending_challenge_from, user.username) == 0)
                        online_users[u].pending_challenge_from[0] = 0;
                  }
                  for(int s = 0; s < 10; s++)
                     free(user.saved_games[s]);

                  remove_client(online_users, i, &actual);
                  strncpy(buffer, user.username, BUF_SIZE - 1);
                  buffer[BUF_SIZE - 1] = 0;
                  strncat(buffer, " disconnected !", BUF_SIZE - strlen(buffer) - 1);
                  send_message_to_all_clients(online_users, user, actual, buffer, 1);
               }
               else
               {
                  /* Handle command (parsing is done in command_handler.c) */
                  ctx.user_count = actual;
                  size_t sent_to = i;
                  printf("Command from %s: %s\n", user.username, buffer);
                  handle_command(&ctx, buffer, buffer, BUF_SIZE, i, &sent_to);

                  if (sent_to == SIZE_MAX) {
                      send_message_to_all_clients(online_users, user, actual, buffer, 0);
                  } else if (buffer[0] != '\0') {
                      write_client(online_users[sent_to].sockfd, buffer);
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

   int reuse = 1;
   setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)); /* restart without "address in use" */

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
      perror("send()"); /* the client is gone; recv() will report it */
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
