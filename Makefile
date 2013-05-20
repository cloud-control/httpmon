CXXFLAGS=-g -std=c++0x -Wall -Werror -pedantic -Wno-vla
LDLIBS=-lboost_program_options -lvirt -ltinyxml

all: actuator

actuator: actuator.o

clean:
	rm -f *.o actuator

