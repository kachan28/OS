CC=gcc
CFLAGS=-c -Wall

all: arhivator

arhivator: main.o 
	$(CC) main.o -o main

main.o: main.c
	$(CC) $(CFLAGS) main.c

clean:
	rm -rf *.o