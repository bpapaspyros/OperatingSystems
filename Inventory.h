#ifndef SHAREDHEADER_H
#define SHAREDHEADER_H

#include <stdio.h>		// standard i/o
#include <stdlib.h>		// standard library e.g. exit function
#include <string.h>		// string handling
#include <unistd.h>		// declaration of numerous functions and macros
#include <sys/socket.h> // socket definitions
#include <sys/types.h> 	// system data types
#include <sys/un.h> 	// Unix domain sockets
#include <errno.h>		// EINTR variable
#include <sys/wait.h>	// waitpid system call
#include <netinet/in.h>	// includes all the necessary protocols
#include <netdb.h>		// definitions for network database operations
#include <semaphore.h>	// semaphore functions
#include <sys/ipc.h>	// inter process communication access structure
#include <sys/shm.h>	// defines constants and functions for shared memory
#include <fcntl.h>      // contains the O* constants
#include <sys/stat.h>	// mode constants

// defining port number
#define PORT_NO 5623

// defining line length
#define LINE_LEN 32

// defining the max size in chars of an inventory
#define pSize 1024

// struct containing the inventory data
typedef struct {
	char **items;
	int *quantity;
	int count;
	int quota;
}Inventory;

/*- ---------------------------------------------------------------- -*/
/**
 * @brief This function initializes the variables of an inventory struct
 * to zero or NULL values so that they are ready for further editing
 *
 * @param Takes an inventory pointer
 *
 */
void initInventory(Inventory *inv) {
	// initializing the counter of the items to zero
	inv->count = 0;
	inv->quota = 0;

	// setting our pointers to null
	inv->items = NULL;
	inv->quantity = NULL;
}

/*- ---------------------------------------------------------------- -*/
/**
 * @brief This function allocates memory for one item in the struct
 * and fills in this item
 *
 * @param Takes an inventory pointer, the item string and its quantity
 *
 */
void newInventoryRecord(Inventory *inv, char *item, int quantity) {
	// allocating memory for our arrays
	inv->quantity = realloc((inv->quantity), sizeof(int)*(inv->count+1));

	if ( (inv->quantity) == NULL ) {
		perror("Allocation error");
		exit(1);
	}

	// 2d array re allocations
		// first dimension
	inv->items = realloc((inv->items), (inv->count+1)*sizeof(char *));

	if ( (inv->items) == NULL ) {
		perror("Allocation error -> items first dimension");
		exit(1);
	}

		// initializing the allocated pointer to NULL
	inv->items[inv->count] = NULL;

		// second dimension
	inv->items[inv->count] = realloc((inv->items[inv->count]), (strlen(item)+1)*sizeof(char));			

	if ( (inv->items[inv->count]) == NULL ) {
		perror("Allocation error -> items second dimension");
		exit(1);
	}

	// assigning the values tha were read to the struct
	inv->quantity[inv->count] = quantity;
	strcpy(inv->items[inv->count], item);

	// increasing the counter since we added a new item
	inv->count++;
	inv->quota += quantity;
}

/*- ---------------------------------------------------------------- -*/
/**
 * @brief This function frees the heap allocated memory
 *
 * @param Takes an inventory pointer
 *
 */
void freeInventory(Inventory *inv) {
	int i;	// for counter

	// free-ing the 1d array
	free((inv->quantity));

	// free-ing the 2nd dimension of our items array
	for (i=0; i<(inv->count); ++i) {
		free((inv->items[i]));
	}

	// free-ing the 1st dimension of the items array
	free((inv->items));
}

/*- ---------------------------------------------------------------- -*/
/**
 * @brief This function reads through the given inventory
 * file and adds the data given to a struct so that we don't
 * have to read through the file all the time
 *
 * @param Takes in the filename and an instance of an inventory 
 * struct, an int that tells use whether tha player name is
 * included in the file and a name string
 *
 * @return Returns 0 if the function ran correctly and 1 if not
 */
