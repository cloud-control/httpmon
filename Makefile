CXXFLAGS=-g -std=c++0x -Wall -Werror -pedantic -Wno-vla
LDLIBS=-lboost_program_options -lboost_thread -lvirt -ltinyxml -lcurl

all: httpmon

httpmon: httpmon.cc

clean:
	rm -f *.o httpmon

