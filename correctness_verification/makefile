TESTS = output
CC = g++
CFLAGS = -g -Wall -O3 -pthread -D_GNU_SOURCE
LDLIBS = -lpthread -lm
INCLUDES = -I.

all: $(TESTS)

output: WRL_queue.cpp
		$(CC) $(CFLAGS) ${INCLUDES} -o $@ $^

clean:
	rm -f $(TESTS) *.o