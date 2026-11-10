#ifndef SERVER_H
#define SERVER_H

#ifdef WIN32

#include <winsock2.h>

#elif defined (linux) || defined (__APPLE__)

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h> /* close */
#include <netdb.h> /* gethostbyname */
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket(s) close(s)
typedef int SOCKET;
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr SOCKADDR;
typedef struct in_addr IN_ADDR;

#else

#error not defined for this platform

#endif

#define CRLF        "\r\n"
#define PORT         1977
#define MAX_CLIENTS     100
#define MAX_GAMES       10

#define BUF_SIZE    1024

#include "user.h"
#include "game.h"

typedef struct ServerContext ServerContext;

struct ServerContext {
    User* online_users;
    int user_capacity;
    int user_count;
    Game* active_games;
    int game_capacity;
    int game_count;
};

int handle_command(ServerContext* ctx, const char* command_line, char* response_buffer, size_t buffer_size, size_t user_index, size_t* sent_to);
const char* dispatch_command(ServerContext* ctx, char* line, size_t user_index, size_t* sent_to);

void init(void);
void end(void);
void app(void);
int init_connection(void);
void end_connection(int sock);
int read_client(SOCKET sock, char *buffer);
void write_client(SOCKET sock, const char *buffer);
void send_message_to_all_clients(User *clients, User sender, int actual, const char *buffer, char from_server);
void remove_client(User *clients, int to_remove, int *actual);
void clear_clients(User *clients, int actual);

#endif /* guard */
