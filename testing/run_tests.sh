# !/bin/bash



function test1() {
	# Test 1
	clear && echo "Test 1 - Lasts about 35 seconds - Try the chat"
	sleep 2;

	# starting the server
	(x-terminal-emulator -e timeout --signal=SIGINT 35s ../server "-p" "3" "-q" "4" "-i" "server1.dat") &

	# wait for a sec
	sleep 1;

	# starting 3 clients
	(x-terminal-emulator -e timeout --signal=SIGINT 30s ../client "-n" "Baios P"   "-i" "client1.dat" "$(hostname)") &
	(x-terminal-emulator -e timeout --signal=SIGINT 30s ../client "-n" "John Doe"  "-i" "client2.dat" "$(hostname)") &
	(x-terminal-emulator -e timeout --signal=SIGINT 30s ../client "-n" "Jane Doe"  "-i" "client3.dat" "$(hostname)") &

	# wait for test 1 to end
	sleep 35;
}

function test2 {
	# Test 2 
	clear && echo "Test 2 - Lasts about 20 seconds - Opening Multiple game rooms (1 players each)"
	sleep 5;

	# starting the server
	(x-terminal-emulator -e timeout --signal=SIGINT 10s ../server "-p" "1" "-q" "4" "-i" "server1.dat") &

	# wait for a sec
	sleep 1;
	for i in {1..10}
	do 
		(x-terminal-emulator -e timeout --signal=SIGINT 3s ../client "-n" "$i" "-i" "client1.dat" "$(hostname)") &
	done

	sleep 5;
	clear && echo "Check that all shared memory segments are removed !"
	sleep 5;
	ipcs -m;

	sleep 5;
}

function test3 {
	# Test 3
	clear && echo "Test 3 - Lasts about 15 seconds - Inventory tests"
	echo "		Adding 2 users repeatedly that will try to exhaust the same item to check synchronisation"
	echo "		Third player just fills the spot left in the 2 player room"
	sleep 5;

	# starting the server
	(x-terminal-emulator -e timeout --signal=SIGINT 40s ../server "-p" "2" "-q" "100" "-i" "server1.dat") &

	sleep 1;
	for i in {1..10}
	do 
		(x-terminal-emulator -e timeout --signal=SIGINT 3s ../client "-n" "1" "-i" "client4.dat" "$(hostname)") &
		(x-terminal-emulator -e timeout --signal=SIGINT 3s ../client "-n" "2" "-i" "client4.dat" "$(hostname)") &
		
		(x-terminal-emulator -e timeout --signal=SIGINT 3s ../client "-n" "3" "-i" "client3.dat" "$(hostname)") &

		sleep 1;
	done

	clear && echo "* Check the server messages to see that players connect randomly depending on who reached the semaphore first"
	echo "* See how players 1 and 2 that tried to exhaust the same item, never end up in the same room"
	echo "* The server window will remain open for 30 seconds, so that you can check the results"
}

# checking if the user wants a specific test
if [ $# -eq 0 ]
then
	# printing instructions
	clear && echo "Sample tests - From now on keep this terminal on top of the others ..."
	echo " - Many terminals will flash during this test"
	echo " - Try to focus a this terminal and the one that is left open"
	echo " - All terminals will timeout ! No need for you to close them or you might get undefined behaviour"

	echo "Tests start in"

	for i in {15..1}
	do
		echo "$i"
		sleep 1;
	done

	# calling all the tests
	test1
	test2
	test3

elif [ $1 == 1 ] 
then
	# running test 1
	test1
elif [ $1 == 2 ] 
then
	# running test 2
	test2
elif [ $1 == 3 ] 
then
	# running test 3
	test3
fi


echo " "
echo "Tests finished !"