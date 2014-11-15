#ifndef CLIENTBACKEND_H
#define CLIENTBACKEND_H

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Structs
typedef struct {
	char name[10];
	char inventory[16];
	char host_name[16];
}cSettings;

/*- ---------------------------------------------------------------- -*/
/**
 * @brief Initializes the the settings struct according
 * to the parameters given for later use by the client
 *
 * @param Takes in the number of parameters as well as the 
 * parameter array that was passed in to the main function
 */
 void initcSettings(int argc, char **argv, cSettings *s) {
	int i;					// for counter
	
	// flags we raise when we get the necessary parameters
	int gotN = 0;
	int gotI = 0;
	int gotH = 0;

	// managing invalid parameter input
	if (argc != 6) {
		printf("Invalid parameters. Exiting ... \n");
		exit(1);		
	}

	// parsing arguments
	for(i=1; i<argc; i+=2) {
		if ( !strcmp(argv[i], "-n") && gotN == 0 ) {
			strcpy(s->name, argv[i+1]);
			gotN = 1; 
		} else if ( !strcmp(argv[i], "-i") && gotI == 0 ) {
			strcpy(s->inventory, argv[i+1]);
			gotI = 1;
		} else if ( gotH == 0 ) {
			strcpy(s->host_name, argv[i--]);
			gotH = 1;			
		} else {
			break;
		}
	} // for

	// checking if we got everything we need
	if (gotN && gotI && gotH) {
		printf("\n\t Settings for this player: \n\n");
		printf("\t Name: %s \n", s->name);
		printf("\t Inventory selection: %s \n", s->inventory);
		printf("\t Host name: %s \n\n", s->host_name);
	} else {
		printf("Invalid or missing parameters. Exiting ... \n");
		exit(1);
	}
}
#endif