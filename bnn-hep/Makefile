# Variables to work with Boost
BOOST_INCLUDE = $(BOOST_ROOT)/include/boost-1_55
BOOST_LIB = $(BOOST_ROOT)/lib
BOOST_LIB_POSTFIX = -gcc47-mt-1_55

# Define the flags to control make
CC = g++
INCLUDE = -Iinclude -I../libconfig/ -I$(shell root-config --incdir) -I$(BOOST_INCLUDE)
OPFLAGS = 
CFLAGS = -Wall -Wextra -Wno-unused-local-typedefs -std=c++11 $(INCLUDE) $(OPFLAGS)
LDFLAGS = $(shell root-config --libs) -lTreePlayer \
 -L$(BOOST_LIB) -lboost_filesystem$(BOOST_LIB_POSTFIX) -lboost_system$(BOOST_LIB_POSTFIX) \
 -lboost_filesystem$(BOOST_LIB_POSTFIX) -lboost_system$(BOOST_LIB_POSTFIX) \
 -Wl,-rpath=$(BOOST_LIB)
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
