CC = gcc
CFLAGS = -std=gnu99 -g -O2 -Wall
CXX = g++
CXXFLAGS = -g -O2 -Wall

LDFLAGS = -lpthread
MEMOBJS = mem.o log.o
TARG = dummy-acc record infer
TEST = mem-test infer-test

all: $(TARG) $(TEST)

dummy-acc: $(MEMOBJS) mem-dummy.o mem-main.o
	$(CC) $^ $(LDFLAGS) -o $@

mem-record.o: mem.h

record: $(MEMOBJS) mem-record.o mem-main.o
	$(CC) $^ $(LDFLAGS) -o $@

mem-test: mem-test.o mem.o
	$(CC) $^ $(LDFLAGS) -o $@

# The following are for log processing

infer.o: infer.hpp

infer: infer.o infer-main.o
	$(CXX) $^ -o $@

infer-test: infer-test.o infer.o $(MEMOBJS)
	$(CXX) $^ -o $@ -lboost_unit_test_framework-mt

test: $(TEST)

clean:
	-rm -f *.o
	-rm -f $(TARG)
	-rm $(TEST)

%.c:
%.cpp:
%.h:
%.hpp:
%.o:
