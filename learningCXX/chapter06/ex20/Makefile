CXX=g++
CXXFLAGS=-g -Wall
LD=g++
LDFLAGS=
OBJS=
LIBS=

all: test

test: test.o ${OBJS}
	${LD} ${LDFLAGS} -o test test.o ${OBJS} ${LIBS}

test.o: test.cpp
	${CXX} ${CXXFLAGS} -c test.cpp

clean:
	rm -f *.o test
