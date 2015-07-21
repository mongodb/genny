SHELL = /bin/sh
.SUFFIXES:
.SUFFIXES: .c .cpp .o

CPP=g++
CPPFLAGS=-l yaml-cpp -std=c++11 -I/usr/local/include/mongocxx/v0.3 -I/usr/local/include/libmongoc-1.0 -I/usr/local/include/bsoncxx/v0.3 -I/usr/local/include/libbson-1.0 -L/usr/local/lib -lmongocxx -lbsoncxx  -lbson-1.0 

query.o : query.cpp query.hpp 

insert.o : insert.cpp insert.hpp

parse_util.o : parse_util.cpp parse_util.hpp

mwg : parser.cpp query.o parse_util.o
	$(CPP) $<  -I.  $(CPPFLAGS) -o $@ query.o parse_util.o 

