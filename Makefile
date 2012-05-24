CC = gcc
CFLAGS = -std=gnu99 -g -O3 -Wall
CXX = g++
CXXFLAGS = -g -O3 -Wall

LDFLAGS = -lpthread
MEMOBJS = mem.o log.o
TARG = record play infer
TEST = mem-test

SRC = mem.c mem-main.c

all: $(TARG) $(TEST)

dummy-acc: $(MEMOBJS) mem-dummy.o mem-main.o
	$(call cc-link-command)

record: $(MEMOBJS) mem-record.o mem-main.o
	$(call cc-link-command)

play: $(MEMOBJS) mem-replay.o mem-main.o
	$(call cc-link-command)

mem-test: mem-test.o mem.o
	$(call cc-link-command)

infer: infer.o
	$(call cc-link-command)
	cp $@ log/

test: $(TEST)

clean:
	-rm -f *.o
	-rm -rf .*.d
	-rm -f $(TARG)
	-rm $(TEST)

include rules.make
