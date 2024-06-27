CC=gcc
CFLAGS=-g -Wall
target=testlib

objects=bitmap.o debug.o hash.o hex_dump.o list.o main.o 
sources=bitmap.c debug.c hash.c hex_dump.c list.c main.c

$(target): $(objects) bitmap.h debug.h hash.h hex_dump.h list.h round.h limits.h main.h
	$(CC) -o $@ $(objects)

clean:
	rm -f *.o
	rm -f $(target)

run:
	./$(TARGET)
