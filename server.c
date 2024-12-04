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
    int vote_total; // So I can clean things up
    int max_voters; // So I can clean things up
} SharedData;

// Global variable to hold the structure
SharedData shared_data;

int save_votes(int* vote_total);
void print_all_keys(GHashTable* voters, int* vote_total);

// Signal handler for SIGINT (Ctrl+C)
void handle_sigint(int sig) {
    printf("Caught signal %d (Ctrl+C). Storing Voter Info...\n", sig);
    g_hash_table_destroy(shared_data.voters);
    save_votes(&shared_data.vote_total);
    exit(0);
}

// Signal handler for SIGTSTP (Ctrl+Z)
void handle_sigtstp(int sig) {
    printf("Caught signal %d (Ctrl+Z). Ignoring...\n", sig);
}

// Tally vote by checking if the PID is already in the hash table
int tally_vote(int* vote_total, int* max_voters, GHashTable* voters, pid_t pid, char* candidate) {
    pid_t *pid_key = malloc(sizeof(pid_t));
    *pid_key = pid;

    if (!g_hash_table_contains(voters, (void*)pid_key)) {
        char* candidate_copy = strdup(candidate); // Copy the candidate string
        g_hash_table_insert(voters, (void*)pid_key, (void*)candidate_copy);
        (*vote_total)++;
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
    fprintf(voters_file, "%d %s\n", pid, candidate);
}

int save_votes(int* vote_total) {
    printf("Saving votes...\n");

    // Open the CSV files for writing
    FILE* voters_file = fopen("voterinfo.csv", "w"); // Open in write mode (text mode)
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

    // Close the files
    fclose(num_file);
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

    // Read the vote total from num_file (skip the header)
    char header[100];  // Buffer to read the header line
    fgets(header, sizeof(header), num_file);  // Skip the header
    if (fscanf(num_file, "%d", vote_total) != 1) {
        printf("Error reading vote total from file\n");
        return -1;
    }
    *max_voters = *vote_total * 2;  // Allocate extra capacity

    // Initialize the hash table for voters
    shared_data.voters = g_hash_table_new(g_int_hash, g_int_equal);  // Create a new hash table

    // Read the PIDs and candidates from voters_file (skip the header)
    fgets(header, sizeof(header), voters_file);  // Skip the header
    pid_t* pid = malloc(sizeof(pid_t) * (*vote_total));
    char* candidate = malloc(20 * sizeof(char)); // Allocate memory for candidate strings
    int i = 0;
    while (fscanf(voters_file, "%d %s", &pid[i], candidate) == 2) {
        // Insert each PID and candidate into the hash table
        pid_t* pid_key = malloc(sizeof(pid_t));
        *pid_key = pid[i];

        char* candidate_copy = strdup(candidate); // Make a copy of the candidate name

        g_hash_table_insert(shared_data.voters, (void*)pid_key, (void*)candidate_copy);
        i++;
    }

    free(pid);
    free(candidate);
    fclose(num_file);
    fclose(voters_file);

    return 0;
}

void print_all_keys(GHashTable* voters, int* vote_total) {
    printf("Printing all keys...\n");
    gpointer* keys = g_hash_table_get_keys_as_array(shared_data.voters, (guint*)vote_total);  // Get all keys in the hash table

    // Check if keys are loaded correctly
    if (keys == NULL) {
        printf("No keys in the hash table!\n");
        return;
    }

    // Iterate through the list of keys and print them
    for(int i = 0; i < *vote_total; i++) {
        pid_t pid = *((pid_t*)(keys[i]));  // Dereference the pointer to get the actual PID value
        printf("PID: %d\n", pid);
    }
    printf("\n");
    free(keys);  // Free the list of keys, but not the actual keys
}

void handle_vote(int client_socket) {
    char buffer[BUFFER_SIZE];
    int read_size;

    // Receive the message (PID and vote)
    while ((read_size = read(client_socket, buffer, BUFFER_SIZE)) > 0) {
        buffer[read_size] = '\0';
        printf("Received: %s\n", buffer);

        // Parse the PID and vote (assuming "PID: <pid>, Vote: <vote>")
        int pid;
        char vote[BUFFER_SIZE];
        sscanf(buffer, "PID: %d, Vote: %s", &pid, vote);
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

    shared_data.voters = g_hash_table_new(g_int_hash, g_int_equal);  // Initialize the hash table

    if (load_old) {
        int load_old_votes = load_votes(&shared_data.vote_total, &shared_data.max_voters); // Load previous votes from file
        printf("Loaded %d voters\n\n", shared_data.vote_total);
    } else {
        printf("Storing new data, overwriting old\n");
    }

    //pid_t pid = 8;
    //tally_vote(&shared_data.vote_total, &shared_data.max_voters, shared_data.voters, pid, "Name");
    //tally_vote(&shared_data.vote_total, &shared_data.max_voters, shared_data.voters, 2, "Bartholomew");
    
    int server_fd, client_socket, c;
    struct sockaddr_in server, client;

    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Could not create socket");
        return 1;
    }
    printf("Socket created.\n");

    // Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(PORT);

    // Bind
    if (bind(server_fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("Bind failed");
        return 1;
    }
    printf("Bind done.\n");

    // Listen
    listen(server_fd, 3);

    // Accept an incoming connection
    printf("Waiting for incoming connections...\n");
    c = sizeof(struct sockaddr_in);
    client_socket = accept(server_fd, (struct sockaddr *)&client, (socklen_t *)&c);
    if (client_socket < 0) {
        perror("Accept failed");
        return 1;
    }
    printf("Connection accepted.\n");

    // Handle the vote
    handle_vote(client_socket);

    close(client_socket);
    close(server_fd);

    save_votes(&shared_data.vote_total);  // Save votes back to files
    // Free allocated memory
    g_hash_table_destroy(shared_data.voters);  //
    return 0;
}
