CC=gcc
CFLAGS=-D_GNU_SOURCE -g -O2 -Wall
#CFLAGS=-D_GNU_SOURCE -g -Wall
LDFLAGS=-static
OBJS=
LIBS=-lwebmon -lz


all: statusd

statusd: main.o
	${CC} ${LDFLAGS} -o statusd main.o ${LIBS}

main.o: main.c
	${CC} ${CFLAGS} -c main.c

clean: 
	rm -f *.o statusd 
