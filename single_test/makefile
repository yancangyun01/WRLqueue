TESTS = wfqueue_0 faa_0 lock_0 waitfree_0 waitfree_1 waitfreeup_0 test_0 jiffy_0

CC = gcc
CFLAGS = -g -Wall -O3 -pthread -D_GNU_SOURCE
LDLIBS = -lpthread -lm

all: $(TESTS)

wfqueue_0: wfqueue_0.o
	$(CC) $(CFLAGS) -o $@ $^

faa_0: faa_0.c
	$(CC) $(CFLAGS) -o $@ $^

lock_0: lock_0.c
	$(CC) $(CFLAGS) -o $@ $^

test_0: UpdateByMe_0.cpp
	g++ $(CFLAGS) -o $@ $^

waitfreeup_0: waitfreeup_0.c
	$(CC) $(CFLAGS) -o $@ $^

waitfree_0 : waitfree_0.o SyncUtils.o
	g++ $(CFLAGS) -o $@ $^

waitfree_1 : waitfree_1.o SyncUtils.o
	g++ $(CFLAGS) -o $@ $^

jiffy_0: TimeMain_0.cpp
	g++  -std=c++11 -O3 -march=native -pthread -fexceptions -o $@ $^

wfqueue_0.o: wfqueue_0.c
	$(CC) $(CFLAGS) -c -o $@ $^

waitfree_0.o: waitfree_0.cpp
	g++ $(CFLAGS) -c waitfree_0.cpp

waitfree_1.o: waitfree_1.cpp
	g++ $(CFLAGS) -c waitfree_1.cpp

SyncUtils.o: SyncUtils.cpp
	g++ $(CFLAGS) -c SyncUtils.cpp

clean:
	rm -f $(TESTS) *.o
