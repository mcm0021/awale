# Awale (Oware) Network Game

Awale is a two-player seed-sowing board game, also known as Oware. This
project provides:

- A TCP server that accepts multiple players.
- Terminal clients that can chat and play games.
- Challenges between any two online players.
- Live game listings and spectators.
- Public and private game visibility.
- Saved games that players can review.
- A simple leaderboard.
- A standalone local version of the game.

The network implementation is written in C and uses TCP sockets with
`select()` to handle the server, players, chat, and spectators.

## How the Game Works

Each player has six pits containing four seeds at the beginning of a game.
Players take turns choosing one of their pits. The seeds from that pit are
sown one at a time into the following pits (the starting pit is skipped when
there are 12 seeds or more).

If the last seed lands on the opponent's side and makes that pit hold 2 or 3
seeds, those seeds are captured, together with the previous pits on the
opponent's side that also hold 2 or 3 seeds.

The server checks every move:

- The pit must be one of the player's own pits (`0` to `5`) and not empty.
- If the opponent has no seeds, the player must play a move that gives them
  seeds.
- A move that would capture all of the opponent's seeds captures nothing.

The game ends when a player has captured 25 seeds or more, when fewer than 6
seeds remain on the board, or when the player to move has no legal move (each
player then keeps the seeds on their own side). The player with the higher
score wins.

## Building

Build all programs with:

```bash
make
```

This creates:

- `server2` - the network server.
- `client2` - the network client.
- `awale_01` - the standalone local game.

Remove the generated binaries with:

```bash
make clean
```

Alternatively, compile manually:

```bash
gcc -Wall -Wextra -Werror server2.c user.c game.c command_handler.c -o server2
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

The final argument is the player's username. Usernames must be unique; a
client using a name that is already online is disconnected.

## Network Commands

Commands are typed into a connected client. Each command is sent when the
line is submitted. Replies and errors are sent only to the user who typed the
command; chat messages and disconnect notices go to everyone.

### Online users

```text
get_online_users
```

### Chat

Send a message to every other online user, during or outside a game:

```text
chat: hello everyone
```

Other clients receive `Alice: hello everyone`.

### Bios

A bio has up to 10 lines. Each `add_bio` adds one line:

```text
add_bio Second year student
add_bio I like board games
```

Show the bio of any online user:

```text
bio Alice
```

### Friends and privacy

```text
add_friend Bob
change_mode private
change_mode public
```

Anyone can watch the games of a public player. Only friends can watch the
games of a private player. If either player of a game is private, a spectator
must be that player's friend.

### Challenge, accept, refuse

```text
challenge Bob
```

Bob is notified and answers with one of:

```text
accept Alice
refuse Alice
```

The server randomly decides who starts. The server rejects challenges to
offline users, to oneself, and to players who are already in a game.

### Play a game

On your turn, type the number of one of your pits:

```text
2
```

The server sends the updated board to both players and to all spectators after
every move. If a player quits or disconnects during a game, the game ends and
nobody gets leaderboard points.

### List and watch ongoing games

```text
online games
watch 1
```

`online games` lists the active games you are allowed to watch. `watch <id>`
sends you the current board, then every update and the final result. You
cannot watch while playing, and you stop watching when your own game starts.

### Saved games

Every finished game is saved for both players, for as long as they stay
connected.

```text
saved games
review 1
```

`saved games` lists your saved games. `review <id>` shows the board after
every move of that game, then the final score.

### Leaderboard

```text
leaderboard
```

Lists the online users from the highest score to the lowest. A win gives
`+1` and a loss gives `-1` (a tie changes nothing). A game ended by a
player quitting or disconnecting changes nothing. Scores start at `+0` when a
user connects and are kept only while the user stays connected.

Example:

```text
Leaderboard:
1. Alice +2
2. Carol +0
3. Bob -1
```

### Quit

```text
quit
```

Disconnects from the server. This is the same as closing the client: if you
are in a game, the game ends without leaderboard points.

## Standalone Local Game

The standalone version does not use the network server:

```bash
./awale_01
```

It supports two local players in the same terminal:

- Player 1 chooses pits `0` through `5`.
- Player 2 chooses pits `6` through `11`.

## Project Files

- `server2.c` - TCP server: connections, `select()` loop, starting games,
  disconnects.
- `client2.c` - terminal TCP client.
- `command_handler.c` - parses every command and runs it: chat, bios,
  friends, privacy, challenges, moves, watching, saved games, leaderboard.
- `user.c` / `user.h` - user data: bio, friends, privacy, saved games, score.
- `game.c` / `game.h` - board, moves, captures, legality checks, game end.
- `awale_01.c` - standalone local game.
- `Makefile` - build and clean commands.
