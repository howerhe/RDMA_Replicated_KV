CC=gcc
CFLAGS=-g -O3

all: server client
	rm *.o

ht.o: ht.c
	${CC} ${CFLAGS} -fPIC -c $<;

rdma.o: rdma.c
	${CC} ${CFLAGS} -fPIC -c $<;

sokt.o: sokt.c
	${CC} ${CFLAGS} -fPIC -c $<;

server: server.c parameters.h ht.o rdma.o sokt.o
	${CC} ${CFLAGS} -c $<;
	${CC} server.o ht.o rdma.o sokt.o -libverbs -o server

client: client.c parameters.h ht.o sokt.o
	${CC} ${CFLAGS} -c $<;
	${CC} client.o ht.o sokt.o -o client

clean:
	rm server client