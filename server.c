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

#define PORT 8080
#define BUFFER_SIZE 1024

#define MAX_VOTING_MACHINES 10
#define DEFAULT_VOTERS 2000

// Define a structure to hold data to export when exiting
typedef struct {
    GHashTable* voters; // Hash table for storing voters by PID
    GHashTable* results;
    int vote_total; // So I can clean things up
    int max_voters; // So I can clean things up
    int server_fd; 
} SharedData;

// Global variable to hold the structure
SharedData shared_data;

int save_votes();

void free_key(gpointer key) {
    free(key);
}

void free_value(gpointer value) {
    free(value);
}

void cleanup() {
    close(shared_data.server_fd);
    save_votes();
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

void save_singular_vote(gpointer key, gpointer value, gpointer user_data) {
    // Assuming keys are of type pid_t and values are strings
    pid_t pid = *(pid_t*)key;
    char *candidate = (char*) value;
    
    // Print the key-value pair to the file provided in user_data
    FILE *voters_file = (FILE *)user_data;
    fprintf(voters_file, "%d, %s\n", pid, candidate);
}

void save_candidate_vote(gpointer key, gpointer value, gpointer user_data) {
    // Assuming keys are of type pid_t and values are strings
    char* candidate = (char*) key;
    int votes = *(int*)value;
    
    // Print the key-value pair to the file provided in user_data
    FILE* results_file = (FILE*) user_data;
    printf("Saving %s %d\n", candidate, votes);
    fprintf(results_file, "%s %d\n", candidate, votes); //maybe add a comma here
}

int save_votes() {
    printf("Saving votes...\n");

    // Open the CSV files for writing
    FILE* voters_file = fopen("voterinfo.csv", "w"); // Open in write mode (text mode)
    if (voters_file == NULL) {
        perror("Error opening file for writing");
        return -1;
    }

    FILE* results_file = fopen("results.csv", "w"); // Open in write mode (text mode)
    if (voters_file == NULL) {
        perror("Error opening file for writing");
        return -1;
    }

    FILE* num_file = fopen("votertotal.csv", "w"); // Open in write mode (text mode)
    if (num_file == NULL) {
        perror("Error opening file for writing");
        return -1;
    }
    int hash_vote_count = g_hash_table_size(shared_data.voters);
    // Write vote total to the num_file in CSV format
    fprintf(num_file, "Vote Total\n"); // Column header
    fprintf(num_file, "%d\n", hash_vote_count);  // Write the vote total

    // Write all PIDs in the hash table to voters_file in CSV format
    fprintf(voters_file, "PID\n");  // Column header for the PIDs
    g_hash_table_foreach(shared_data.voters, save_singular_vote, voters_file);

    //FIXME
    
    // Write Results for each candidate in results_file in CSV format
    fprintf(results_file, "RESULTS\n");  // Column header for the PIDs
    g_hash_table_foreach(shared_data.results, save_candidate_vote, results_file);
    

    // Close the files
    fclose(num_file);
    fclose(results_file);
    fclose(voters_file);

    return 0;
}

int load_votes(int* vote_total, int* max_voters) {
    printf("Loading votes...\n");

    // Open the CSV files for reading
    FILE* voters_file = fopen("voterinfo.csv", "r"); // Open in read mode (text mode)
    if (voters_file == NULL) {
        printf("No existing voter info found, storing new data\n");
        return -1;
    }

    FILE* num_file = fopen("votertotal.csv", "r"); // Open in read mode (text mode)
    if (num_file == NULL) {
        printf("No existing vote total found, storing new data\n");
        return -1;
    }

    FILE* results_file = fopen("results.csv", "r"); // Open in read mode (text mode)
    if (results_file == NULL) {
        printf("No existing results found, storing new data\n");
        return -1;
    }

    // Read the vote total from num_file (skip the header)
    char header[100];  // Buffer to read the header line
    fgets(header, sizeof(header), num_file);  // Skip the header
    if (fscanf(num_file, "%d", vote_total) != 1) {
        printf("Error reading vote total from file\n");
        return -1;
    }
    *max_voters = *vote_total * 2;  // Allocate extra capacity

    // Initialize the hash table for voters
    shared_data.voters = g_hash_table_new_full(g_int_hash, g_int_equal, free_key, free_value);
    shared_data.results = g_hash_table_new_full(g_str_hash, g_str_equal, free_key, free_value);


    // Read the PIDs and candidates from voters_file (skip the header)
    fgets(header, sizeof(header), voters_file);  // Skip the header
    pid_t* pid = malloc(sizeof(pid_t) * (*vote_total));
    char* candidate = malloc(20 * sizeof(char)); // Allocate memory for candidate strings
    int i = 0;
    while (fscanf(voters_file, "%d, %s", &pid[i], candidate) == 2) {
        // Insert each PID and candidate into the hash table
        pid_t* pid_key = malloc(sizeof(pid_t));
        *pid_key = pid[i];

        char* candidate_copy = strdup(candidate); // Make a copy of the candidate name

        g_hash_table_insert(shared_data.voters, (void*)pid_key, (void*)candidate_copy);
        i++;
    }

    // Read the results for each candidate from results file (skip the header)
    fgets(header, sizeof(header), results_file);  // Skip the header
    int* votes = malloc(sizeof(int) * (*vote_total));
    i = 0;
    while (fscanf(results_file, "%s %d", candidate, &votes[i]) == 2) {
        // Insert each PID and candidate into the hash table
        int* vote_value = malloc(sizeof(int));
        *vote_value = votes[i];
        
        char* candidate_copy = strdup(candidate); // Make a copy of the candidate name

        g_hash_table_insert(shared_data.results, (void*)candidate_copy, (void*)vote_value);
        i++;
    }

    free(votes);
    free(pid);
    free(candidate);
    fclose(num_file);
    fclose(voters_file);
    fclose(results_file);

    return 0;
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

int main(int argc, char* argv[]) {
    // Register the cleanup function to be called at exit
    atexit(cleanup);

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
				//show_help();
				exit(1);
				break;
		}
	}
    printf("Load old %d\n", load_old);
    shared_data.vote_total = 0;  // Initialize vote_total to 0
    shared_data.max_voters = (int) DEFAULT_VOTERS; // Default max voters capacity

    if (load_old) {
        if (load_votes(&shared_data.vote_total, &shared_data.max_voters) != 0) {
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

    //pid_t pid = 8;
    //tally_vote(&shared_data.vote_total, &shared_data.max_voters, shared_data.voters, pid, "Name");
    //tally_vote(&shared_data.vote_total, &shared_data.max_voters, shared_data.voters, 2, "Bartholomew");
    
    struct sockaddr_in server, client;

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

    while (1) {
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

        // Handle the vote from the client
        handle_vote(client_socket);

        // Close the client socket after handling the vote
        close(client_socket);
    }

    //close(shared_date.server_fd);

    //save_votes();  // Save votes back to files
    // Free allocated memory
    //g_hash_table_destroy(shared_data.voters);  //
    return 0;
}
