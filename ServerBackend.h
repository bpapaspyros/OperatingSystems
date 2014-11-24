#ifndef SERVERBACKEND_H
#define SERVERBACKEND_H

// Structs
typedef struct {
	int players;
	int quota;
	char inventory[16];
}Settings;

typedef struct {
	int room_count;
	int slots;
}GameStatus;

/*- ---------------------------------------------------------------- -*/
/**
 * @brief Initializes the the settings struct according
 * to the parameters given for later use by the server
 *
 * @param Takes in the number of parameters as well as the 
 * parameter array that was passed in to the main function
 * plus the settings and game status structs
 */
 void initSettings(int argc, char **argv, Settings *s, GameStatus *gs) {
	int i;	// for counter
	
	// flags we raise when we get the necessary parameters
	int gotP = 0;
	int gotQ = 0;
	int gotI = 0;

	// managing invalid parameter input
	if (argc != 7) {
		printf("Invalid parameters. Exiting ... \n");
		exit(1);		
	}

	// parsing arguments
	for(i=1; i<argc; i+=2) {
		if ( !strcmp(argv[i], "-p") && gotP == 0 ) {
			s->players = atoi(argv[i+1]);
			gotP = 1; 
		} else if ( !strcmp(argv[i], "-q") && gotQ == 0 ) {
			s->quota = atoi(argv[i+1]);
			gotQ = 1;
		} else if ( !strcmp(argv[i], "-i") && gotI == 0 ) {
			strcpy(s->inventory, argv[i+1]);
			gotI = 1;
		} else {
			break;
		}
	} // for

	// checking if we got everything we need
	if (gotP && gotQ && gotI) {
		// setting up the status 
		gs->room_count = 1;
		gs->slots = s->players;

		// printing the settings that were read
		printf("\n\t Settings for this game: \n\n");
		printf("\t Players: %d \n", s->players);
		printf("\t Inventory per player: %d \n", s->quota);
		printf("\t Using %s as inventory file\n\n", s->inventory);
	} else {
		printf("Invalid or missing parameters. Exiting ... \n");
		exit(1);
	}
}
/*- ---------------------------------------------------------------- -*/

#endif