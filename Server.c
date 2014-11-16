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

#include <stdio.h>		// standard i/o
#include <unistd.h>		// declaration of numerous functions and macros
#include <sys/socket.h> // socket definitions
#include <sys/types.h> 	// system data types
#include <sys/un.h> 	// Unix domain sockets
#include <errno.h>		// EINTR variable
#include <sys/wait.h>	// waitpid system call

#include "ServerBackend.h"	// server backend, which handles the game

// size for the queue
#define LISTENQ 30

// socket name and path
#define SRV_PATH "./gameserver.str"

// functions
void catch_sig(int signo);	// signal handler for zombie processes

/*- ---------------------------------------------------------------- -*/

int main(int argc, char **argv) {
	Settings set;	// Game settings

	// getting parameters to set up the server according to the user
	// initSettings(argc, argv, &set);

	/*- ------------------- testing ------------------- */
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
	bzero(&servaddr, sizeof(servaddr) );	// zero servaddr fields
	servaddr.sun_family = AF_LOCAL; 		// setting the socket type to local
	strcpy(servaddr.sun_path, SRV_PATH);    // defining the socket's name

	// creating the file for the socket and registering it
	bind(listenfd, (struct sockaddr*) &servaddr, sizeof(servaddr));

	// creating the request queue
	listen(listenfd, LISTENQ); 

	// infinite loop, here we handle requests
	for (;;) {
		clilen = sizeof(cliaddr);	// got address length

		// get next request and remove it from queue afterwards
		connfd = accept(listenfd, (struct sockaddr *) &cliaddr, &clilen);

		if (connfd < 0) {
			if ( errno == EINTR ) /* Something interrupted us. */
				continue;	/* Back to for()... */
			else {
				fprintf( stderr, "Accept Error\n" );
				exit( 0 );
			}
		}

		childpid = fork(); /* Spawn a child. */

		if ( childpid == 0 ) {	/* Child process. */
			close( listenfd );	/* Close listening socket. */
			while ( read( connfd, &line, sizeof( char ) ) > 0 )
			putchar( line );
			putchar( '\n' );

			exit(0); /* Terminate child process. */
		}

		close(connfd);	/* Parent closes connected socket */
	}
	/*- ------------------- testing ------------------- */


	return 0;
}

/*- ---------------------------------------------------------------- -*/

void catch_sig(int signo) {
	pid_t pid;
	int stat;

	// ensuring that all children processes have died
	while((pid = waitpid(-1, &stat, WNOHANG)) > 0) {
		printf("Child %d terminated.\n", pid);
	}
}

/*- ---------------------------------------------------------------- -*/