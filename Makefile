CC=gcc
CFLAGS=-Wall -g `pkg-config --cflags glib-2.0`
LIBS=`pkg-config --libs glib-2.0`

# Targets for the final executables
all: server votingmachine

# Rule for compiling the server executable
server: server.o fileio.o
	$(CC) $(CFLAGS) -o server server.o fileio.o $(LIBS)

# Rule for compiling the votingmachine executable
votingmachine: votingmachine.o
	$(CC) $(CFLAGS) -o votingmachine votingmachine.o $(LIBS)

# Rule for compiling server.c into an object file
server.o: server.c shared_data.h fileio.h server.h
	$(CC) $(CFLAGS) -c server.c

# Rule for compiling fileio.c into an object file
fileio.o: fileio.c fileio.h
	$(CC) $(CFLAGS) -c fileio.c

# Rule for compiling votingmachine.c into an object file
votingmachine.o: votingmachine.c
	$(CC) $(CFLAGS) -c votingmachine.c

# Clean up the generated files
clean:
	rm -f server votingmachine *.o
