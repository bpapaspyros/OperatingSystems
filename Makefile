CXX=gcc
CXXFLAGS=-Wall -Wextra
DEBUG=-g


all: GameServer	GameClient

GameServer:	Server.o 
	$(CXX) $^ -o test_server

Server.o: Server.c 	ServerBackend.h
	@ $(CXX) -c Server.c

GameClient: Client.o
	$(CXX) $^ -o test_client

Client.o: Client.c ClientBackend.h
	@ $(CXX) -c Client.c 

.PHONY:	clean

clean:
	rm -f test *.o