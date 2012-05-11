CFLAGS = -std=gnu99 -g -O2 -Wall
LDFLAGS = -lpthread
CC = gcc
OBJS = mem.o log.o
TARG = dummy-acc record

all: $(TARG)

dummy-acc: $(OBJS) mem-dummy.o main.o
	$(CC) $^ $(LDFLAGS) -o $@

record: $(OBJS) mem-record.o main.o
	$(CC) $^ $(LDFLAGS) -o $@

unit-test: unit-test.o mem.o
	$(CC) $^ $(LDFLAGS) -o $@

test: unit-test
	./unit-test

clean:
	-rm -f *.o
	-rm -f $(TARG)
	-rm unit-test
