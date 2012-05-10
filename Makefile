CFLAGS = -std=gnu99 -g -O2
LDFLAGS = -lpthread
CC = gcc
OBJS = mem.o
TARG = dummy-acc

all: $(TARG)

dummy-acc: mem-dummy.o main.o $(OBJS)
	$(CC) $^ $(LDFLAGS) -o $@

unit-test: unit-test.o $(OBJS)
	$(CC) $^ $(LDFLAGS) -o $@

test: unit-test
	./unit-test

clean:
	-rm -f *.o
	-rm -f $(TARG)
	-rm unit-test
