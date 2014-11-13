CXX=gcc
CXXFLAGS=-Wall -Wextra
DEBUG=-g


all:	Server.o 
	$(CXX) $^ -o test

.cpp.o:
	$(CXX) -c $(CXXFLAGS) $< -o $@

.PHONY:	clean

clean:
	rm -f test *.o