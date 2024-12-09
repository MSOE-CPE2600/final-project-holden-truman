#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include <glib.h>

// Structure for global variables to make cleanup simpler
typedef struct {
    GHashTable* voters;  // Hash table for storing voters by PID
    GHashTable* results; // Hash table for storing voting results
    int vote_total;      // Total number of votes
    int max_voters;      // Maximum number of voters
    int server_fd;       // Server file descriptor
} SharedData;

#endif // SHARED_DATA_H