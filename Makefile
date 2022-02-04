# When building a package or installing otherwise in the system, make
# sure that the variable PREFIX is defined, e.g. make PREFIX=/usr/local
PROGNAME=dump1090

ifdef PREFIX
BINDIR=$(PREFIX)/bin
SHAREDIR=$(PREFIX)/share/$(PROGNAME)
EXTRACFLAGS=-DHTMLPATH=\"$(SHAREDIR)\"
endif

#CFLAGS=-O2 -g -Wall -W `pkg-config --cflags librtlsdr`
CFLAGS=-O2 `pkg-config --cflags librtlsdr`
LIBS=`pkg-config --libs librtlsdr` -lpthread -lm -lcurl
CC=gcc

all: flight-tracker
# tell Make how to prepare .o-files
%.o: %.c
	$(CC) $(CFLAGS) $(EXTRACFLAGS) -c $<
# tell Make that dump1090 depends on listed o.-files
# whenever there's a change in any of these object files, make will take action
flight-tracker: dump1090.o interactive.o mode_ac.o mode_s.o data.o
	$(CC) -g -o $@ dump1090.o interactive.o mode_ac.o mode_s.o data.o $(LIBS) $(LDFLAGS)

clean:
	rm -f *.o flight-tracker
