/**
 * @file Server.c
 * @author Vaios Papaspyros
 * 
 * @brief Implementation of the game's server
 *
 * Shared memory allocation, creation of sockets and all the backend
 * functions that are expected from a server are implemented in
 * this file. We handle the players requests and implement a chatting
 * system
 *
 */

#include "Inventory.h"
#include "ServerBackend.h"	// server backend, which handles the game


	/*- ---- Global Variables & Defining ---- -*/ 
#define LISTENQ 150		// size for the queue
#define SHM_KEY 5623	// key for the shared memory segment
#define WAIT 60			// wait time for the server until connection expires
#define MYERRCODE -5623 // used as error code, funny because it's my student id

sem_t *my_sem = NULL;		// declaring a semaphore variable
int roomsOpened = 0; 		// room counter
int shmid = MYERRCODE;		// id of the current room's shared memory segment
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
void openGameRoom(int *fd, ServerVars *sv);

// opens another server that handles his player's requests
void servePlayer(int connfd, int *qData, ServerVars *sv, char **name,
	int *fullFlag, int full);

// gets a single player's messages and sends it to the game room server
int chat(int connfd, int *plPipe, char *name, int *qData, 
	int plCountPos, int players);

// game room handles pushing messages to all the players
void pushMessage(int *plPipe, int *sockArray, int *qData, int plCountPos, int players);

// opens a memory segment for ipc
int openSharedMem(Inventory *inv, int **data);

// closes the memory segments we opened
void closeSharedMem(int shmid);
	/*- ------- Function declarations ------- -*/ 

/*- ---------------------------------------------------------------- -*/
// 				Function definitions 
/*- ---------------------------------------------------------------- -*/
int main(int argc, char **argv) {
	// compact way to access useful vars
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

	// attempting to open the semaphore
	my_sem = sem_open("sem5623", O_CREAT, 0600, 1);

	// checking if the semaphore opened
	if (my_sem == SEM_FAILED) {	
		printf("Could not open semaphore!\n");
		exit(1);
	}
}

/*- ---------------------------------------------------------------- -*/
/**
 * @brief Main server loop where the server waits for client requests.
 * Here we make the necessary calls to arrange the game rooms and check
 * for the client's request validity
 *
 * @param Takes the ServerVars struct containing the inventory and settings
 *
 */
