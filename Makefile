CC = gcc
CFLAGS = -std=gnu99 -g -O2 -Wall
CXX = g++
CXXFLAGS = -g -O2 -Wall

LDFLAGS = -lpthread
MEMOBJS = mem.o log.o
TARG = dummy-acc record infer
TEST = mem-test infer-test

SRC = mem.c mem-main.c

all: $(TARG) $(TEST)

dummy-acc: $(MEMOBJS) mem-dummy.o mem-main.o
	$(call cc-link-command)

mem-record.o: mem.h

record: $(MEMOBJS) mem-record.o mem-main.o
	$(call cc-link-command)

mem-test: mem-test.o mem.o
	$(call cc-link-command)

# The following are for log processing

infer: infer.o infer-main.o
	$(call cxx-link-command)

infer-test: infer-test.o infer.o $(MEMOBJS)
	$(call cxx-link-command, -lboost_unit_test_framework-mt)

test: $(TEST)

clean:
	-rm -f *.o
	-rm -rf .*.d
	-rm -f $(TARG)
	-rm $(TEST)

include rules.make