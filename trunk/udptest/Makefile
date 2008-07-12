#CROSS=/opt/buildroot/build_i386/staging_dir/usr/bin/i386-linux-
CROSS=
CC=${CROSS}gcc
CFLAGS=-g -O2 -Wall
LDFLAGS=-static
LIBS=-lwebmon -lz -lpthread

all: client server

client: client.o
	${CC} ${LDFLAGS} -o client client.o ${LIBS}

client.o: client.c
	${CC} ${CFLAGS} -c client.c

server: server.o
	${CC} ${LDFLAGS} -o server server.o ${LIBS}

server.o: server.c
	${CC} ${CFLAGS} -c server.c

clean: 
	rm -f *.o client server
