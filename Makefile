CXX=gcc 
CXXFLAGS=-Wall -Wextra

all: GameServer	GameClient

GameServer:	Server.o 
	$(CXX) $^ -o test_server -lpthread

Server.o: Server.c 	ServerBackend.h	SharedHeader.h
	@ $(CXX) -c Server.c -lpthread

GameClient: Client.o
	$(CXX) $^ -o test_client -lpthread

Client.o: Client.c ClientBackend.h SharedHeader.h
	@ $(CXX) -c Client.c -lpthread

.PHONY:	clean

clean:
	rm -f test *.o	*.str test_client test_server