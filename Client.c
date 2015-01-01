/**
 * @file Client.c
 * @author Vaios Papaspyros
 * 
 * @brief Implemenation of the game's client
 *
 * In this file the player (client) is created. We make contact with
 * the server and get its response concerning the room the player 
 * must be assigned in. We also check his quota and its validity.
 * If the server gives as confirmation we connect to his chat
 *
 */

#include "Inventory.h"
#include "ClientBackend.h"

#include <pthread.h>

	/*- ---- Global Variables & Defining ---- -*/ 
#define WAIT 60 // wait time

// declaring a var to let us know when the client waited too long
volatile int timeOut = WAIT;
	/*- ---- Global Variables & Defining ---- -*/ 


	/*- ------- Function declarations ------- -*/ 
// thread start functions
void *playerRead(void *param);
void *playerWrite(void *param);

void initChat(int sockfd);

void catch_alarm(int signo);
void catch_alarm_con(int signo);

void init(int *sockfd, struct hostent *server, 
	struct sockaddr_in *servaddr, char *h);

void clientUp(int sockfd, struct sockaddr_in *servaddr, cSettings set, Inventory inv);
int sendInv(Inventory inv, int sockfd);
	/*- ------- Function declarations ------- -*/ 

/*- ---------------------------------------------------------------- -*/
// 				Function definitions 
/*- ---------------------------------------------------------------- -*/
int main(int argc, char **argv) {
	cSettings set;					// player settings
	Inventory inv;					// player inventory

	int sockfd;						// socket
	struct sockaddr_in servaddr;	// struct for server address
	struct hostent *server = NULL;	// stores information about the given host

	// setting the alarm to another handler
	signal(SIGALRM, catch_alarm_con);

	// getting parameters to set up the server according to the user
	initcSettings(argc, argv, &set);

	// taking data from inventory to a struct for easy management
	if ( readInventory(set.inventory, &inv) ) {
		perror("Inventory problem");
		return -1;
	}
	
	// printing the inventory to the user
	printInventory(inv);

	// initializing socket and server values
	init(&sockfd, server, &servaddr, set.host_name);

	// starting up the client
	clientUp(sockfd, &servaddr, set, inv);

	// free-ing allocated space
	free(server);

	return 0;
}

/*- ---------------------------------------------------------------- -*/
/**
 * @brief Initializing socket and server values according to the 
 * hostname given and the port selected
 *
 * @param Takes in the socket, the hostent struct, the sockaddress
 * struct and the hostname
 *
 */
void init(int *sockfd, struct hostent *server, 
	struct sockaddr_in *servaddr, char *host) {
		
	// initializing connection variables
	server = gethostbyname(host);
	if ( server == NULL ) {
		fprintf(stderr, "Invalid hostname \n");
		exit(1);
	}

	*sockfd = socket(AF_INET, SOCK_STREAM, 0);  // client endpoint
	if ( *sockfd < 0 ) {
		perror("Couldn't open socket");
	}

	// zero servaddr fields
	bzero(servaddr, sizeof(*servaddr)); 

	// copying server values to the hostent struct
	bcopy((char *)server->h_addr, 
         (char *)&servaddr->sin_addr.s_addr,
         server->h_length);

	// setting the family and port for the server
	servaddr->sin_family = AF_INET;	
	servaddr->sin_port = htons(PORT_NO);
}

/*- ---------------------------------------------------------------- -*/
/**
 * @brief Setting up the client to make contact with the server and 
 * managing all the client actions
 *
 * @param Takes in the socket and the sockaddress
 *
 */
void clientUp(int sockfd, struct sockaddr_in *servaddr, cSettings set, Inventory inv) {
	char strInv[pSize];		// player's inventory in chars
	char response[LINE_LEN];// server's response depending on the inventory's validity

	// connect the client's and server's endpoints
	if ( connect(sockfd, (struct sockaddr *)servaddr, sizeof(*servaddr)) < 0 ) {
		perror("Couldn't connect");
		exit(1);
	}
	
	// parsing the inventory struct to char * (ascii chars)
	parseInvIntoStr(set.name, inv, strInv);

	// setting an alarm to close the connection
	// if the response takes too long
	alarm(30);	// shouldn't take more than 30 seconds

	// writing the string to the server
	if (write(sockfd, strInv, sizeof(strInv)) < 0) {
		perror("Error while sending the inventory");

		close(sockfd);

		exit(1);
	}

	// waiting for the server to respond on the inventory and 
	// this player's participationmak
	if (read(sockfd, response, sizeof(response)) < 0) {
		perror("Error getting the server's response");
		
		close(sockfd);

		exit(1);
	}

	// checking the response
	if (strcmp(response, "OK\n")) {
		// something went wrong therefore we inform the player
		printf("Your inventory is invalid or the requested items are not available\n");
		printf("Exiting ... Try again with a different inventory\n");

		close(sockfd);

		exit(1);
	} else {
		printf("%s\n", response);
	}

	// stopping the alarm
	alarm(0);

	// // removing the handler
	// signal(SIGALRM, SIG_IGN);

	// // opening the chat
	// initChat(sockfd);

	// close the connected socket
	close(sockfd);
}

