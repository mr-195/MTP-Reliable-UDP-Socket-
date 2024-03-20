CC=gcc
CFLAGS=-Wall

user: user1 user2

user1: user1.o libmsocket.a
	$(CC) $(CFLAGS) -o user1 user1.o -L. -lmsocket -lpthread

user1.o: user1.c
	$(CC) $(CFLAGS) -c user1.c

user2: user2.o libmsocket.a
	$(CC) $(CFLAGS) -o user2 user2.o -L. -lmsocket -lpthread

user2.o: user2.c
	$(CC) $(CFLAGS) -c user2.c

libmsocket.a: 
			make -f makefile.libmsocket

clean:
	rm -f user1 user2 user1.o user2.o
	make -f makefile.libmsocket clean
