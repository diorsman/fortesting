CC=gcc
CFLAGS=-g -O2 -Wall
#CFLAGS=-g -Wall
LDFLAGS=
OBJS=httpd.o
LIBS=-lwebmon -lz


all: testhttpd

testhttpd: ${OBJS}
	${CC} ${LDFLAGS} -o testhttpd ${OBJS} ${LIBS}

httpd.o: httpd.c
	${CC} ${CFLAGS} -c httpd.c


clean: 
	rm -f *.o testhttpd
