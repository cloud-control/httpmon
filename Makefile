CXXFLAGS=-g -std=c++0x -Wall -Werror -pedantic -Wno-vla
LDLIBS=-lboost_program_options -lvirt -ltinyxml

all: actuator rm

actuator: actuator.o

rm: rm.o VirtualManager.o

clean:
	rm -f *.o actuator

