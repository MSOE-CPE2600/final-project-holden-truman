#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include "fileio.h"

#include "shared_data.h"
#include "server.h"

void save_candidate_vote(gpointer key, gpointer value, gpointer user_data) {
    // Assuming keys are of type pid_t and values are strings
    char* candidate = (char*) key;
    int votes = *(int*)value;
    
    // Print the key-value pair to the file provided in user_data
    FILE* results_file = (FILE*) user_data;
    printf("Saving %s %d\n", candidate, votes);
    fprintf(results_file, "%s %d\n", candidate, votes); //maybe add a comma here
}

void save_singular_vote(gpointer key, gpointer value, gpointer user_data) {
    // Assuming keys are of type pid_t and values are strings
    pid_t pid = *(pid_t*)key;
    char *candidate = (char*) value;
    
    // Print the key-value pair to the file provided in user_data
    FILE *voters_file = (FILE *)user_data;
    fprintf(voters_file, "%d, %s\n", pid, candidate);
}

int save_votes(GHashTable** voters, GHashTable** results) {
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
    int hash_vote_count = g_hash_table_size(*voters);
    // Write vote total to the num_file in CSV format
    fprintf(num_file, "Vote Total\n"); // Column header
    fprintf(num_file, "%d\n", hash_vote_count);  // Write the vote total

    // Write all PIDs in the hash table to voters_file in CSV format
    fprintf(voters_file, "PID\n");  // Column header for the PIDs
    g_hash_table_foreach(*voters, save_singular_vote, voters_file);
    
    // Write Results for each candidate in results_file in CSV format
    fprintf(results_file, "RESULTS\n");  // Column header for the PIDs
    g_hash_table_foreach(*results, save_candidate_vote, results_file);
    

    // Close the files
    fclose(num_file);
    fclose(results_file);
    fclose(voters_file);

    return 0;
}

int load_votes(int* vote_total, int* max_voters, GHashTable** voters, GHashTable** results) {
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
    *voters = g_hash_table_new_full(g_int_hash, g_int_equal, free_key, free_value);
    *results = g_hash_table_new_full(g_str_hash, g_str_equal, free_key, free_value);


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

        g_hash_table_insert(*voters, (void*)pid_key, (void*)candidate_copy);
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

        g_hash_table_insert(*results, (void*)candidate_copy, (void*)vote_value);
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