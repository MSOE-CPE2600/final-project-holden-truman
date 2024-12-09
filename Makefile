CC=gcc
CFLAGS=-Wall -g

all: server votingmachine

server: server.c
	$(CC) $(CFLAGS) -o server server.c `pkg-config --cflags --libs glib-2.0`

votingmachine: votingmachine.c
	$(CC) $(CFLAGS) votingmachine.c -o votingmachine

clean:
	rm -rf server votingmachine
