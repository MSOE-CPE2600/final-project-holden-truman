// Function declarations
#ifndef SERVER_H
#define SERVER_H

void free_key(gpointer key);
void free_value(gpointer value);
void cleanup();
void handle_sigint(int sig);
void handle_sigtstp(int sig);
void track_votes(char* candidate);
int tally_vote(int* vote_total, int* max_voters, GHashTable* voters, pid_t pid, char* candidate);
void handle_vote(int client_socket);
void show_help();

#endif