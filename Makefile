#
# Makefile for DAMSON
#

# Clear the list of suffixes
.SUFFIXES:
# Default compilation Suffix rules for .c .o
.SUFFIXES: .c .o

#
# Macros
#

CC = gcc
GCC_OPTIONS = -Wall -pg -std=c99

OBJECTS = damson.o compiler.o emulator.o codegen.o debug.o

#
# Targets
#

### damson program

damson: $(OBJECTS) 
	$(CC) -pg -o $@ $(OBJECTS) -lelf


### SUFFIX rule statement

%.o : %.c
	$(CC) $(GCC_OPTIONS) -c $<


### Clean up

clean:
	rm *.o damson

