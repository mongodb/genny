SHELL = /bin/sh
.SUFFIXES:
.SUFFIXES: .c .cpp .o

CC=g++
CCFLAGS=-l yaml-cpp -std=c++11

mwg : parser.cpp
	$(CC) $< -I.  $(CCFLAGS) -o $@
