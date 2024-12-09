//gcc votingmachine.c -o votingmachine
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>

#define PORT 8080
#define SERVER_IP "127.0.0.1"
#define BUFFER_SIZE 1024

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

int main() {
    char* vote1 = "Melendez\0";
    char* vote2 = "Joe\0";

    run_socket(vote1);
    run_socket(vote1);
    run_socket(vote2);
    
    return 0;
}
