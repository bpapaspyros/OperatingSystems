/**
 * @file Server.c
 * @author Vaios Papaspyros
 * 
 * @brief Implemenation of the game's server
 *
 * Shared memory allocation, creation of sockets and all the backend
 * functions that are expected from a server are implemented in
 * this file. We handle the players requests and implement a chatting
 * system
 *
 */

#include "Inventory.h"
#include "ServerBackend.h"	// server backend, which handles the game

#include <pthread.h>

	/*- ---- Global Variables & Defining ---- -*/ 
#define LISTENQ 150		// size for the queue
#define WAIT 60			// wait time for the server until connection expires
#define MYERRCODE -5623 // used as error code, funny because it's my student id

sem_t *my_sem = NULL;		// declaring a semaphore variable
int roomsOpened = 0; 		// room counter

pid_t pprocID = MYERRCODE;	// main process's id 
pid_t rprocID = MYERRCODE;	// only game rooms should store their pid here
	/*- ---- Global Variables & Defining ---- -*/ 

	/*- ------- Function declarations ------- -*/ 
void catch_sig(int signo);		// signal handler for zombie processes
void catch_int(int signo);		// terminating the server
void catch_alarm(int signo);	// alarm handling

// initializes the server struct (ports, etc)
void initServer(int *listenfd, struct sockaddr_in *servaddr);	

// gets the main server going	
void serverUp(ServerVars *sv);

// opens a smaller server acting as the game room
void *openGameRoom(void *arg);

// Callback function. Called to initialize a player's serving
void *playerHandler(void *arg);

// opens another server that handles his player's requests
void servePlayer(char **name, PlayerData *pd);

// gets a single player's messages and sends it to the game room server
void chat(int connfd, int *plPipe, char *name);

// game room handles pushing messages to all the players
void pushMessage(int *plPipe, int *sockArray, PlayerData *pd);

// allocates memory for an additional pthread
int roomAlloc(pthread_t **pArray);
	/*- ------- Function declarations ------- -*/ 

/*- ---------------------------------------------------------------- -*/
// 				Function definitions 
/*- ---------------------------------------------------------------- -*/
int main(int argc, char **argv) {
	// compact way to acces usefull vars
	ServerVars sv;

	// structs for server address
	struct sockaddr_in servaddr; 

	// getting parameters to set up the server according to the user
	initSettings(argc, argv, &(sv.s));

	// taking data from the inventory file to a struct for easy management
	if ( readInventory(sv.s.inventory, &(sv.inv)) ) {
		perror("Inventory problem");
		return -1;
	}

	// not allowing duplicates in the server's inventory
	if (checkForDuplicates(sv.inv)) {
		perror("The game's inventory is not allowed to have duplicate entries");
		return -1;
	}

	// printing the inventory to the user	
	printInventory(sv.inv);

	// initializing sockets and server address
	initServer(&(sv.listenfd), &servaddr);

	// start listening
	serverUp(&sv);

	return 0;
}

/*- ---------------------------------------------------------------- -*/
/**
 * @brief Initializes the server address and assigns a socket and port
 * number to the server so that the client can connect through them.
 * We also set our signal handlers and create a semaphore for use 
 * later on
 *
 * @param Takes in the socket number and the server address struct
 * 
 */
