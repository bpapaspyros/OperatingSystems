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

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>

#define SHMSZ  27

void makeHandshake(int s);

int main(int argc, char **argv) {
	cSettings set;					// player settings

	int sockfd;						// socket
	struct sockaddr_in servaddr;	// struct for server address
	struct hostent *server;
	int pid;						// pid

	// getting parameters to set up the server according to the user
	initcSettings(argc, argv, &set);

	// initializing connection variables
	server = gethostbyname(set.host_name);
	if ( server == NULL ) {
		fprintf(stderr, "Invalid hostname \n");
		exit(1);
	}

	sockfd = socket(AF_INET, SOCK_STREAM, 0);  // client endpoint
	if ( sockfd < 0 ) {
		perror("Couldn't open socket");
	}

	bzero(&servaddr, sizeof(servaddr)); 	   // zero servaddr fields
	bcopy((char *)server->h_addr, 
         (char *)&servaddr.sin_addr.s_addr,
         server->h_length);
	servaddr.sin_family = AF_INET;			   // setting the socket type to local
	servaddr.sin_port = htons(PORT_NO);


/*- ------------------- testing ------------------- */
	// connect the client's and server's endpoints
	if ( connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0 ) {
		perror("Couldn't connect");
	}

	int j = 0;
	char send;
	int getID;
	char buf[20];

	write(sockfd, set.name, sizeof(set.name));

	// close(sockfd);
	// connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
	// while( getchar() != '\n' ) {

	// }

	read(sockfd, buf, sizeof(buf));

	printf("assigned id: %s\n", buf);

	// if (read(connfd, &get, sizeof(int)) > 0) {
	// 	if (getID != -1) {
	// 		set.roomID = getID;
	// 	}
	// }


	close(sockfd);
/*- ------------------- testing ------------------- */

	return 0;
}

