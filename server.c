#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <glib.h> // for hash table
#include <sys/types.h>  // For pid_t

#define PORT 12345
#define MAX_VOTING_MACHINES 10
#define DEFAULT_VOTERS 2000

// Define a structure to hold data to export when exiting
typedef struct {
    GHashTable* voters; // Hash table for storing voters by PID
    int vote_total;
    int max_voters;
} SharedData;

// Global variable to hold the structure
SharedData shared_data;

int save_votes(int* vote_total);
void print_all_keys(GHashTable* voters, int* vote_total);

// Signal handler for SIGINT (Ctrl+C)
void handle_sigint(int sig) {
    printf("Caught signal %d (Ctrl+C). Storing Voter Info...\n", sig);
    int* l = (int*) 8;
    save_votes(l);
    exit(0);
}

// Signal handler for SIGTSTP (Ctrl+Z)
void handle_sigtstp(int sig) {
    printf("Caught signal %d (Ctrl+Z). Ignoring...\n", sig);
}

// Tally vote by checking if the PID is already in the hash table
int tally_vote(int* vote_total, int* max_voters, GHashTable* voters, pid_t pid) {
    printf("Before checking PID in hash table: %d\n", pid);
    printf("VOTE TOTAL1: %d\n", *vote_total);
    pid_t *pid_key = malloc(sizeof(pid_t));
    pid_t *pid_value = malloc(sizeof(pid_t));
    *pid_key = pid;
    *pid_value = pid;
    if (!g_hash_table_contains(voters, (void*) pid_key)) {
        printf("ADDING: %d\n", pid);
        g_hash_table_insert(voters, (void*) pid_key, (void*) pid_value);
        print_all_keys(shared_data.voters, vote_total);
        printf("VOTE TOTAL2: %d\n", *vote_total);
        (*vote_total) = (*vote_total) + 1;
    } else {
        printf("VOTER FRAUD DETECTED: PID %d\n", pid);
        free(pid_key);  // Free the key since it's not used
        free(pid_value); // Free the value since it's not used
    }
    printf("VOTE TOTAL3: %d\n", *vote_total);
    return 1;  // Successfully added the vote
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
    printf("WRITING THIS %d\n", hash_vote_count);
    fprintf(num_file, "%d\n", hash_vote_count);  // Write the vote total

    // Write all PIDs in the hash table to voters_file in CSV format
    fprintf(voters_file, "PID\n");  // Column header for the PIDs
    gpointer* keys = g_hash_table_get_keys_as_array(shared_data.voters, vote_total);

    // Ensure keys is valid and write them to the file
    if (keys != NULL) {
        for (int i = 0; i < hash_vote_count; i++) {
            pid_t pid = *((pid_t*) keys[i]);  // Dereference the pointer to get the actual PID value
            printf("Saving %d\n", pid);
            fprintf(voters_file, "%d\n", pid);  // Write the PID to the CSV file
        }
        free(keys);  // Free the list of keys, but not the actual keys
    } else {
        printf("Error: Could not retrieve keys from hash table.\n");
    }

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
    printf("Loaded vote total: %d\n", *vote_total);

    // Initialize the hash table for voters
    shared_data.voters = g_hash_table_new(g_int_hash, g_int_equal);  // Create a new hash table

    // Read the PIDs from voters_file (skip the header)
    fgets(header, sizeof(header), voters_file);  // Skip the header
    pid_t* pid = malloc((size_t) *vote_total * sizeof(pid_t));
    int i = 0;
    while (fscanf(voters_file, "%d", &pid[i]) == 1) {
        // Insert each PID into the hash table
        pid_t *pid_key = malloc(sizeof(pid_t));
        pid_t *pid_value = malloc(sizeof(pid_t));
        *pid_key = pid[i];
        *pid_value = pid[i];
        g_hash_table_insert(shared_data.voters, (void*) pid_key, (void*) pid_value);
        printf("Loaded PID: %d\n", pid[i]);  // Debug print to verify PID loading
        i++;
    }

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

int main(int argc, char* argv[]) {
    printf("Starting program...\n");

    signal(SIGINT, handle_sigint);   // Handle Ctrl+C
    signal(SIGTSTP, handle_sigtstp); // Handle Ctrl+Z

    shared_data.vote_total = 0;  // Initialize vote_total to 0
    shared_data.max_voters = (int) DEFAULT_VOTERS; // Default max voters capacity

    shared_data.voters = g_hash_table_new(g_int_hash, g_int_equal);  // Initialize the hash table

    int load_old_votes = load_votes(&shared_data.vote_total, &shared_data.max_voters); // Load previous votes from file
    printf("Loaded %d voters\n", shared_data.vote_total);

    printf("All PIDs\n");
    print_all_keys(shared_data.voters, &shared_data.vote_total); // Fixed call to print_all_keys()

    pid_t pid = 8;
    tally_vote(&shared_data.vote_total, &shared_data.max_voters, shared_data.voters, pid);
    //tally_vote(&shared_data.vote_total, &shared_data.max_voters, shared_data.voters, 7);
    tally_vote(&shared_data.vote_total, &shared_data.max_voters, shared_data.voters, 6);
    tally_vote(&shared_data.vote_total, &shared_data.max_voters, shared_data.voters, 5);

    printf("All PIDs\n");
    print_all_keys(shared_data.voters, &shared_data.vote_total); // Fixed call to print_all_keys()
    printf("VOTE TOTAL: %d\n", shared_data.vote_total);
    save_votes(&shared_data.vote_total);  // Save votes back to files

    // Free allocated memory
    g_hash_table_destroy(shared_data.voters);  //
}
