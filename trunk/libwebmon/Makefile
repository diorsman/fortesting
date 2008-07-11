CC=gcc
CFLAGS=-DCOMPRESS -D_GNU_SOURCE -g -O2 -Wall
#CFLAGS=-DCOMPRESS -D_GNU_SOURCE -g -Wall
LDFLAGS=
AR=ar
OBJS=webmon.o defgraphs.o str.o chart.jar.o
LIBS=-lz


all: test

test: test.o libwebmon.a
	${CC} ${LDFLAGS} -o test test.o libwebmon.a ${LIBS}

test.o: test.c webmon.h
	${CC} ${CFLAGS} -c test.c

libwebmon.a: ${OBJS}
	${AR} -r libwebmon.a ${OBJS}

webmon.o: webmon.c defgraphs.h str.h webmon.h
	${CC} ${CFLAGS} -c webmon.c

defgraphs.o: defgraphs.c webmon.h defgraphs.h
	${CC} ${CFLAGS} -c defgraphs.c

str.o: str.c str.h
	${CC} ${CFLAGS} -c str.c

chart.jar.o: chart.jar.c
	${CC} ${CFLAGS} -c chart.jar.c

clean: 
	rm -f *.o test libwebmon.a
