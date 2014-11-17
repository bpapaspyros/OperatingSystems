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




// counter for the servers created
 int counter = 0;

// functions
void catch_sig(int signo);	// signal handler for zombie processes

/*- ---------------------------------------------------------------- -*/

int main(int argc, char **argv) {
	Settings set;	// Game settings

	// getting parameters to set up the server according to the user
	// initSettings(argc, argv, &set);

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
	// Room* r = addRoom(&set);


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
		childpid = fork();

		if (childpid == 0) {	// checking if it is the child process
			close(listenfd);	// closing up the listening socket
			
			// if (read(connfd, &line, sizeof(char)) > 0) {
				// if ()
			// }

			while(read(connfd, &line, sizeof(char)) > 0) {
				putchar(line);
			}

			putchar('\n');

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