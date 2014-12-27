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

// initializes the sockets and server
void init(int *sockfd, struct hostent *server, 
	struct sockaddr_in *servaddr, char *h);

// getting the client running
void clientUp(int sockfd, struct sockaddr_in *servaddr, cSettings set, Inventory inv);

// sending the inventory to the server
int sendInv(Inventory inv, int sockfd);


/*- ---------------------------------------------------------------- -*/

int main(int argc, char **argv) {
	cSettings set;					// player settings
	Inventory inv;					// player inventory

	int sockfd;						// socket
	struct sockaddr_in servaddr;	// struct for server address
	struct hostent *server = NULL;	// stores information about the given host


	// getting parameters to set up the server according to the user
	initcSettings(argc, argv, &set);

	// taking data from inventory to a struct for easy management
	if ( readInventory(set.inventory, &inv, 0, NULL) ) {
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
	// connect the client's and server's endpoints
	if ( connect(sockfd, (struct sockaddr *)servaddr, sizeof(*servaddr)) < 0 ) {
		perror("Couldn't connect");
		exit(1);
	}
	

	close(sockfd);
}