int readInventory(char *filename, Inventory *inv) {
	FILE *fp;		// file pointer
	char *buffer;	// temp buffer for file strings
	int q;			// int variable for file ints

	// initializing the pointers and counters of our struct
	initInventory(inv);

	// checking if file exists and opening it for reading
	if ( (fp = fopen(filename, "r" )) ) {
		while(!feof(fp)) {	// looping until we find the EOF
			// allocating memory for the temporary buffer
			buffer = malloc(sizeof(char)*LINE_LEN);

			if (buffer == NULL) {
				perror("Allocation error -> buffer");
				exit(1);
			}
			
			// all data are stored like: string \t int
			if ( fscanf(fp, "%s \t %d", buffer, &q) < 0 ) {	// reading data from file
				free(buffer);
				break;	// the parameter list didn't meet our expectations
			}

			// Adding the record to our inventory
			newInventoryRecord(inv, buffer, q);

			// freeing the allocated memory
			free(buffer);
		}

		fclose(fp);	// closing it up

		return 0;	// no error occured
	} else {
		perror("Inventory problem");
		return 1;	// file was not found or couldn't open
	}
}

/*- ---------------------------------------------------------------- -*/
/**
 * @brief This function converts a string (that contains inventory data)
 * to an inventory struct. (This function is similar to the readInventory
 * function, but works with a string instead of a file)
 *
 * @param Takes in  the name, a string and an empty inventory (pointer)
 */
void parseStrIntoInv(char **name, char *str, Inventory *outInv) {
	int i;			 	// for counter
	int strIndex = 0;	// index for the string we are filling
	int gotName = 0; 	// flag for the name
	int gotTab = 0;		// flag for the tab
	int size = (int)strlen(str);	// getting the string length

	// using the buffers below to store the strings
	char *item = NULL;
	char *num = NULL;

	// initializing the name pointer to NULL
	*name = NULL;	

	// initializing our struct
	initInventory(outInv);

	// iterating through each char of the string
	for (i=0; i<size; ++i) {
		if ( (str[i] == '\n') && (strIndex == 0) ) {
			break;	// empty line (according to the rules the request has ended)
		}

		// parsing the name
		if (!gotName) {
			if (str[i] == '\n') {
				++gotName;		// setting the flag to 1
				
				// allocating space for the \0 char
				(*name) = realloc((*name), (++strIndex)*sizeof(char));
				
				if (*name == NULL) { exit(1); }	// couldn't reserve space 

				// terminating the string
				(*name)[strIndex-1] = '\0';

				// resetting the index for the next strings
				strIndex = 0;	
			} else {
				// allocating space for one more char
				(*name) = realloc((*name), (++strIndex)*sizeof(char));
				
				if (*name == NULL) { exit(1); }	// couldn't reserve space 

				// putting the char into the string
				(*name)[strIndex-1] = str[i];				
			}
		} else {
			// parsing the inventory
				// setting up for the next line
			if (str[i] == '\n') {
				// allocating space for one more char
				num = realloc(num, (++strIndex)*sizeof(char));
				
				if (num == NULL) { exit(1); }	// couldn't reserve space 

				// terminating the quantity string 
				num[strIndex-1] = '\0';

				// adding a new record to our struct with what we parsed
				newInventoryRecord(outInv, item, atoi(num));

				// free-ing the allocated buffers
				free(num);
				free(item);

				// setting their value to NULL for reuse
				num = NULL;
				item = NULL;

				--gotTab;			// resetting the tab flag
				strIndex = 0;		// reset the string index
			} else if (str[i] == '\t') {
				// allocating space for one more char
				item = realloc(item, (++strIndex)*sizeof(char));

				if (item == NULL) { exit(1); }	// couldn't reserve space 

				// terminating the string
				item[strIndex-1] = '\0';

				++gotTab;		// setting the flag to 1
				strIndex = 0;	// resetting the string index
			} else {
				if (gotTab) {
					// allocating space for one more char
					num = realloc(num, (++strIndex)*sizeof(char));

					if (num == NULL) { exit(1); }	// couldn't reserve space 

					// filling the quantity string
					num[strIndex-1] = str[i];
				} else {
					// allocating space for one more char
					item = realloc(item, (++strIndex)*sizeof(char));
					
					if (item == NULL) { exit(1); }	// couldn't reserve space 

					// filling the items string
					item[strIndex-1] = str[i];
				}
			}			
		}// if gotName
	}// for
}

