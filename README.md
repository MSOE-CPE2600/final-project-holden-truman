# System Programming Lab 13 Final Project
## Overview of Implementation & Purpose
This project provides a voting machine that sends a user's command line argument [-v <vote>] to the a server which tracks votes. The program tries to encapsulate desired
features of real voting machines, such as fraud detection, which is done with tracking PIDS. In order to do this the glib HashTable implementation, which has not been used
in class previously, was used to store users' votes along with their PID, and due to the HashTable's constant contains runtime this process is very efficient 
(rather than having to parse through potentially millions of votes in a real scenario). The server runs a socket, which is how different voting machines can connect to it. A
voting machine sends its' PID and vote to the socket, which are then processed by the server.

The server can be stopped with Ctrl+C, and when it shutdowns it automatically cleans up and saves the results to .csv files. The cleanup function ensures that no memory 
leaks occur in the server program, and no memory leaks are possible in the voting machine program. In addition to saving the results the server program's default behavior is to load votes from existing runs, but this can overridden with the argument -l 0.

*See the report pdf in the repository for screenshots of the programming running.*

## Sample Inputs

Start Server With Fresh Voting Data: ./server -l 0

Start Server With Old Voting Data: ./server or ./server -l [!=0]


Vote: ./votingmachine -v <vote>

Get Help With Vote: ./votingmachine -h


## Package Dependencies Installation
In order to run the project you'll need glib, which can be installed by running

$ sudo apt-get install libglib2.0-dev

You can check that it can be accessed with

$ pkg-config --cflags --libs glib-2.0





