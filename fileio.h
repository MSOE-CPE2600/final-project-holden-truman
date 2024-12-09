/**********************************
* CPE 2600 121 Lab 13: Final Project
* Filename: fileio.h
* Description: A heafer file that contains method declarations
* for fileio.c
*
* Author: Holden Truman
* Date 12/9/2024
***********************************/

#ifndef FILEIO_H
#define FILIO_H

void save_singular_vote(gpointer key, gpointer value, gpointer user_data);
void save_candidate_vote(gpointer key, gpointer value, gpointer user_data);
int save_votes(GHashTable** voters, GHashTable** results);
int load_votes(int* vote_total, int* max_voters, GHashTable** voters, GHashTable** results;);

#endif