TESTS = wfqueue faa lock waitfree waitfreeup test jiffy

CC = gcc
CFLAGS = -g -Wall -O3 -pthread -D_GNU_SOURCE 
LDLIBS = -lpthread -lm

all: $(TESTS)

wfqueue: wfqueue.o
	$(CC) $(CFLAGS) -o $@ $^

faa: faa.c
	$(CC) $(CFLAGS) -o $@ $^

lock: lock.c
	$(CC) $(CFLAGS) -o $@ $^

test: UpdateByMe.cpp
	g++ $(CFLAGS) -o $@ $^

waitfree : waitfree.o SyncUtils.o
	g++ $(CFLAGS) -o $@ $^

waitfreeup: waitfreeup.o
	$(CC) $(CFLAGS) -o $@ $^

jiffy: TimeMain.cpp
	g++  -std=c++11 -O3 -march=native -pthread -fexceptions -o $@ $^

wfqueue.o: wfqueue.c
	$(CC) $(CFLAGS) -c -o $@ $^

waitfree.o: waitfree.cpp
	g++ $(CFLAGS) -c waitfree.cpp

waitfreeup.o: waitfreeup.c
	$(CC) $(CFLAGS) -c -o $@ $^

SyncUtils.o: SyncUtils.cpp
	g++ $(CFLAGS) -c SyncUtils.cpp

clean:
	rm -f $(TESTS) *.o
