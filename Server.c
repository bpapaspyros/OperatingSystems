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

#include "Inventory.h"
#include "ServerBackend.h"	// server backend, which handles the game

// size for the queue
#define LISTENQ 150

// key for the shared memory segment
// that will hold the gameserver flags
#define SHM_KEY 5623

// catching signals
void catch_sig(int signo);	// signal handler for zombie processes
void catch_int(int signo);	// terminating the server

// initializing the structs and vars for the server
void initSockets(int *listenfd, struct sockaddr_in *servaddr);		

// takes the server "online"
void serverUp(int connfd, int listenfd, struct sockaddr_in cliaddr, 
		socklen_t clilen, pid_t childpid, Inventory *inv, Settings *s);

// opening a game room
void openGameRoom(int *fd, Settings *s, Inventory *inv, int connfd, int listenfd, 
	socklen_t clilen, struct sockaddr_in cliaddr);


/*- ---------------------------------------------------------------- -*/
int main(int argc, char **argv) {
	// game vars 
	Settings set;	// Game settings

	// Game inventory as requested
	Inventory inv;

	// communication vars
	int listenfd, connfd = -1; // socket descriptors
	pid_t childpid = -1; 	   // storing the child process pid
	socklen_t clilen = -1;	   // client address length
		// structs for client/server addresses
	struct sockaddr_in cliaddr, servaddr; 

	// getting parameters to set up the server according to the user
	initSettings(argc, argv, &set);

	// taking data from inventory to a struct for easy management
	if ( readInventory(set.inventory, &inv, 0, NULL) ) {
		perror("Inventory problem");
		return -1;
	}

	// printing the inventory to the user	
	printInventory(inv);

	// initializing sockets and server address
	initSockets(&listenfd, &servaddr);

	// start listening
	serverUp(connfd, listenfd, cliaddr, 
		     clilen, childpid, &inv, &set);

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
	// and handling server termination
	signal(SIGCHLD, catch_sig);
	signal(SIGINT, catch_int);

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
		socklen_t clilen, pid_t childpid, 
		Inventory *inv, Settings *s) {

	// flag to signal us that we need a new room
	int needroom = 1;

	// pipe vars
	int fd[2];	// pipe array
	pipe(fd);	// declaring fd is a pipe


	// infinite loop, here we handle requests
	for (;;) {
		clilen = sizeof(cliaddr);	// got address length

		// forking the process and creating a child
		// if the last room is full or this room is the first
		if (needroom) {
			needroom = 0;		// updating the flag to zero until we need a room
			childpid = fork();	// well ... fork
		}

		if (childpid == 0) {	// checking if it is the child process	
			needroom = 0;		// only the parent server can create rooms, avoiding trouble	
			openGameRoom(fd, s, inv, connfd, listenfd, clilen, cliaddr);
		} else {
			// Printing the parent pid
			printf("\n| Main Server pid: %d |\n\n", getppid());
			printf("* Game room opened ... \n\n");

			// waiting until we need a new room
			read(fd[0], &needroom, sizeof(needroom));
		} // if		
	} // for
}

/*- ---------------------------------------------------------------- -*/
/**
 * @brief Game Room server (a fork of the initial game server) that will
 * handle client requests by forking itself and serving them. When the 
 * room is full we write through a pipe to the parent server to let him
 * know that we need a new room. After all players have exited the room
 * the game room server closes up the room before exiting
 *
 * @param Takes the pipe array, the settings, the servers inventory
 * the connection and listening sockets, the client length (struct 
 * length) and client address struct
 *
 */
void openGameRoom(int *fd, Settings *s, Inventory *inv, int connfd, int listenfd, 
	socklen_t clilen, struct sockaddr_in cliaddr) {
	
	// pid for the server process that will handle the player
	pid_t newpid = -1;

	// room flag
	int needroom = 0;

	// player vars
	int playerCount = 0;	// counting the players

	// pipe between the gameroom and the server-client
	int pl[2];
	pipe(pl);	// declaring the pl array as a pipe


	// printing the room's pid
	printf("\n| Opened a game room with pid: %d |\n\n", getpid());

	for (;;) {			
		// get next request and remove it from queue afterwards
		connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);

		// checking if connection was successfull
		if (connfd < 0) {
			if (errno == EINTR) {
				continue;
			} else { // something interrupted us
				fprintf(stderr, "Got an error while trying to connect \n");
				exit(1);
			}
		}

		// forking the process to serve the request
		newpid = fork();

		// the child process handles the request
		if (newpid == 0) {
			close(listenfd);	// closing up the listening socket

/*- -------------- testing -------------- -*/
			++playerCount;
			write(pl[1], &playerCount, sizeof(playerCount));

			// do shit here
			sleep(10);

			// --playerCount;
			// write(pl[1], &playerCount, sizeof(playerCount));
/*- -------------- testing -------------- -*/


			exit(0);
		} else {
			close(connfd);	// closing the connection socket (child process is handling it)

			// waiting for the child to update us on the player counter
			read(pl[0], &playerCount, sizeof(playerCount));

			// if we reach the limit set then we stop listening
			// and ask the parent server for a new room
			if (playerCount == s->players) {
				close(listenfd);	// closing the listening socket for this room
				
				// printing a message from the server's point to
				// inform that this room is full
				printf("\n| Room %d: Full |\n\n", getpid());
				
				// closing up the pipe on both points
				close(pl[0]);
				close(pl[1]);

				// raising the room flag and writing to the parent
				needroom = 1;
				write(fd[1], &needroom, sizeof(needroom));

				// breaking out of the loop
				break;
			}
		} // if
	} // while

/*- -------------- testing -------------- -*/
	printf("\n| Room %d: Game in progress ...|\n\n", getpid());
	sleep(30);
	printf("\n| Room %d: Game in ended ...|\n\n", getpid());
/*- -------------- testing -------------- -*/

	// exiting with success status after closing up the room
	exit(0);
}

/*- ---------------------------------------------------------------- -*/
/**
 * @brief Handles the sigchld signal
 *
 * @param Takes in the signal int code
 *
 */
void catch_sig(int signo) {
	(void) signo;

	pid_t pid;
	int stat;

	// ensuring that all children processes have died
	while( (pid = waitpid(-1, &stat, WNOHANG)) > 0 ) {
		printf("\n \t Child %d terminated.\n", pid);
	}
}

/*- ---------------------------------------------------------------- -*/
/**
 * @brief Handles the sigint signal
 *
 * @param Takes in the signal int code
 *
 */
void catch_int(int signo) {
	(void) signo;

	// goodbye message
	printf("\n\n\t\t Server Terminated. GoodBye ! \n\n");	

	// exiting
	exit(0);
}

/*- ---------------------------------------------------------------- -*/
