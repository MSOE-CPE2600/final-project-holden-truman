//gcc -g -o server server.c `pkg-config --cflags --libs glib-2.0`
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <glib.h> // for hash table
#include <sys/types.h>  // For pid_t
#include <sys/socket.h>
#include <linux/unistd.h>  // for SO_PEERCRED

#include "server.h"
#include "fileio.h"
#include "shared_data.h"

#define PORT 8080
#define BUFFER_SIZE 1024

#define MAX_VOTING_MACHINES 10
#define DEFAULT_VOTERS 2000

// Global variable to hold data for cleanup
SharedData shared_data;

int main(int argc, char* argv[]) {
    signal(SIGINT, handle_sigint);   // Handle Ctrl+C
    signal(SIGTSTP, handle_sigtstp); // Handle Ctrl+Z

    int load_old = 1;

    char arg;
    while((arg = getopt(argc,argv,"l:h"))!=-1) {
		switch(arg) 
		{
			case 'l':
				load_old = atof(optarg);
				break;
            case 'h':
				show_help();
				return 0;;
				break;
		}
	}

    //important info for user (especially for closing program)
    show_help();
    printf("\n");

    // Register the cleanup function to be called at exit
    atexit(cleanup);

    shared_data.vote_total = 0;  // Initialize vote_total to 0
    shared_data.max_voters = (int) DEFAULT_VOTERS; // Default max voters capacity

    if (load_old) {
        if (load_votes(&shared_data.vote_total, &shared_data.max_voters, &shared_data.voters, &shared_data.results) != 0) {
            // If loading votes failed, initialize hash tables
            shared_data.voters = g_hash_table_new_full(g_int_hash, g_int_equal, free_key, free_value);
            shared_data.results = g_hash_table_new_full(g_str_hash, g_str_equal, free_key, free_value);
        }
        printf("Loaded %d voters\n\n", shared_data.vote_total);
    } else {
        printf("Storing new data, overwriting old\n");
        shared_data.voters = g_hash_table_new_full(g_int_hash, g_int_equal, free_key, free_value);
        shared_data.results = g_hash_table_new_full(g_str_hash, g_str_equal, free_key, free_value);
    }
    
    //struct sockaddr_in server, client;
    struct sockaddr_in server;

    // Create socket
    shared_data.server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (shared_data.server_fd == -1) {
        perror("Could not create socket");
        return 1;
    }
    printf("Socket created.\n");

    // Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(PORT);

    // Bind
    if (bind(shared_data.server_fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("Bind failed");
        return 1;
    }
    printf("Bind done.\n");

    // Listen
    listen(shared_data.server_fd, 3);

    while (1) { //stays open forever, closer with ctrl+c
        int client_socket, c;
        struct sockaddr_in client;
        
        // Accept an incoming connection
        printf("Waiting for incoming connections...\n");
        c = sizeof(struct sockaddr_in);
        client_socket = accept(shared_data.server_fd, (struct sockaddr *)&client, (socklen_t *)&c);
        if (client_socket < 0) {
            perror("Accept failed");
            continue;  // Skip this iteration and keep waiting for other connections
        }
        printf("Connection accepted.\n");

        // Handle votes from the client
        handle_vote(client_socket);

        // Close the client socket after handling the vote
        close(client_socket);
    }

    //will never be reached, see cleanup function
    return 0;
}

void show_help() {
    printf("Usage: ./server -l 0 [-h]\n");
    printf("Program will not close on its' own. Use Ctrl+C to close the program properly\n");
    printf("Options:\n");
    printf( "  -l <load>  setting this to 0 will make it so the program doesn't load old votes\n"     
            "             any other number will load old votes which is also the default behavior\n\n");
    printf("  -h         Show this help information\n");
}

void free_key(gpointer key) {
    free(key);
}

void free_value(gpointer value) {
    free(value);
}

void cleanup() {
    close(shared_data.server_fd);
    save_votes(&shared_data.voters, &shared_data.results);
    g_hash_table_destroy(shared_data.voters);
    g_hash_table_destroy(shared_data.results);
}


// Signal handler for SIGINT (Ctrl+C)
void handle_sigint(int sig) {
    printf("Exiting Program. Cleaning up...\n");
    exit(0);
}

// Signal handler for SIGTSTP (Ctrl+Z)
void handle_sigtstp(int sig) {
    printf("Caught signal %d (Ctrl+Z). Ignoring. (Ctrl+C) to exit\n", sig);
}

void track_votes(char* candidate) {
    gpointer count_ptr = g_hash_table_lookup(shared_data.results, candidate);
    int* count;

    if (count_ptr) {
        // Correctly dereference and increment the vote count
        count = (int*)count_ptr;
        (*count)++;  // Increment the vote count
        printf("Updated count for %s: %d\n", candidate, *count);
    } else {
        // If the candidate is not found, initialize it with 1 vote
        count = malloc(sizeof(int));
        *count = 1;  // Initialize the count with 1 vote
        g_hash_table_insert(shared_data.results, strdup(candidate), count); // Use strdup to copy the candidate string
        printf("Initialized count for %s: %d\n", candidate, *count);
    }

    printf("COUNT for %s: %d\n", candidate, *count);
}


// Tally vote by checking if the PID is already in the hash table
int tally_vote(int* vote_total, int* max_voters, GHashTable* voters, pid_t pid, char* candidate) {
    pid_t *pid_key = malloc(sizeof(pid_t));
    *pid_key = pid;

    if (!g_hash_table_contains(voters, (void*)pid_key)) {
        char* candidate_copy = strdup(candidate); // Copy the candidate string
        g_hash_table_insert(voters, (void*)pid_key, (void*)candidate_copy);
        (*vote_total)++;
        track_votes(candidate); //add to candidates count
        printf("NEW VOTE: PID %d; Candidate: %s; VOTE TOTAL: %d\n", pid, candidate, *vote_total);
    } else {
        printf("VOTER FRAUD DETECTED: PID %d\n", pid);
        free(pid_key);  // Free the key since it's not used
    }
    return 1;  // Successfully added the vote
}

void handle_vote(int client_socket) {
    char buffer[BUFFER_SIZE];
    int read_size;

    memset(buffer, 0, sizeof(buffer)); 

    // Receive the message (PID and vote)
    while ((read_size = read(client_socket, buffer, BUFFER_SIZE)) > 0) {
        buffer[read_size] = '\0';
        printf("Received: %s\n", buffer);

        // Parse the PID and vote (assuming "PID: <pid>, Vote: <vote>")
        int pid;
        char vote[BUFFER_SIZE];
        sscanf(buffer, "PID: %d, Vote: %s ;", &pid, vote);
        printf("Received vote from PID: %d\n", pid);
        printf("Vote: %s\n", vote);
        tally_vote(&shared_data.vote_total, &shared_data.max_voters, shared_data.voters, pid, vote);
    }

    if (read_size == 0) {
        printf("Client disconnected.\n");
    } else if (read_size == -1) {
        perror("recv failed");
    }
}
