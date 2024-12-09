#ifndef FILEIO_H
#define FILIO_H

void save_singular_vote(gpointer key, gpointer value, gpointer user_data);
void save_candidate_vote(gpointer key, gpointer value, gpointer user_data);
int save_votes(GHashTable** voters, GHashTable** results);
int load_votes(int* vote_total, int* max_voters, GHashTable** voters, GHashTable** results;);

#endif