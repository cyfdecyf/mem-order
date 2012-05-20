CC = gcc
CFLAGS = -std=gnu99 -g -O2 -Wall
CXX = g++
CXXFLAGS = -g -O2 -Wall

LDFLAGS = -lpthread
MEMOBJS = mem.o log.o
TARG = dummy-acc record infer

all: $(TARG)

dummy-acc: $(MEMOBJS) mem-dummy.o mem-main.o
	$(CC) $^ $(LDFLAGS) -o $@

mem-record.o: mem.h

record: $(MEMOBJS) mem-record.o mem-main.o
	$(CC) $^ $(LDFLAGS) -o $@

unit-test: unit-test.o mem.o
	$(CC) $^ $(LDFLAGS) -o $@

test: unit-test
	./unit-test

infer.o: infer.hpp

infer: infer.o infer-main.o
	$(CXX) $^ $(LDFLAGS) -o $@

clean:
	-rm -f *.o
	-rm -f $(TARG)
	-rm unit-test

%.c:
%.cpp:
%.h:
%.hpp:
%.o:
