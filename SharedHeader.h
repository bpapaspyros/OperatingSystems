#include <stdio.h>		// standard i/o
#include <unistd.h>		// declaration of numerous functions and macros
#include <sys/socket.h> // socket definitions
#include <sys/types.h> 	// system data types
#include <sys/un.h> 	// Unix domain sockets
#include <errno.h>		// EINTR variable
#include <sys/wait.h>	// waitpid system call
#include <sys/un.h> 	// Unix domain sockets

// socket name and path
char SRV_PATH[20] = "./gameserver.str";