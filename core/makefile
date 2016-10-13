.PHONY: all clean
CC=gcc
CPP=g++
AR=ar
CFLAGS= -g -Wall -Wno-unused-function
C11FLAGS= -g -Wall -Wno-unused-function -std=c++11
SRCDIR = .
IDIR = ./include
ODIR = obj
INCLUDEDIR = -I$(IDIR) -I.
DEPS =
LIBDIR = -L./lib
LIBS = -lenet
BINDIR = ./bin
_OBJ = 
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))
PROGRAMS = forwarder

all: $(PROGRAMS)


forwarder: $(SRCDIR)/main.cpp $(OBJ)
	$(CPP) $(C11FLAGS) -o $(BINDIR)/$@ $^ $(INCLUDEDIR) $(LIBDIR) $(LIBS) 

clean:
	rm -rf $(PROGRAMS) *.dSYM *.o

