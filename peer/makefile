# Anmol Virdi
# Angel Garcia
# EECE 446
# Fall 2024

EXE = peer
CFLAGS = -Wall
CXXFLAGS = -Wall
LDLIBS = 
CC = gcc
CXX = g++

.PHONY: all
all: $(EXE)

peer: peer.c
	$(CC) $(CFLAGS) peer.c $(LDLIBS) -o $(EXE)

.PHONY: clean
clean:
	rm -f $(EXE)