/*- ---------------------------------------------------------------- -*/
/**
 * @brief This function converts an inventory to a string so that the
 * client can send his inventory to the server
 *
 * @param Takes in the players name and his inventory and parses it to a string
 */
void parseInvIntoStr(char *name, Inventory inv, char *str) {
	int i; 					// for counter
	char tab[] = "\t";		// tab seperator
	char newline[] = "\n";	// newline
	char numToS[LINE_LEN];	// number that we will convert to int

	// putting the name in the first line
	strcpy(str, name);
	strcat(str, newline);

	// concatenating the rest of the items
	for(i=0; i<inv.count; i++) {
		strcat(str, inv.items[i]);
		strcat(str, tab);
		sprintf(numToS, "%d", inv.quantity[i]);
		strcat(str, numToS);
		strcat(str, newline);
	}
}

/*- ---------------------------------------------------------------- -*/
/**
 * @brief Finds the index of an item stored in the inventory.
 * If the item can not be found we return 0
 *
 * @param Takes in an inventory struct
 *
 * @return returns 1 if item was found
 */
int findItem(Inventory inv, char *target, int *index) {
	int i;	// for counter

	for(i=0; i<inv.count; ++i) {
		if ( !strcmp(inv.items[i], target) ) {
			*index = i;	// keeping the position of the item in the array

			return 1;	// item found
		}
	}

	return 0;	// item was not found
}

/*- ---------------------------------------------------------------- -*/
/**
 * @brief Checks for dublicates in the given inventory
 *
 * @param Takes in an inventory struct
 *
 * @return 1 if dublicate was found or 0 if not
 */
int checkForDuplicates(Inventory inv) {
	int i, j;		// for counter
	int flag;	// flag for dublicates

	for (i=0; i<inv.count; ++i) {
		flag = 0;	// set the flag to zero at every loop

		for (j=0; j<inv.count; ++j) {
			if (inv.items[i] == inv.items[j]) {
				++flag;
			}
		}// j

		if (flag > 1) {
			return 1;	// found a duplicate entry
		}
	}// i

	return 0;
}

/*- ---------------------------------------------------------------- -*/
/**
 * @brief Subtracts two given inventories according to the rules of the
 * server. This function checks all necaissary conditions before registering
 * the changes to the room's inventory
 *
 * @param Takes in the room's and player's inventories and the max quota
 * set in the beginning
 *
 * @return 1 if subtraction took place or 0 if a problem occured
 */
int subInventories(Inventory *room, Inventory player, int *qData, int quota) {
	int i;			// for counter
	int pos = -1;	// position of the matched item
	int posArray[(player.count)];	// an array of indexes


	// checking if the player's inventory follows the rules
	// concerning the max quota
	if (player.quota > quota) {
		return 0;
	}

	// iterating through the player's items to check
	// that all items exist, while keeping their position
	// in the quantity array
	for (i=0; i<player.count; ++i) {	
		if (!findItem(*room, player.items[i], &pos)) {
			return 0;	// item was not found
		} else {
			// last check concerning the remaining quantity
			if ( (qData[pos]-player.quantity[i]) >= 0 ) {
				posArray[i] = pos;
			} else {
				return 0;	// asking too much :P
			}
		}
	}

	// registering the changes since everything went well
	for (i=0; i<player.count; ++i) {
			qData[posArray[i]] -= player.quantity[i];
	}

	// all good ...
	return 1;
}

/*- ---------------------------------------------------------------- -*/
/**
 * @brief Prints the info stored in a Inventory struvt
 *
 * @param Takes in an inventory struct
 */
void printInventory(Inventory inv) {
	int i = 0;	// for counter

	// printing our data in the appropriate format
	printf("Inventory: \n\n");
	for (i=0; i<inv.count; i++){
		printf("%s \t %d\n", inv.items[i], inv.quantity[i]);
	}

	printf("Quota: %d\n", inv.quota);

	printf("\n");
}

/*- ---------------------------------------------------------------- -*/

#endif