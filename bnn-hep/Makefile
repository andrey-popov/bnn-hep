# Define the flags to control make
CC = g++
INCLUDE = -Iinclude -I../libconfig/ -I$(shell root-config --incdir) -I/afs/cern.ch/sw/lcg/external/Boost/1.50.0_python2.7/x86_64-slc5-gcc46-opt/include/boost-1_50/
OPFLAGS = 
CFLAGS = -Wall -Wextra -std=c++11 $(INCLUDE) $(OPFLAGS)
LDFLAGS = $(shell root-config --libs) -lTreePlayer -L/afs/cern.ch/sw/lcg/external/Boost/1.50.0_python2.7/x86_64-slc5-gcc46-opt/lib/ -lboost_filesystem-gcc46-mt-1_50 -lboost_system-gcc46-mt-1_50
EXECUTABLE = bnn-hep
SOURCES = $(shell ls src/ | grep .cpp)
OBJECTS = $(SOURCES:.cpp=.o)
# See http://www.gnu.org/software/make/manual/make.html#Substitution-Refs

# Define the search paths for the targets and dependencies. See
# http://www.gnu.org/software/make/manual/make.html#Directory-Search
vpath %.hpp include
vpath %.cpp src

# Define the phony targets
.PHONY: clean

# The default rule
all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $+ ../libconfig/libconfig++.a -o $@
# '$@' is expanded to the target, '$+' expanded to all the dependencies. See
# http://www.gnu.org/savannah-checkouts/gnu/make/manual/html_node/Automatic-Variables.html

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@
# '$<' is expanded to the first dependency

clean:
	@rm -f *.o
# '@' prevents the command from being printed to stdout