void initServer(int *listenfd, struct sockaddr_in *servaddr) {
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
 * @brief Main Server openinng threads (game rooms) when needed 
 *
 * @param Takes a ServerVars * struct that holds all the info about
 * the server address and ports plus the inventory structs
 *
 */
void serverUp(ServerVars *sv) {

	// thread array (one thread per room)
	pthread_t *pArray = NULL;

	// thread creation status
	int tstat = -1;

	// flag to signal us that we need a new room
	int needroom = 1;

	// pipe vars
	pipe(sv->openRoomFlag);

	// storing this process's id
	pprocID = getpid();

	// Printing the parent pid
	printf("\n\n| Main Server pid: %d |\n", pprocID);

	// keeping a backup of the initial quantity values
	int *backupQ = backupQuantity(sv->inv); 

	if (backupQ == NULL) {
		perror("Ran out of space");
		exit(1);
	}

	// infinite loop, here we handle requests
	for (;;) {
		// forking the process and creating a child
		// if the last room is full or this room is the first
		if (needroom) {
			needroom = 0;		// updating the flag to zero until we need a room
			
			// allocating space for a new room
			if (roomAlloc(&pArray) < 0) {
				perror("Couldn't allocate memory");
				exit(1);
			}

			// attempting to create a thread
			tstat = pthread_create(&pArray[roomsOpened], NULL, openGameRoom, sv);

			// checking if thread was successfully created
			if (tstat != 0) {
				perror("Couldn't create thread");
				exit(1);
			} else {
				++roomsOpened;
			}
			
			// join the thread with this process termination
			// pthread_join(pArray[roomsOpened], NULL);
		} else {	
			// waiting until we need a new room
			if (read(sv->openRoomFlag[0], &needroom, sizeof(needroom)) < 0) {
				perror("Couldn't read from the game server");
				exit(1);		
			}

			// resetting the quantity array
			copyQuantity(&(sv->inv), backupQ);
		}
	
	} // for
}

/*- ---------------------------------------------------------------- -*/
/**
 * @brief Callback function for the game room thread. It accepts players
 * before assigning them into a new thread. When the room is full this
 * function starts the message pushing function 
 *
 * @param Takes a void * that is casted into ServerVars *
 *
 * @return Returns NULL
 */
void *openGameRoom(void *arg) {
	// casting the argument to ServerVars
	ServerVars *sv = (ServerVars *)arg;

	// array of structs for the player data
	// on for each player
	PlayerData pd[sv->s.players];

	// player counter for the room
	int playerCount = 0;

	// thread array, we will assign one thread
	// per player. That way we can server many
	// clients at the same time
	pthread_t playerThread[sv->s.players];

	// creating a room specific mutex
	pthread_mutex_t lock;

	// initializing the mutex
    if (pthread_mutex_init(&lock, NULL) != 0)
    {
        perror("Mutex failed\n");
        exit(1);
    }

	// thread creation status
	int tstat = -1;

	// connection socket
	int connfd = -1;

	// client's address length
	socklen_t clilen;

	// client's address struct
	struct sockaddr_in cliaddr;

	// room flag
	int needroom = 0;
	int full = 0;

	// creating a pipe with the player thread
	int lastPlayer[2];
	pipe(lastPlayer);

	// socket array
	int *sockArray = malloc(sizeof(int)*(sv->s.players));

	if (sockArray == NULL) {
		perror("error -> sockArray");
		exit(1);
	}

	// printing the room's pid
	printf("| Opened a game room with pid: %d |\n", getpid());

	for (;;) {			
		clilen = sizeof(cliaddr);	// got address length

		if (!full) {
			// get next request and remove it from queue afterwards
			connfd = accept(sv->listenfd, (struct sockaddr *)&cliaddr, &clilen);

			// checking if connection was successfull
			if (connfd < 0) {
				if (errno == EINTR) {
					continue;
				} else { // something interrupted us
					fprintf(stderr, "Got an error while trying to connect \n");
					exit(1);
				}
			}

			// if we need one more player, then we set the server to
			// blocking as we want to filter the last request
			if ( playerCount == sv->s.players - 1 ) {
				++full;	// server might be full with the client we accepted
			}

			// keeping the sockets
			sockArray[playerCount] = connfd;
			
			// pack data for the player
				// connfd
			pd[playerCount].pconnfd = &connfd;
			
				// getting the sv struct to the pd for the player
			pd[playerCount].sv = sv;

				// keeping a pointer to the player counter
			pd[playerCount].pCount = &playerCount;

				// giving the mutex to the player
			pd[playerCount].lock = &lock;

				// giving him acces to the pipe
			pd[playerCount].lastPlayer = lastPlayer;

			// creating the thread that will serve this request
			tstat = pthread_create(&playerThread[playerCount], NULL, playerHandler, &pd);

			// checking if thread was successfully created
			if (tstat != 0) {
				perror("Couldn't create thread");
				exit(1);
			}			
			
			// if we raised the full flag because we suspected the room 
			// will be full after this client and this is the game server
			// not the child
			if (full) {
				// then we wait confirmation wheter or not the last
				// spot was taken
				read(lastPlayer[0], &full, sizeof(full));

				// if it was indeed taken, we give the go 
				// for a new room by entering the main loop
				if (full) {
					continue;
				}
			}

		} else {
			// printing a message from the server's point to
			// inform that this room is full
			printf("| Room %d: Full |\n", getpid());

			// raising the room flag and writing to the parent
			needroom = 1;
			if (write(sv->openRoomFlag[1], &needroom, sizeof(needroom)) < 0) {
				perror("Couldn't write to the main server");
				exit(1);		
			}

			// informing the server side that the game started
			printf("| Room %d: Game in progress ...|\n", getpid());

			// pushing messages from the child servers to the players
			pushMessage(lastPlayer, sockArray, &pd[0]);
			
			// informing the server side that this game ended
			printf("| Room %d: Game ended ...|\n", getpid());

			// breaking out of the loop
			break;
		}
	} // for
		
	// terminating the current thread
	pthread_exit(NULL);

}

/*- ---------------------------------------------------------------- -*/
/**
 * @brief Callback function for the threads that will serve 
 * incoming players
 *
 * @param Takes in a void * argument, that in our case is a
 * PlayerData struct 
 *
 * @return returns NULL
 */
void *playerHandler(void *arg) {
	// casting the parameter to PlayerData *
	PlayerData *pd = (PlayerData *)arg;

	// player name
	char *name = NULL;

	// setting an alarm for this player
	// if he doesn't stop it then we kick him
	alarm(WAIT);

	// initiate contact with the player
	servePlayer(&name, pd);
	
	// stopping the alarm
	alarm(0);

	// connecting this thread to the chat
	chat(*pd->pconnfd, pd->lastPlayer, name);

	// entering the critical area
	pthread_mutex_lock(pd->lock);

	// updating the counter that a player left
	--(*(pd->pCount));

	// leaving the critical area
	pthread_mutex_unlock(pd->lock);

	*pd->pconnfd = MYERRCODE;

	// informing the server side that a player disconnected
	printf("\t| Player > %s < left room %d |\n", name, getppid());

	// free-ing heap allocated memory
	free(name);

	// terminating the current thread
	pthread_exit(NULL);
}

/*- ---------------------------------------------------------------- -*/
/**
 * @brief Makes contact with the player and serves him depending on his
 * inventory. 
 *
 * @param Takes the PlayerData struct holding all necessairay variables
 * about the connection and the server and the playr's name
 *
 */
void servePlayer(char **name, PlayerData *pd) {
	// player vars
	char plStr[pSize];			// player's inventory in chars
	Inventory plInv;			// player's inventory in our struct
	char response[LINE_LEN];	// response to the player
	int status = 0;				// request status (valid/invalid)
	int full;					// flag for the game's last spot

	// waiting for the player to send us his inventory
	if (read(*(pd->pconnfd), plStr, sizeof(char)*pSize) < 0) {
		perror("Error reading the player's inventory");
		exit(1);
	}

	// parsing the string we received to our Inventory format
	parseStrIntoInv(name, plStr, &plInv);

	// entering the critical area
	pthread_mutex_lock(pd->lock);

	// checking if another player beat us to the last slot
	if (*(pd->pCount) < pd->sv->s.players) {

		// attempting to give items to the player
		status = subInventories(&(pd->sv->inv), plInv, (pd->sv->s.quota));
	} else {

		// can't add this player, not enough slots in the room
		status = 0;
	}
	
	// checking if the subtraction took place
	if (status) {

		// increasing the player counter
		++(*(pd->pCount));

		// leaving the critical area
		pthread_mutex_unlock(pd->lock);

		// informing the server side that a player successfully connected
		printf("| Player > %s < connected |\n", *name);

		// sending the ok message
		strcpy(response, "OK\n");	
	} else {

		// leaving the critical area
		pthread_mutex_unlock(pd->lock);

		// sending a problem message
		strcpy(response, "Encoutered a problem");
	}


	// entering the critical area
	pthread_mutex_lock(pd->lock);

	// checking for the last game room spot
	// if this is indeed the last player
	if ( *pd->pCount == pd->sv->s.players ) {

		// raising the flag
		full = 1;
		
		// we send confirmation to open another room
		write(pd->lastPlayer[1], &full, sizeof(full));
	} else if ( *pd->pCount == pd->sv->s.players ) {

		// otherwise we lower the flag
		full = 0;

		// and let the game server know
		write(pd->lastPlayer[1], &full, sizeof(full));		
	}

	// leaving the critical area
	pthread_mutex_unlock(pd->lock);

	// writing the response back to the player
	if (write(*pd->pconnfd, response, sizeof(response)) < 0) {
		perror("Couldn't respond to the player");
		exit(1);
	}

	if (!status) {
		// invalid request
		pthread_exit(NULL);
	}

	// free data before returning
	freeInventory(&plInv);
}

/*- ---------------------------------------------------------------- -*/
/**
 * @brief Initiates the chat. This function handles the server-player side
 * where if the player sends a message we push it to the game server for 
 * further distribution
 *
 * @param Takes in the connection socket, a pipe to reach the game server
 * and the player's name to attach to his messages
 *
 */
void chat(int connfd, int *plPipe, char *name) {
	// this end of the pipe to send messages
	int fd2 = plPipe[1];

	// declaring a buffer for the raw message
	char raw[pSize];

	// declaring a string that will hold the final message
	char message[pSize];

	// read/write fd sets
	fd_set read_set;
	fd_set write_set;

	while (1) {
		// zeroing the read and write fs sets
		FD_ZERO(&read_set);
		FD_ZERO(&write_set);

		// adding sockets to the sets
		FD_SET(connfd, &read_set);	// reading list
		FD_SET(fd2, &write_set);	// writing list

		// using select to check if the sockest in our list are ready to read/write
		if(select(connfd+1, &read_set, &write_set, NULL, NULL) > 0) {
			// checking if connfd is read to read
			if (FD_ISSET(connfd, &read_set)) {
				// attempting to read 
				if (read(connfd, raw, sizeof(raw)) > 0) {
					// checking if the pipe is good to read
					// writing the sender's socket num
					if (FD_ISSET(fd2, &write_set)) {
						write(fd2, &connfd, sizeof(connfd));
					}

					// checking if the pipe is good to read
					// writing the message
					if (FD_ISSET(fd2, &write_set)) {
						// adding the players name to the raw message
						sprintf(message, "[%s]: %s", name, raw);

						// writing the message
						write(fd2, message, sizeof(message));
					} 
				} else {
					// set the connfd to a custom error code
					connfd = MYERRCODE;

					// write this code back to the game server
					write(fd2, &connfd, sizeof(connfd));

					// lost connection to the player, so we exit the chat
					break;
				}
			} // connfd is set 
		} // select
	} // while
}

/*- ---------------------------------------------------------------- -*/
/**
 * @brief Receives messages from the child servers and then pushes directly
 * to the players 
 *
 * @param Takes in the pipe array between the game server and the child one,
 * the array with the stored client sockets, the shared memory pointer and
 * an index to the player counter for the shared memory pointer
 *
 */
void pushMessage(int *plPipe, int *sockArray, PlayerData *pd) {
	char message[pSize];	// message that we have to push	
	int senderSocket = -1;	// used to identify the sender's socket to avoid duplicate message
	int i;					// for counter

	// first message to send is START
	strcpy(message, "START\n");

	// sending the game start message to the players
	for (i=0; i<*pd->pCount; ++i) {
		// attempting to write the message to the clients
		if (write(sockArray[i], message, sizeof(message)) < 0) {							
			perror("Couldn't write START");
			exit(1);
		}
	} 

	// pushing until all players leave the room
	pthread_mutex_lock(pd->lock);	// entering critical area
	while (*pd->pCount > 0) {
	pthread_mutex_unlock(pd->lock);	// leaving critical area

		// attempting to read the sender's socket
		read(plPipe[0], &senderSocket, sizeof(senderSocket));

		// received an error code, which means the player left
		if (senderSocket == MYERRCODE) {
			continue;
		}

		// attempting to get the message from the child server
		if (read(plPipe[0], message, sizeof(message)) < 0) {
			perror("Error pushing message");
		} else {				
			// iterating through the open sockets to push the message
			for (i=0; i<pd->sv->s.players; ++i) {
				// this was the sender so we don't push the message
				if (sockArray[i] == senderSocket || sockArray[i] == MYERRCODE) {
					continue;
				}

				// attempting to write the message to the clients
				if (write(sockArray[i], message, sizeof(message)) < 0) {							
					perror("Error pushing message 2");
				}
			} // for
		}
	} // while 
	
	// // exiting critical area
	// sem_post(my_sem);
}

/*- ---------------------------------------------------------------- -*/
/**
 * @brief Allocates additional space for one more pthread variable
 *
 * @param Takes in a double pointer to a pthread array 
 *
 * @return Returns 0 on success otherwise -1
 */
int roomAlloc(pthread_t **pArray) {
	// reserving space for at least on thread
	(*pArray) = realloc( (*pArray), sizeof(pthread_t)*(roomsOpened+1) );

	// checking if the allocation was successfull
	if ((*pArray) == NULL) {
		perror("Couldn't allocate memory for the room");
		return -1;
	}

	return 0;
}

/*- ---------------------------------------------------------------- -*/
/**
 * @brief Handles the sigchld signal
 *
 * @param Takes in the signal int code
 *
 */
void catch_sig(int signo) {
	(void) signo;	// unused

	pid_t pid;
	int stat;

	// ensuring that all children processes have died
	while( (pid = waitpid(-1, &stat, WNOHANG)) > 0 ) {
		// waiting for all children to die, avoiding zombie processes
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
	(void) signo;	// unused

	if (pprocID == getpid()) {
		// goodbye message
		printf("\n\n\t\t Server terminated with SIGINT ... GoodBye ! \n\n");	
	}

	// exiting
	exit(0);
}

/*- ---------------------------------------------------------------- -*/
/**
 * @brief Handles the sigalarm signal
 *
 * @param Takes in the signal int code
 *
 */
void catch_alarm(int signo) {
	(void) signo;	// unused

	// deactivating the alarm
	signal(SIGALRM, SIG_IGN);

	// inforiming the server user that this connection timed out
	printf("| A player in room %d timed out and was kicked ... |\n", getpid());

	// exiting ...
	exit(1);
}

/*- ---------------------------------------------------------------- -*/
