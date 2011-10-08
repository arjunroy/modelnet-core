SOURCES = assignment.cpp main.cpp weight.cpp flow.cpp util.cpp
OBJECTS = $(SOURCES:%.cpp=%.o)
INCLUDE_DIR = -I.. -I. -I./chaco -I./metis
LIBDIR = -L./metis -L./chaco -L../
LIBS = -lmngraph -lxerces-c -lmetis -lchaco -lm -lpopt
CC = g++
CPPFLAGS = -g -Wall -O2

%.o : %.cpp
	$(CC) $(INCLUDE_DIR) $(CPPFLAGS) -c $< -o $@

tools: all

all: program

program: $(OBJECTS)
	$(CC) $(OBJECTS) -o assign  ../xml.o $(CPPFLAGS) $(LIBDIR)  $(LIBS)

clean:
	rm -f *.o assign
