/**
 * @file Server.c
 * @author Vaios Papaspyros
 * 
 * @brief Implemenation of the game's server
 *
 * Shared memory allocation, creation of sockets and all the backend
 * functions that are expected from a server are implemented in
 * this file
 *
 */

#include "SharedHeader.h"
#include "ServerBackend.h"	// server backend, which handles the game

// size for the queue
#define LISTENQ 30

// keeping track of the rooms created
int room_count = 0;

// functions
void catch_sig(int signo);	// signal handler for zombie processes

/*- ---------------------------------------------------------------- -*/

int main(int argc, char **argv) {
	Settings set;	// Game settings
	Inventory inv;	// Game inventory as requested

	// getting parameters to set up the server according to the user
	initSettings(argc, argv, &set);

	readInventory(set.inventory, &inv);

	int b = 0;
	for (b=0; b<inv.count; b++){
		printf("----> %s \t %d\n", inv.items[b], inv.quantity[b]);
	}

	int listenfd, connfd; // socket descriptors
	char line;			  // used to output chars to stdout
	pid_t childpid;		  // storing the child process pid
	socklen_t clilen;	  // client address length


	// structs for client/server addresses
	struct sockaddr_un cliaddr, servaddr; 

	// handling signals to avoid zombie processes
	signal(SIGCHLD, catch_sig);

	// server's endpoint
	listenfd = socket(AF_LOCAL, SOCK_STREAM, 0); 

	// removes previously created files
	unlink(SRV_PATH);

	// initializing connection variables
	bzero(&servaddr, sizeof(servaddr));		// zero servaddr fields
	servaddr.sun_family = AF_LOCAL; 		// setting the socket type to local
	strcpy(servaddr.sun_path, SRV_PATH);    // defining the socket's name

	// creating the file for the socket and registering it
	bind(listenfd, (struct sockaddr*) &servaddr, sizeof(servaddr));

	// creating the request queue
	listen(listenfd, LISTENQ); 

/*- ------------------- testing ------------------- */


	// infinite loop, here we handle requests
	for (;;) {
		clilen = sizeof(cliaddr);	// got address length

		// get next request and remove it from queue afterwards
		connfd = accept(listenfd, (struct sockaddr *) &cliaddr, &clilen);

		if (connfd < 0) {
			if (errno == EINTR) // something interrupted us
				continue;
			else {
				fprintf(stderr, "Got an error while trying to connect \n");
				exit(1);
			}
		}

		// forking the process and creating a child
		printf("----> %d \n", ++room_count);

		childpid = fork();

		if (childpid == 0) {	// checking if it is the child process
			close(listenfd);	// closing up the listening socket

			while(read(connfd, &line, sizeof(char)) > 0) {
				putchar(line);
			}

			putchar('\n');


			// ----> here we should check tha validity of our items
			write(connfd, &room_count, sizeof(int));

			exit(1); 	// terminating the process
		}

		close(connfd);	// closing the connected socket
	}
/*- ------------------- testing ------------------- */


	return 0;
}

/*- ---------------------------------------------------------------- -*/
/**
 * @brief Handles the various signal codes
 *
 * @param Takes in the signal int code
 *
 */
void catch_sig(int signo) {
	pid_t pid;
	int stat;

	// ensuring that all children processes have died
	while((pid = waitpid(-1, &stat, WNOHANG)) > 0) {
		printf("Child %d terminated.\n", pid);
	}
}

/*- ---------------------------------------------------------------- -*/