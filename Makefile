SHELL = /bin/sh
.SUFFIXES:
.SUFFIXES: .c .cpp .o

CPP=g++
CPPFLAGS=-l yaml-cpp -std=c++11 -I/usr/local/include/mongocxx/v0.3 -I/usr/local/include/libmongoc-1.0 -I/usr/local/include/bsoncxx/v0.3 -I/usr/local/include/libbson-1.0 -lmongocxx -lbsoncxx  -lbson-1.0 

default : mwg

query.o : query.cpp query.hpp 

insert.o : insert.cpp insert.hpp

parse_util.o : parse_util.cpp parse_util.hpp

workload.o : workload.cpp workload.hpp

node.o :node.cpp node.hpp

random_choice.o : random_choice.cpp random_choice.hpp

sleep.o : sleep.cpp sleep.hpp

mwg : parser.cpp query.o insert.o parse_util.o workload.o node.o random_choice.o sleep.o
	$(CPP) $<  -I.  $(CPPFLAGS) -o $@ query.o parse_util.o insert.o workload.o node.o random_choice.o sleep.o

