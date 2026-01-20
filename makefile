# Makefile
CC = gcc
CFLAGS = -std=c99 -Wall -Werror -pedantic-errors -DNDEBUG -g
LDFLAGS = -pthread

# Targets
all: main

main: main.o customAllocator.o
	$(CC) $(CFLAGS) -o main main.o customAllocator.o $(LDFLAGS)

main.o: main_old.c customAllocator.h
	$(CC) $(CFLAGS) -c main.c

customAllocator.o: customAllocator.c customAllocator.h
	$(CC) $(CFLAGS) -c customAllocator.c

clean:
	rm -f *.o main