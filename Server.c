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

// wait time for the server until connection expires
#define WAIT 60

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
	socklen_t clilen, struct sockaddr_in cliaddr, int roomsOpened);

// opening a memory segment for the game room
int openSharedMem(int roomsOpened, Inventory *inv, int **data);

// closing the room's shared memory segment
void closeSharedMem(int shmid);

// handler for the alarm function
void catch_alarm(int signo) {
	(void) signo;

	// deactivating the alarm
	signal(SIGALRM, SIG_IGN);

	// inforiming the server user that this connection timed out
	printf("Connection with pid %d timed out, exiting ... \n\n", getpid());

	// exiting ...
	exit(1);
}

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

	// taking data from the inventory file to a struct for easy management
	if ( readInventory(set.inventory, &inv) ) {
		perror("Inventory problem");
		return -1;
	}

	// not allowing duplicates in the server's inventory
	if (checkForDuplicates(inv)) {
		perror("The game's inventory is not allowed to have duplicate entries");
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
	// setting our signal handlers
	signal(SIGCHLD, catch_sig);
	signal(SIGINT, catch_int);
	signal(SIGALRM, catch_alarm);


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

	// room counter
	int roomsOpened = 0;

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
			++roomsOpened;		// about to open a new room
			childpid = fork();	// well ... fork
		}

		if (childpid == 0) {	// checking if it is the child process	
			needroom = 0;		// only the parent server can create rooms, avoiding trouble	
			openGameRoom(fd, s, inv, connfd, listenfd, clilen, cliaddr, roomsOpened);
		} else {
			// Printing the parent pid
			printf("\n| Main Server pid: %d |\n\n", getppid());
			printf("* Game room opened ... \n\n");

			// waiting until we need a new room
			if (read(fd[0], &needroom, sizeof(needroom)) < 0) {
				perror("Couldn't read from the game server");
				exit(1);		
			}
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
 * length), the client address struct and the room counter
 *
 */
void openGameRoom(int *fd, Settings *s, Inventory *inv, int connfd, int listenfd, 
	socklen_t clilen, struct sockaddr_in cliaddr, int roomsOpened) {
	
	// semaphore initialization
	// sem_t *my_sem;	// declaring a semaphore variable
	
	// attemting to open the semaphore 
	// my_sem = sem_open("semsem", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 1);

	// // checking if the semahore opened
	// if (my_sem == SEM_FAILED) {	
	// 	printf("Could not open semaphore!\n");
	// 	exit(1);
	// }

	// pid for the server process that will handle the player
	pid_t newpid = -1;

	// room flag
	int needroom = 0;

	// player vars
	int playerCount = 0;	// counting the players
	char plStr[pSize];		// player's inventory in chars
	Inventory plInv;		// player's inventory in our struct
	char *name;				// player's name
	char response[LINE_LEN];// response to the player
	int status = 0;			// request status (valid/invalid)

	// pipe between the gameroom and the server-client
	int pl[2];
	pipe(pl);	// declaring the pl array as a pipe

	// share memory vars
	int *qData = NULL;		// pointer to our shared memory data
	int shmid = -1;			// shared memory id


	// printing the room's pid
	printf("\n| Opened a game room with pid: %d |\n\n", getpid());

	// opening a room specific shared memory
	shmid = openSharedMem(roomsOpened, inv, &qData);

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

			// waiting for the player to send us his inventory
			if (read(connfd, plStr, sizeof(plStr)) < 0) {
				perror("Error reading the player's inventory");
				exit(1);
			}

			// parsing the string we received to our Inventory format
			parseStrIntoInv(&name, plStr, &plInv);

			// attempting to give items to the player
			status = subInventories(inv, plInv, qData, s->quota);

			// checking if the subtraction took place
			if (status) {
				++playerCount;	// accepted a player

				// sending the ok message
				strcpy(response, "OK\n");	
			} else {
				// sending a problem message
				strcpy(response, "Encoutered a problem");
			}

			// writing the response back to the player
			if (write(connfd, response, sizeof(response)) < 0) {
				perror("Couldn't respond to the player");
				exit(1);
			}

			// writing the player counter to the current game server
			if (write(pl[1], &playerCount, sizeof(playerCount)) < 0) {
				perror("Couldn't write to the pipe");
				exit(1);
			}

			close(pl[1]);	// we no longer need this pipe's endpoint
/*- -------------- testing -------------- -*/

			// alarm(WAIT);
/*- -------------- testing -------------- -*/

			// exiting this process
			exit(0);
		} else {
			close(connfd);	// closing the connection socket (child process is handling it)
			
			// waiting for the child to update us on the player counter
			if (read(pl[0], &playerCount, sizeof(playerCount)) < 0) {
				perror("Couldn't read from the pipe");
				exit(1);				
			}

			// if we reach the limit set then we stop listening
			// and ask the parent server for a new room
			if (playerCount == s->players) {
				close(listenfd);	// closing the listening socket for this room
				
				// we filled the room and no longer need the segment
				closeSharedMem(shmid);

				// printing a message from the server's point to
				// inform that this room is full
				printf("\n| Room %d: Full |\n\n", getpid());
				
				// closing from this side
				close(pl[0]);

				// raising the room flag and writing to the parent
				needroom = 1;
				if (write(fd[1], &needroom, sizeof(needroom)) < 0) {
					perror("Couldn't write to the main server");
					exit(1);		
				}

				// breaking out of the loop
				break;
			}
		} // if
	} // while

/*- -------------- testing -------------- -*/
	printf("\n| Room %d: Game in progress ...|\n\n", getpid());
	sleep(30);


	// do something here ...


	printf("\n| Room %d: Game in ended ...|\n\n", getpid());
/*- -------------- testing -------------- -*/
	
	// exiting with success status after closing up the room
	exit(0);
}

/*- ---------------------------------------------------------------- -*/
/**
 * @brief Creates a shared memory segment the size of our inventory
 * struct, with a key that depends on the rooms opened. This way we
 * can achive a dynamic shared memory allocator so that each room
 * gets its own segment to write on
 *
 * @param Takes in the number of rooms opened, the inventory struct 
 * and a pointer to the beginning of the segment
 *
 */
int openSharedMem(int roomsOpened, Inventory *inv, int **data) {
	int shmid;
	key_t key;
	size_t shmsize = sizeof(int)*(inv->count);
	int *start;
	int i;

	// naming our shared memory
	key = SHM_KEY + roomsOpened;

	// creating the memory segment
	if ((shmid = shmget(key, shmsize, IPC_CREAT | 0666)) < 0) {
		perror("shmget error");
		exit(1);
	}


	// attaching segment to our data space
	if ((*data = shmat(shmid, NULL, 0)) == (int *)-1) {
		perror("shmat error");
		exit(1);
	}

	// adding data to the segment
	start = *data;
	
	// we are only attaching the quantity data to save some space
	// since we already have the rest of the data in our struct
	for (i=0; i<inv->count; ++i) {
		*start = inv->quantity[i];
		++start;
	}

	// returning the id so that we can remove the shared memory later on
	return shmid;
}

/*- ---------------------------------------------------------------- -*/
/**
 * @brief Releases the shared memory segment we reserved
 *
 * @param Takes in the shared memory id
 *
 */
void closeSharedMem(int shmid) {
	key_t ret;

	if ((ret = shmctl(shmid, IPC_RMID, (struct shmid_ds *) NULL)) == -1) {
		perror("shmctl error");
		exit(1);
	}
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
