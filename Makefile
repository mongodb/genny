SHELL = /bin/sh
.SUFFIXES:
.SUFFIXES: .c .cpp .o

CC=g++
CCFLAGS=-l yaml-cpp -std=c++11 -I/usr/local/include/mongocxx/v0.3 -I/usr/local/include/libmongoc-1.0 -I/usr/local/include/bsoncxx/v0.3 -I/usr/local/include/libbson-1.0 -L/usr/local/lib -lmongocxx -lbsoncxx 

mwg : parser.cpp
	$(CC) $< -I.  $(CCFLAGS) -o $@
