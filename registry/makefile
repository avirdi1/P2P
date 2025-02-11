# Angel Garcia
# Anmol Virdi
# EECE 446
# Fall 2024

EXE = registry
CFLAGS = -Wall
CXXFLAGS = -Wall
LDLIBS = 
CC = gcc
CXX = g++

.PHONY: all
all: $(EXE)

registry: registry.c
	$(CC) $(CFLAGS) registry.c $(LDLIBS) -o $(EXE)

.PHONY: clean
clean:
	rm -f $(EXE)

