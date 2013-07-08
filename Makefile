CC = gcc
CFLAGS = -std=gnu99 -g -O2 -Wall
CXX = g++
CXXFLAGS = -g -O2 -Wall

LDFLAGS = -lpthread
MEMOBJS = mem.o log.o
TARG = record play reorder-memop merge-memop
TEST = mem-test

SRC = mem.c mem-main.c

all: $(TARG)

dummy-acc: $(MEMOBJS) mem-main.o
	$(CC) -O2 $(CFLAGS) -c -DDUMMY mem-main.c
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

reorder-memop: reorder-memop.o log.o
	$(call cxx-link-command)

merge-memop: merge-memop.o log.o
	$(call cxx-link-command)

test: $(TEST)

clean:
	-rm -f *.o
	-rm -rf .*.d .*.dpp
	-rm -f $(TARG)
	-rm $(TEST)

include rules.make
