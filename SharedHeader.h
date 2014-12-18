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
#define PORT_NO 55623
#define SHM_SIZE  270			// defining the shared memory size
char SEM_NAME[]= "gameservers";	// shared memory name

// struct containing the inventory data
typedef struct {
	char items[1024][32];
	int quantity[1024];
	int count;
	int quota;
}Inventory;

/*- ---------------------------------------------------------------- -*/
/**
 * @brief This function reads through the given inventory
 * file and adds the data given to a struct so that we don't
 * have to read through the file all the time
 *
 * @param Takes in the filename and an instance of an inventory 
 * struct
 *
 * @return Returns 0 if the function ran correctly and 1 if not
 */
int readInventory(char *filename, Inventory *inv) {
	FILE *fp;			// file pointer
	char buffer[32];	// temp buffer for file strings
	int q;				// int variable for file ints

	// initializing the counter of the items to zero
	inv->count = 0;
	inv->quota = 0;

	// checking if file exists and opening it for reading
	if ( (fp = fopen(filename, "r" )) ) {
		while(!feof(fp)) {	// looping until we find the EOF
			// all data are stored like: string \t int
			if ( fscanf(fp, "%s \t %d", buffer, &q) < 0) {	// reading data from file
				break;	
			}

			// assigning the values tha were read to the struct
			inv->quantity[inv->count] = q;
			strcpy(inv->items[inv->count], buffer);

			// increasing the counter since we added a new item
			inv->count++;
			inv->quota += q;
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
void parseStrIntoInv(char *name, char *str, Inventory *outInv) {
	int i;			 	// for counter
	int strIndex = 0;	// index for the string we are filling
	int gotName = 0; 	// flag for the name
	int gotTab = 0;		// flag for the tab
	int size = (int)strlen(str);	// getting the string length
	char num[32];		// buffer for the quantity before we parse it to int

	// setting the struct item and quota counter to 0
	outInv->count = 0;
	outInv->quota = 0;

	// iterating through each char of the string
	for (i=0; i<size; ++i) {
		if ( (str[i] == '\n') && (strIndex == 0) ) {
			break;	// empty line (according to the rules the request has ended)
		}

		// parsing the name
		if (!gotName) {
			if (str[i] == '\n') {
				++gotName;				// setting the flag to 1
				name[strIndex] = '\0';	// terminating the string
				strIndex = 0;			// resetting the index for the next strings
			} else {
				name[strIndex++] = str[i];	// filling the string with chars				
			}
		} else {
			// parsing the inventory
				// setting up for the next line
			if (str[i] == '\n') {
				// terminating the quantity string 
				num[strIndex] = '\0';

				// updating the struct with the quantity that we read
				outInv->quantity[(outInv->count)] = atoi(num);
				outInv->quota += outInv->quantity[(outInv->count)];	// updating the quota var

				++(outInv->count);	// increasing the item counter
				--gotTab;			// resetting the tab flag
				strIndex = 0;		// reset the string index
			} else if (str[i] == '\t') {
				// terminating the current string
				outInv->items[(outInv->count)][strIndex] = '\0';

				++gotTab;		// setting the flag to 1
				strIndex = 0;	// resetting the string index
			} else {
				if (gotTab) {
					// filling the quantity string
					num[strIndex++] = str[i];
				} else {
					// filling the items string
					outInv->items[(outInv->count)][strIndex++] = str[i];
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
void parseInvIntoStr(char *name, Inventory *inv, char *str) {
	int i; 					// for counter
	char tab[] = "\t";		// tab seperator
	char newline[] = "\n";	// newline
	char numToS[20];		// number that we will convert to int

	// putting the name in the first line
	strcpy(str, name);
	strcat(str, newline);

	// concatenating the rest of the items
	for(i=0; i<inv->count; i++) {
		strcat(str, inv->items[i]);
		strcat(str, tab);
		sprintf(numToS, "%d", inv->quantity[i]);
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

	for(i=0; i<inv.count; i++) {
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

	for (i=0; i<inv.count; i++) {
		flag = 0;	// set the flag to zero at every loop

		for (j=0; j<inv.count; j++) {
			if (inv.items[i] == inv.items[j]) {
				flag++;
			}
		}// j

		if (flag > 1) {
			return 1;
		}
	}// i

	return 0;
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