CXXFLAGS=-g -std=c++0x -Wall -Werror -pedantic -Wno-vla
LDLIBS=-lboost_program_options -lboost_thread -lvirt -ltinyxml -lcurl

all: actuator rm httpmon

actuator: actuator.o

rm: rm.o VirtualManager.o

httpmon: httpmon.cc

clean:
	rm -f *.o actuator