void serverUp(ServerVars *sv) {

	// child pid
	pid_t childpid = -1;

	// flag to signal us that we need a new room
	int needroom = 1;

	// pipe vars
	int fd[2];	// pipe array
	pipe(fd);	// declaring fd is a pipe

	// storing this process's id
	pprocID = getpid();

	// infinite loop, here we handle requests
	for (;;) {
		// forking the process and creating a child
		// if the last room is full or this room is the first
		if (needroom) {
			needroom = 0;		// updating the flag to zero until we need a room
			++roomsOpened;		// about to open a new room
			childpid = fork();	// well ... fork
		}

		if (childpid == 0) {	// checking if it is the child process	
			needroom = 0;		// only the parent server can create rooms, avoiding trouble	
			openGameRoom(fd, sv);
		
			// making sure no child survives past this point
			exit(0);
		} else {
			// Printing the parent pid
			printf("\n\n| Main Server pid: %d |\n", getpid());

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
 * @param Takes the pipe array, ServerVars struct containing the 
 * inventory, settings and listening socket vars
 *
 */
void openGameRoom(int *fd, ServerVars *sv) {
	
	// pid for the server process that will handle the player
	pid_t newpid = -1;

	// connection socket
	int connfd = -1;

	// client's address length
	socklen_t clilen;

	// client's address struct
	struct sockaddr_in cliaddr;

	// room flag
	int needroom = 0;
	int full = 0;

	// share memory vars
	int *qData = NULL;		// pointer to our shared memory data

	// player's name
	char *name;

	int plPipe[2];	// pipe array
	pipe(plPipe);	// declaring plPipe is a pipe

	int fullFlag[2];	// pipe array
	pipe(fullFlag);		// declaring fullFlag is a pipe

	// socket array
	int *sockArray = malloc(sizeof(int)*(sv->s.players));

	// storing this process's id
	rprocID = getpid();

	// printing the room's pid
	printf("| Opened a game room with pid: %d |\n", rprocID);

	if (sockArray == NULL) {
		perror("error -> sockArray");
		exit(1);
	}

	// opening a room specific shared memory
	shmid = openSharedMem(&(sv->inv), &qData);

	for (;;) {			
		clilen = sizeof(cliaddr);	// got address length

		if (!full) {
			// get next request and remove it from queue afterwards
			connfd = accept(sv->listenfd, (struct sockaddr *)&cliaddr, &clilen);

			// checking if connection was successful
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
			if ( qData[sv->inv.count] == sv->s.players - 1) {
				++full;	// server might be full with the client we accepted
			}

			// keeping the sockets
			sockArray[(qData[sv->inv.count])] = connfd;

			// forking the process to serve the request
			newpid = fork();
			
			// if we raised the full flag because we suspected the room 
			// will be full after this client and this is the game server
			// not the child
			if (full && (newpid != 0)) {

				// then we wait confirmation whether or not the last
				// spot was taken
				read(fullFlag[0], &full, sizeof(full));

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
			if (write(fd[1], &needroom, sizeof(needroom)) < 0) {
				perror("Couldn't write to the main server");
				exit(1);		
			}

			// informing the server side that the game started
			printf("| Room %d: Game in progress ...|\n", getpid());

			// pushing messages from the child servers to the players
			pushMessage(plPipe, sockArray, qData, sv->inv.count, sv->s.players);

			// detaching the room from the shared memory
			shmdt(qData);

			// marking the shared memory segment for deletion
			closeSharedMem(shmid);
			
			// informing the server side that this game ended
			printf("| Room %d: Game ended ...|\n", getpid());

			// breaking out of the loop
			break;
		}

		// the child process handles the request
		if (newpid == 0) {

			// closing up the listening socket
			close(sv->listenfd);	

			// setting an alarm for this player
			// if he doesn't stop it then we kick him
			alarm(WAIT);

			// resetting the copied pid to error status
			// so that we can tell these processes apart
			rprocID = MYERRCODE;

			// initiate contact with the player
			servePlayer(connfd, qData, sv, &name, fullFlag, full);

			// stopping the alarm
			alarm(0);

			// connecting the player to the chat
			chat(connfd, plPipe, name, qData, sv->inv.count, sv->s.players);

			// entering critical area
			sem_wait(my_sem);

			// lost connection to the player
			--qData[sv->inv.count];

			// leaving critical area
			sem_post(my_sem);

			// informing the server side that a player disconnected
			printf("\t| Player > %s < left room %d |\n", name, getppid());

			// exiting this process
			exit(0);
		}
	} // for
	
	// exiting with success status after closing up the room
	exit(0);
}

/*- ---------------------------------------------------------------- -*/
/**
 * @brief Makes contact with the player and serves him depending on his
 * inventory. 
 *
 * @param Takes in the connection socket, the shared memory pointer,
 * the ServerVars struct containing the inventory, settings and 
 * listening socket vars the player's name, the parent pipe and 
 * the full flag
 *
 */
void servePlayer(int connfd, int *qData, ServerVars *sv, char **name, 
	int *fullFlag, int full) {
	// player vars
	char plStr[pSize];			// player's inventory in chars
	Inventory plInv;			// player's inventory in our struct
	char response[LINE_LEN];	// response to the player
	int status = 0;				// request status (valid/invalid)

	// waiting for the player to send us his inventory
	if (read(connfd, plStr, sizeof(plStr)) < 0) {
		perror("Error reading the player's inventory");
		exit(1);
	}

	// parsing the string we received to our Inventory format
	parseStrIntoInv(name, plStr, &plInv);

	// locking the segment (critical section)
	sem_wait(my_sem);

	// attempting to give items to the player
	status = subInventories(&sv->inv, plInv, qData, sv->s.quota);

	// checking if the subtraction took place
	if (status) {

		// increasing the player counter
		++qData[sv->inv.count];

		// unlocking the segment (out of the critical section)
		sem_post(my_sem);

		// informing the server side that a player successfully connected
		printf("| Player > %s < connected |\n", *name);

		// sending the ok message
		strcpy(response, "OK\n");	
	} else {

		// unlocking the segment
		sem_post(my_sem);

		// sending a problem message
		strcpy(response, "Encoutered a problem");
	}

	// the game server has raised this flag
	// meaning this might be the last player
	if (full) {
		// if this is indeed the last player
		if ( qData[sv->inv.count] == sv->s.players) {
			
			// we send confirmation to open another room
			write(fullFlag[1], &full, sizeof(full));
		} else if (qData[sv->inv.count] == sv->s.players - 1) {

			// otherwise we lower the flag
			full = 0;

			// and let the game server know
			write(fullFlag[1], &full, sizeof(full));		
		}
	}

	// writing the response back to the player
	if (write(connfd, response, sizeof(response)) < 0) {
		perror("Couldn't respond to the player");
		exit(1);
	}

	if (!status) {
		// invalid request
		exit(0);
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
int chat(int connfd, int *plPipe, char *name, int *qData, 
	int plCountPos, int players) {
	// this end of the pipe to send messages
	int fd2 = plPipe[1];

	// declaring a buffer for the raw message
	char raw[pSize];

	// declaring a string that will hold the final message
	char message[pSize];

	// read/write fd sets
	fd_set read_set;
	fd_set write_set;

	// until everyone is connected tell the player to wait
	strcpy(message, "Waiting for more players ...\n");

	// waiting for other players
	sem_wait(my_sem);	// entering critical area
	while (qData[plCountPos] != players) {
		sem_post(my_sem);	// leaving critical area
		if (write(connfd, message, sizeof(message)) <= 0) {
			return 1;
		}

		// sleep for 5
		sleep(5);
	}
	sem_post(my_sem);	// leaving critical area

	// letting the player know the game is starting
	strcpy(message, "START\n");
	if (write(connfd, message, sizeof(message)) <= 0) {
		return 1;
	}

	while (1) {
		// zeroing the read and write fs sets
		FD_ZERO(&read_set);
		FD_ZERO(&write_set);

		// adding sockets to the sets
		FD_SET(connfd, &read_set);	// reading list
		FD_SET(fd2, &write_set);	// writing list

		// using select to check if the socket in our list are ready to read/write
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
					// closing this socket
					close(connfd);
					
					// lost connection to the player, so we exit the chat
					break;
				}
			} // connfd is set 
		} // select
	} // while

	return 0;
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
void pushMessage(int *plPipe, int *sockArray, int *qData, int plCountPos, int players) {
	char message[pSize];	// message that we have to push	
	int senderSocket = -1;	// used to identify the sender's socket to avoid duplicate message
	int i;					// for counter

	// pushing until all players leave the room
	sem_wait(my_sem);		// entering critical area
	while (qData[plCountPos] > 0) {
		sem_post(my_sem);	// exiting critical area

		// attempting to read the sender's socket
		read(plPipe[0], &senderSocket, sizeof(senderSocket));

		// attempting to get the message from the child server
		if (read(plPipe[0], message, sizeof(message)) < 0) {
			perror("Error pushing message");
		} else {				
			// iterating through the open sockets to push the message
			for (i=0; i<players; ++i) {
				// this was the sender so we don't push the message
				if (sockArray[i] == senderSocket) {
					continue;
				}

				// attempting to write the message to the clients
				if (write(sockArray[i], message, sizeof(message)) <= 0) {							
					continue;
				}
			} // for
		}
	} // while 
	
	// exiting critical area
	sem_post(my_sem);
}

/*- ---------------------------------------------------------------- -*/
/**
 * @brief Creates a shared memory segment the size of our inventory
 * struct, with a key that depends on the rooms opened. This way we
 * can achieve a dynamic shared memory allocator so that each room
 * gets its own segment to write on
 *
 * @param Takes in the number of rooms opened, the inventory struct 
 * and a pointer to the beginning of the segment
 *
 */
int openSharedMem(Inventory *inv, int **data) {
	int shmid;
	key_t key;
	size_t shmsize = sizeof(int)*(inv->count+1);
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

	// last int in the shared memory stands for the player counter
	*start = 0;

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
	// returns the key after shmctl
	key_t ret;

	// attempting to delete the shared memory segment based on its id
	if ((ret = shmctl(shmid, IPC_RMID, (struct shmid_ds *) NULL)) == -1) {
		perror("shmctl error");
		exit(1);
	}

	// setting the variable that used to hold 
	// the shared memory id to error status
	shmid = MYERRCODE;
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

	// if this child is a game server and has died we remove
	// its dedicated shared memory (if it exists)
	if ( !(shmid == MYERRCODE) && !(rprocID == MYERRCODE) ) {
		closeSharedMem(shmid);
	}

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

	// if this child is a game server and has died we remove
	// its dedicated shared memory (because we received a sigint)
	// the last open segment is still open
	if ( !(shmid == MYERRCODE) && !(rprocID == MYERRCODE) ) {
		closeSharedMem(shmid);
	}


	if (pprocID == getppid()) {
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
