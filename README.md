# Awale (Oware) Network Game

Awale is a two-player seed-sowing board game, also known as Oware. This
project provides:

- A TCP server that accepts multiple players.
- Terminal clients that can chat and play games.
- Challenges between any two online players.
- Live game listings and spectators.
- Public and private game visibility.
- A standalone local version of the game.

The network implementation is written in C and uses TCP sockets with
`select()` to handle the server, players, chat, and spectators.

## How the Game Works

Each player has six pits containing four seeds at the beginning of a game.
Players take turns choosing one of their pits. The seeds from that pit are
sown one at a time into the following pits. Captures are made according to the
Awale/Oware rules implemented by the server.

The network board labels Player 1's pits from `0` to `5` and Player 2's pits
from `0` to `5`. On a player's turn, choose one of their own non-empty pits:

```text
move 2
```

The server sends the updated board to both players after every move. A game
ends when a player reaches 25 captured seeds or when the remaining board state
ends the game. The player with the higher score wins.

## Building

Build all programs with:

```bash
make
```

This creates:

- `server2` - the network server.
- `client2` - the network client.
- `awale_01` - the standalone local game.

Remove generated binaries with:

```bash
make clean
```

Alternatively, compile manually:

```bash
gcc -Wall -Wextra -Werror server2.c user.c game.c parser.c -o server2
gcc -Wall -Wextra -Werror client2.c -o client2
gcc -Wall -Wextra -Werror awale_01.c -o awale_01
```

## Running the Network Game

The server listens on TCP port `1977`.

### 1. Start the server

In the first terminal:

```bash
./server2
```

Press `Enter` in the server terminal to stop it, or use `Ctrl+C`.

### 2. Connect players

In a second terminal:

```bash
./client2 127.0.0.1 Alice
```

In another terminal:

```bash
./client2 127.0.0.1 Bob
```

The final argument is the player's username. Start additional clients with
different usernames to use chat or watch games.

## Network Commands

Commands are typed into a connected client. Each command is sent when the
line is submitted.

### Global chat

Send a message to every other online user:

```text
chat: hello everyone
```

Other clients receive:

```text
Alice: hello everyone
```

The sender does not receive a second copy of their own chat message.

The older command form is also accepted:

```text
send_message hello everyone
```

### Online users

List the currently connected users:

```text
get_online_users
```

The response includes each username and their bio.

### Bios

Set a user's bio:

```text
add_bio student
```

### Friends and privacy

Add an online user as a friend:

```text
add_friend Bob
```

Change visibility mode:

```text
change_mode public
change_mode private
```

Public mode allows all online users to see the player's ongoing games.
Private mode limits visibility of a game to:

- The players in that game.
- Friends added by the private player.

For a game to be visible to a spectator, the spectator must be allowed by
both players. Therefore, if either player is private, spectators must be
friends with that private player.

### Challenge and accept a game

Challenge another online player:

```text
challenge -Bob
```

The equivalent form is:

```text
challenge Bob
```

Bob receives a challenge notification. Bob can accept it with:

```text
accept Alice
```

If Bob has only one pending challenge, this is also sufficient:

```text
accept
```

The server rejects challenges to offline users, oneself, or players already in
another game.

### Play a game

After a challenge is accepted, both players receive the game identifier and
the initial board. The player whose turn it is can make a move with:

```text
move 2
```

`next_move 2` is also accepted. Pits must be numbered from `0` to `5`, and the
selected pit must contain seeds. The server rejects moves made out of turn,
invalid pit numbers, and empty pits.

### List ongoing games

List active games visible to the current user:

```text
online games
```

Example:

```text
Game 1: Alice vs Bob
Game 2: Carol vs Dave
```

Private games that the user is not allowed to view are omitted.

### Watch an ongoing game

Use the game ID from `online games`:

```text
watch game -1
```

The watcher immediately receives the current board. After that, every board
update and the final game-over message are sent to the watcher in real time.
A player cannot watch their own game.

If the game no longer exists, is not active, or is private to the requester,
the server reports an error instead.

## Standalone Local Game

The standalone version does not use the network server:

```bash
./awale_01
```

It supports two local players in the same terminal:

- Player 1 chooses pits `0` through `5`.
- Player 2 chooses pits `6` through `11`.

## Project Files

- `server2.c` - TCP server, client management, challenges, games, and
  spectators.
- `client2.c` - terminal TCP client.
- `parser.c` - command parsing for chat and user commands.
- `user.c` / `user.h` - user, friend, bio, and privacy data.
- `game.c` / `game.h` - board state, moves, captures, scores, and game IDs.
- `awale_01.c` - standalone local game.
- `Makefile` - build and clean commands.
