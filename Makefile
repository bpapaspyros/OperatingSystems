FLAGS=-Wall -Wextra
DEBUGFLAGS=-DDEBUG -g -O0
INCLUDES= -I.
LIBS=-lpthread
LL=gcc
CC=gcc $(INCLUDES) $(FLAGS)

all: GameServer	GameClient

debug: CC += $(DEBUGFLAGS)
debug: GameServer	GameClient

GameServer:	Server.o
	$(LL) $^ -o server $(LIBS)

GameClient: Client.o
	$(LL) $^ -o client $(LIBS)

Server.o: Server.c ServerBackend.h Inventory.h
	$(CC) Server.c -c -o Server.o

Client.o: Client.c ClientBackend.h Inventory.h
	$(CC) Client.c -c -o Client.o

# %.o: %.c SharedHeader.h
# 	$(CC) -c -o $@ $<

.PHONY:	clean

clean:
	rm -f test *.o	*.str server client