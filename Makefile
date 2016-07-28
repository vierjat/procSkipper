CFITSIO = $(FITSIOROOT)
CPP = g++
CC = gcc
CFLAGS = -Wall -I$(CFITSIO) 
LIBS = -L$(CFITSIO) -lcfitsio -lm
GLIBS = 
GLIBS += 
OBJECTS = procSkipper.o 
HEADERS = globalConstants.h

ALL : procSkipper.exe
	@echo "Listo!"

procSkipper.exe : $(OBJECTS)
	$(CPP) $(OBJECTS) -o procSkipper.exe $(LIBS) $(GLIBS) $(CFLAGS)

procSkipper.o : procSkipper.cc $(HEADERS)
	$(CPP) -c procSkipper.cc -o procSkipper.o $(CFLAGS)

clean:
	rm -f *~ *.o *.exe
