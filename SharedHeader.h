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

// defining port number
#define PORT_NO 55623


// struct containing the inventory data
typedef struct {
	char **items;
	int *quantity;
	int count;
}Inventory;


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

	// checking if file exists and opening it for reading
	if ( fp = fopen(filename, "r" ) ) {
		while(!feof(fp)) {	// looping until we find the EOF
			// all data are stored like: string \t int
			fscanf(fp, "%s \t %d", buffer, &q);	// reading data from file

			// allocating memory for our arrays
			if (inv->count == 0) {	// allocating memory for the first time
				inv->quantity = (int *)malloc( sizeof(int));

				// 2d array allocation (practically an array of strings)
				inv->items = (char **)malloc( sizeof(char *));
				inv->items[inv->count] = (char *)malloc( sizeof(char));
			} else {				// reallocating memory
				inv->quantity = (int *)realloc(inv->quantity, (inv->count+1)*sizeof(int));
				
				// 2d array re allocations
				inv->items = (char **)realloc(inv->items, (inv->count+1)*sizeof(char *));
				inv->items[inv->count] = (char *)realloc(inv->items[inv->count], (strlen(buffer)+1)*sizeof(char));
			}

			// assigning the values tha were read to the struct
			inv->quantity[inv->count] = q;
			strcpy(inv->items[inv->count], buffer);

			// increasing the counter since we added a new item
			inv->count = inv->count +1;
		}

		fclose(fp);	// closing it up

		return 0;	// no error occured
	} else {
		perror("Inventory problem");
		return 1;	// file was not found or couldn't open
	}
}

	
/**
 * @brief Prints the info stored in a Inventory struvt
 *
 * @param Takes in an inventory struct
 */
void printInventory(Inventory *inv) {
	int i = 0;	// for counter

	// printing our data in the appropriate format
	printf("Inventory: \n\n");
	for (i=0; i<inv->count; i++){
		printf("%s \t %d\n", inv->items[i], inv->quantity[i]);
	}
}