/*- ---------------------------------------------------------------- -*/
/**
 * @brief Opens the chat. This function creates 2 threads, one for
 * reading and one for writing
 *
 * @param Takes in the connection socket
 *
 */
void initChat(int sockfd)
{
	pthread_t threadRead, threadWrite;	// declaring two threads
	int ird, iwr;						// pcreate return values

	// creating a thread for reading from the chat
	ird = pthread_create(&threadRead, NULL, playerRead, &sockfd);
	
	if (ird) {	// checking for errors
		fprintf(stderr, "Error - pthread_create() return code: %d\n", ird);
		exit(1);
	}

	// creating a thread for writing to the chat
	iwr = pthread_create(&threadWrite, NULL, playerWrite, &sockfd);
	
	if (iwr) {	// checking for errors
		fprintf(stderr, "Error - pthread_create() return code: %d\n", iwr);
		exit(1);
	}

	// join the threads with this process's termination
	pthread_join(threadRead, NULL);
	pthread_join(threadWrite, NULL);

	// exiting ... 
	exit(0);
}


/*- ---------------------------------------------------------------- -*/
/**
 * @brief Start function for the reading thread. Prints the receives 
 * messages
 *
 * @param Takes in a pointer to the connection socket
 *
 * @return Returns NULL
 */
void *playerRead(void *args) {
	// casting the parameter to int *
	int *sockfd = (int *)args;

	// declaring a buffer for reading
	char msg[pSize];

	// setting the alarm to another handler
	signal(SIGALRM, catch_alarm);

	// setting the alarm to go off every 5 seconds
	alarm(5);

	// waiting for the START message
	if (read(*sockfd, msg, sizeof(msg)) < 0) {
		perror("Error getting the server's response");
		exit(1);
	}	

	// got the START message and we stop the timer
	alarm(0);

	// print the message
	printf("%s\n", msg);

	// while connection is good
	while (1) {
		// get the message
		if (read(*sockfd, msg, sizeof(msg)) < 0) {
			perror("Error getting the server's response");
			exit(1);
		}	

		// print the message
		printf("%s\n", msg);
	}


	return NULL;
}

/*- ---------------------------------------------------------------- -*/
/**
 * @brief Start function for the writing thread. Writes messages read
 * from the stdin to the connected socket
 *
 * @param Takes in a pointer to the connection socket
 *
 * @return Returns NULL
 */
void *playerWrite(void *args) {
	// casting the parameter to int *
	int *sockfd = (int *)args;

	char c;			// character from stdin
	int count = 0;	// characters read
	char msg[pSize];// character array

	// while connection is good
	while(1) {
		// while the player doesn't press ENTER
		while( (c = getchar()) != '\n' ) {
			// reading what he typed
			msg[count++] = c;
		}

		// terminating the string
		msg[count] = '\0';

		// writing the string to the server
		if (write(*sockfd, msg, sizeof(msg)) < 0) {
			perror("Error while sending the inventory");
			exit(1);
		}

		// resetting the counter
		count = 0;
	}

	return NULL;
}

/*- ---------------------------------------------------------------- -*/
/**
 * @brief Handles the sigalarm signal
 *
 * @param Takes in the signal int code
 *
 */
void catch_alarm_con(int signo) {
	(void) signo;	// unused

	// always wanted to say that
	printf("Connection is taking longer than usual. Exiting ... \n\n");
	
	// exiting
	exit(1);
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

	// removing the handler
	signal(SIGALRM, SIG_IGN);

	// printing wait message
	printf("Waiting for game to start ... \n\n");
	
	// setting the handler again
	signal(SIGALRM, catch_alarm);

	// checking if we waited too long
	if(!(timeOut -= 5)) {
		printf("Connection timed out, exiting ... \n\n");
		exit(1);
	}

	// set the timer again
	alarm(5);
}
/*- ---------------------------------------------------------------- -*/