/**********************************
* CPE 2600 121 Lab 13: Final Project
* Filename: votingmachine.c
* Description: A program to send votes to the server
*
* Author: Holden Truman
* Date 12/9/2024
***********************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <string.h>

#define PORT 8080
#define SERVER_IP "127.0.0.1"
#define BUFFER_SIZE 1024

void show_help();
void send_vote(int socket, const char *vote, pid_t pid);
void run_socket(char* vote);

int main(int argc, char* argv[]) {
    char* real_vote = "NOVOTE";
    char* fake_vote = "Joe";

    char arg;

    while((arg = getopt(argc,argv,"v:h"))!=-1) {
		switch(arg) 
		{
			case 'v':
				real_vote = optarg;
                //strcat(real_vote, "\0");
				break;
            case 'h':
				show_help();
				exit(0);
				break;
		}
	}

    run_socket(real_vote);
    run_socket(fake_vote);
    run_socket(real_vote);
    
    return 0;
}

void show_help() {
    printf("Usage: ./votingmachine -v <vote> [-h]\n");
    printf("Options:\n");
    printf("  -v <vote>  Specify the real vote\n");
    printf("  -h         Show help information\n");
}

void send_vote(int socket, const char *vote, pid_t pid) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    snprintf(buffer, BUFFER_SIZE, "PID: %d, Vote: %s ;", pid, vote);
    if (send(socket, buffer, strlen(buffer), 0) < 0) {
        perror("Send failed");
        return;
    }
    printf("Vote sent: %s\n", buffer);
}

void run_socket(char* vote) {
    int socket_fd;
    struct sockaddr_in server;
    
    // Get and print the PID
    pid_t pid = getpid();
    printf("Voting Machine PID: %d\n", pid);

    // Create socket
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        perror("Could not create socket");
        exit(1);
    }
    printf("Socket created.\n");

    server.sin_addr.s_addr = inet_addr(SERVER_IP);
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);

    // Connect to remote server
    if (connect(socket_fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("Connect failed");
        exit(1);
    }
    printf("Connected to server.\n");

    // Send a vote with PID
    send_vote(socket_fd, vote, pid);

    close(socket_fd);
}
