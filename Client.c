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

#include "SharedHeader.h"
#include "ClientBackend.h"


void testFeature();



// initializes the sockets and server
void init(int *sockfd, struct hostent *server, 
	struct sockaddr_in *servaddr, char *h);

// getting the client running
void clientUp(int sockfd, struct sockaddr_in *servaddr, cSettings set);

/*- ---------------------------------------------------------------- -*/

int main(int argc, char **argv) {
	cSettings set;					// player settings

	int sockfd;						// socket
	struct sockaddr_in servaddr;	// struct for server address
	struct hostent *server;			// stores information about the given host


	// getting parameters to set up the server according to the user
	initcSettings(argc, argv, &set);

	// initializing socket and server values
	init(&sockfd, server, &servaddr, set.host_name);

	// starting up the client
	clientUp(sockfd, &servaddr, set);

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
void clientUp(int sockfd, struct sockaddr_in *servaddr, cSettings set) {
	char buf[20];

	// connect the client's and server's endpoints
	if ( connect(sockfd, (struct sockaddr *) servaddr, sizeof(*servaddr)) < 0 ) {
		perror("Couldn't connect");
	}

/*- ------------------- testing ------------------- */
	write(sockfd, set.name, sizeof(set.name));
	read(sockfd, buf, sizeof(buf));
	printf("assigned id: %s\n", buf);




	testFeature();




	close(sockfd);
/*- ------------------- testing ------------------- */
}

void testFeature() {
 
	int shmid;
	key_t key;
	char *shm, *s;



	/*
	* We need to get the segment named
	* "5678", created by the server.
	*/

	key = 5678;

	/*
	* Locate the segment.
	*/

	if ((shmid = shmget(key, SHMSZ, 0666)) < 0) {
		perror("shmget");
		exit(1);
	}



	/*
	* Now we attach the segment to our data space.
	*/

	if ((shm = shmat(shmid, NULL, 0)) == (char *) -1) {
		perror("shmat");
		exit(1);
	}

	/*
	* Now read what the server put in the memory.
	*/

	for (s = shm; *s != NULL; s++)
		putchar(*s);

	putchar('\n');


	/*
	* Finally, change the first character of the
	* segment to '*', indicating we have read
	* the segment.
	*/

	*shm = '*';
}
