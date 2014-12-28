/**
 * @file Client.c
 * @author Vaios Papaspyros
 * 
 * @brief Implemenation of the game's client
 *
 * In this file the player (client) is created. We make contact with
 * the server and get its response concerning the room the player 
 * must be assigned in. We also check his quota and its validity.
 *
 */

#include "Inventory.h"
#include "ClientBackend.h"

#define WAIT 60

// initializes the sockets and server
void init(int *sockfd, struct hostent *server, 
	struct sockaddr_in *servaddr, char *h);

// getting the client running
void clientUp(int sockfd, struct sockaddr_in *servaddr, cSettings set, Inventory inv);

// sending the inventory to the server
int sendInv(Inventory inv, int sockfd);

// declaring a var to let us know when the client waited too long
volatile int timeOut = WAIT;

// handler for the alarm function
void catch_alarm(int signo) {
	(void) signo;

	signal(SIGALRM, SIG_IGN);
	printf("Waiting for game to start ... \n\n");
	signal(SIGALRM, catch_alarm);

	if(!(timeOut -= 5)) {
		printf("Connection timed out, exiting ... \n\n");
		exit(1);
	}

	alarm(5);
}


/*- ---------------------------------------------------------------- -*/

int main(int argc, char **argv) {
	cSettings set;					// player settings
	Inventory inv;					// player inventory

	int sockfd;						// socket
	struct sockaddr_in servaddr;	// struct for server address
	struct hostent *server = NULL;	// stores information about the given host

	// setting the alarm handler
	signal(SIGALRM, catch_alarm);

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

	// writing the string to the server
	if (write(sockfd, strInv, sizeof(strInv)) < 0) {
		perror("Error while sending the inventory");
		exit(1);
	}

	// waiting for the server to respond on the inventory and 
	// this player's participationmak
	if (read(sockfd, response, sizeof(response)) < 0) {
		perror("Error getting the server's response");
		exit(1);
	}

	// checking the response
	if (strcmp(response, "OK\n")) {
		// something went wrong therefore we inform the player
		printf("Your inventory is invalid or the requested items are not available\n");
		printf("Exiting ... Try again with a different inventory\n");

		exit(1);
	} else {
		printf("%s\n", response);
	}

	// setting the alarm to go off every 5 seconds
	alarm(5);



	close(sockfd);
}
