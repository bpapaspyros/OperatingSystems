/**
 * @file Client.c
 * @author Vaios Papaspyros
 * 
 * @brief Implemenation of the game's client
 *
 * In this file the player (client) is created. We make contact with
 * the server and get its response concerning the room the player 
 * must be assigned in. We also check his quota and its validity.
 * We make use of atomic functions to ensure that we don't exceed
 * the game's inventory
 *
 */

#include <stdio.h>
#include "ClientBackend.h"

int main(int argc, char **argv) {
	cSettings set;			// player settings

	// getting parameters to set up the server according to the user
	initcSettings(argc, argv, &set);


	return 0;
}