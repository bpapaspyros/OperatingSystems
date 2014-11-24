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
#define LISTENQ 150

// catching signals
void catch_sig(int signo);	// signal handler for zombie processes

// initializing the structs and vars for the server
void initSockets(int *listenfd, struct sockaddr_in *servaddr);		

// takes the server "online"
void serverUp(int connfd, int listenfd, struct sockaddr_in cliaddr, 
		socklen_t clilen, pid_t childpid, GameStatus gs, Inventory inv, Settings s);

/*- ---------------------------------------------------------------- -*/

int main(int argc, char **argv) {
	// game vars 
	Settings set;	// Game settings
	GameStatus gs;	// Game status tracking
	Inventory inv;	// Game inventory as requested

	// communication vars
	int listenfd, connfd; // socket descriptors
	char line;			  // used to output chars to stdout
	pid_t childpid;		  // storing the child process pid
	socklen_t clilen;	  // client address length
		// structs for client/server addresses
	struct sockaddr_in cliaddr, servaddr; 


	// getting parameters to set up the server according to the user
	initSettings(argc, argv, &set, &gs);

	// taking data from inventory to a struct for easy management
	readInventory(set.inventory, &inv);
	
	// printing the inventory to the user
	printInventory(&inv);

	// initializing sockets and server address
	initSockets(&listenfd, &servaddr);

	// start listening
	serverUp(connfd, listenfd, cliaddr, 
		     clilen, childpid, gs, inv, set);

	return 0;
}

/*- ---------------------------------------------------------------- -*/
/**
 * @brief Initializes the server address and assigns a socket and port
 * number to the server so that the client can connect through them
 *
 * @param Takes in the socket number and the server address struct
 * 
 */
void initSockets(int *listenfd, struct sockaddr_in *servaddr) {
	// handling signals to avoid zombie processes
	signal(SIGCHLD, catch_sig);

	// server's endpoint
	*listenfd = socket(AF_INET, SOCK_STREAM, 0); 

	if (*listenfd < 0) {	// checking if socket was opened
		perror("Couldn't open socket");
	}

	// initializing connection variables
	bzero(servaddr, sizeof(*servaddr));		// zero servaddr fields
	servaddr->sin_family = AF_INET; 		// setting the socket type to INET
	servaddr->sin_port = htons(PORT_NO);	// assigning the port number
	servaddr->sin_addr.s_addr = INADDR_ANY;	// contains the port number

	// creating the file for the socket and registering it
	bind(*listenfd, (struct sockaddr*) servaddr, sizeof(*servaddr));

	// creating the request queue
	listen(*listenfd, LISTENQ); 
}

/*- ---------------------------------------------------------------- -*/
/**
 * @brief Main server loop where the server waits for client requests.
 * Here we make the necessary calls to arrange the game rooms and check
 * for the client's request validity
 *
 * @param Takes in the connection and listening sockets, client address
 * struct, client length (struct length), the child pid, game status and
 * inventory structs 
 *
 */
void serverUp(int connfd, int listenfd, struct sockaddr_in cliaddr, 
		socklen_t clilen, pid_t childpid, GameStatus gs, Inventory inv, Settings s) {

	char buffer[20];

	// infinite loop, here we handle requests
	for (;;) {
		clilen = sizeof(cliaddr);	// got address length

		// get next request and remove it from queue afterwards
		connfd = accept(listenfd, (struct sockaddr *) &cliaddr, &clilen);

		if (connfd < 0) {
			if (errno == EINTR) 
				continue;
			else { // something interrupted us
				fprintf(stderr, "Got an error while trying to connect \n");
				exit(1);
			}
		}

		// forking the process and creating a child
		// checking if we need a new game room
		childpid = fork();

/*- ------------------- testing ------------------- */

		

		if (childpid == 0) {	// checking if it is the child process
			close(listenfd);	// closing up the listening socket

			if (read(connfd, buffer, sizeof(buffer)) > 0) {
				printf("%s", buffer);
			}

			putchar('\n');

			// itoa(room_count, buffer);
			sprintf(buffer, "%d", gs.slots);

			// ----> here we should check tha validity of our items
			write(connfd, buffer, sizeof(buffer));

			exit(0); 	// terminating the process
		}

		close(connfd);	// closing the connected socket
	}
/*- ------------------- testing ------------------- */
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