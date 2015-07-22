SHELL = /bin/sh
.SUFFIXES:
.SUFFIXES: .c .cpp .o

CPP=g++
CPPFLAGS=-l yaml-cpp -std=c++11 -I/usr/local/include/mongocxx/v0.3 -I/usr/local/include/libmongoc-1.0 -I/usr/local/include/bsoncxx/v0.3 -I/usr/local/include/libbson-1.0 -lmongocxx -lbsoncxx  -lbson-1.0 

default : mwg

OBJS = query.o parse_util.o insert.o workload.o node.o random_choice.o sleep.o

mwg : parser.cpp $(OBJS)
	$(CPP) $<  -I.  $(CPPFLAGS) -o $@ $(OBJS)

clean : 
	rm -rf *.o *.d mwg
