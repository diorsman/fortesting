#CROSS=/opt/buildroot/build_i386/staging_dir/usr/bin/i386-linux-
CROSS=
CC=${CROSS}gcc
CFLAGS=-g -O2 -Wall
LDFLAGS=-static
LIBS=-lwebmon -lz -lpthread

all: client server

client: client.o common.o
	${CC} ${LDFLAGS} -o client client.o common.o ${LIBS}

client.o: client.c common.h
	${CC} ${CFLAGS} -c client.c

server: server.o common.o
	${CC} ${LDFLAGS} -o server server.o common.o ${LIBS}

server.o: server.c common.h
	${CC} ${CFLAGS} -c server.c

common.o: common.c common.h
	${CC} ${CFLAGS} -c common.c

clean: 
	rm -f *.o client server
