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

#include <stdio.h>
#include "ServerBackend.h"

/*- ---------------------------------------------------------------- -*/

int main(int argc, char **argv) {
	Settings set;			// Game settings

	// getting parameters to set up the server according to the user
	initSettings(argc, argv, &set);




	return 0;
}

/*- ---------------------------------------------------------------- -*/
