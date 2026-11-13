CC = gcc
CFLAGS = -Wall -Wextra -Werror

TARGETS = server2 client2 awale_01

all: $(TARGETS)

server2: server2.c user.c game.c parser.c server2.h user.h game.h
	$(CC) $(CFLAGS) server2.c user.c game.c parser.c -o server2

client2: client2.c client2.h
	$(CC) $(CFLAGS) client2.c -o client2

awale_01: awale_01.c
	$(CC) $(CFLAGS) awale_01.c -o awale_01
	
clean:
	rm -f $(TARGETS) server client *.o

.PHONY: all clean
