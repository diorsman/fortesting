CC=gcc
CFLAGS=-g -O2 -Wall
#CFLAGS=-g -Wall
LDFLAGS=-static
OBJS=httpd.o
LIBS=-lwebmon -lpthread -lz


all: testhttpd httpc

testhttpd: ${OBJS}
	${CC} ${LDFLAGS} -o testhttpd ${OBJS} ${LIBS}

httpd.o: httpd.c
	${CC} ${CFLAGS} -c httpd.c

httpc: httpc.o
	${CC} ${LDFLAGS} -o httpc httpc.o

httpc.o: httpc.c
	${CC} ${CFLAGS} -c httpc.c

clean: 
	rm -f *.o testhttpd httpc
