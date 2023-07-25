CC = gcc
CFLAGS = -Wall -Wextra -g
LIBS = -lpthread -lm

all: multi-lookup

multi-lookup: 
	$(CC) $(CFLAGS) -o multi-lookup multi-lookup.c util.c $(LIBS)

clean:
	rm -f multi-lookup core