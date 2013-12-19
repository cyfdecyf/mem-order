CC = gcc
CFLAGS = -std=gnu99 -g -O2 -Wall
CXX = g++
CXXFLAGS = -g -O2 -Wall

LDFLAGS = -lpthread
MEMOBJS = mem.o log.o
TARG = addcnt-rec addcnt-play racey-rec racey-play reorder-memop merge-memop
TEST = mem-test

all: $(TARG)

addcnt-dummy: $(MEMOBJS) addcnt.o
	$(CC) -O2 $(CFLAGS) -c -DDUMMY mem-main.c
	$(call cc-link-command)

addcnt-rec: $(MEMOBJS) mem-record.o addcnt.o
	$(call cc-link-command)

addcnt-play: $(MEMOBJS) mem-replay.o addcnt.o
	$(call cc-link-command)

racey-rec: $(MEMOBJS) mem-record.o racey.o
	$(call cc-link-command)

racey-play: $(MEMOBJS) mem-replay.o racey.o
	$(call cc-link-command)

mem-test: mem-test.o mem.o
	$(call cc-link-command)

infer: infer.o
	$(call cc-link-command)
	cp $@ log/

reorder-memop: reorder-memop.o mem.o log.o
	$(call cxx-link-command)

merge-memop: merge-memop.o mem.o log.o
	$(call cxx-link-command)

test: $(TARG)
	./test.sh 4 10

clean:
	-rm -f *.o
	-rm -rf .*.d .*.dpp
	-rm -f $(TARG)
	-rm $(TEST)

include rules.make
