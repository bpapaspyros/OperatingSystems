# Operating Systems

Basic game server following the server-client model. Implemented by:

  - Forking processes
  - Opening threads

The server is supposed to assign each player to an available game room where the players can chat amongst them. The server has an inventory with items that a player can pick. Likewise the player has an inventory with items that he selects.

### Compile and compile options

To compile and lin the the source code to the binaries simply run:

```sh
make
```

* Links to lpthread

### Server parameters

To run the server properly you need to set 3 variables, the inventory file, the number of players per room and the maximum quota of items is player can select.

* Server call:

```sh
./server -p <player number> -q <quota/player> -i <inventory file>
```

### Client parameters

To run the client properly you need to set 3 variables, the inventory file, a name and the server hostname.

* Client call:

```sh
./client -n <name> -i <inventory file> <hostname>
```
### Sample call:

```sh
./server -p 5 -q 4 -i srvInventory1.dat
```

```sh
./client -n p1 -i cliInventory1.dat $(hostanme)
```
