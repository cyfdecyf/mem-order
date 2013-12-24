CC = gcc
CFLAGS = -std=gnu99 -g -O2 -Wall
CXX = g++
CXXFLAGS = -g -O2 -Wall

LDFLAGS = -lpthread
MEMOBJS = mem.o log.o
RECORD_OBJS = $(MEMOBJS) mem-record.o
TARG = reorder-memop merge-memop \
	   addcnt-seqlock-rec \
	   addcnt-seqbatch-rec \
	   addcnt-rtmseq-rec \
	   addcnt-play \
	   racey-seqlock-rec \
	   racey-seqbatch-rec \
	   racey-rtmseq-rec \
	   racey-play
	   
all: $(TARG)

addcnt-dummy: $(MEMOBJS) addcnt.o
	$(CC) -O2 $(CFLAGS) -c -DDUMMY mem-main.c
	$(call cc-link-command)

addcnt-seqlock-rec: $(RECORD_OBJS) mem-record-seqlock.o addcnt.o
	$(call cc-link-command)

addcnt-seqbatch-rec: $(RECORD_OBJS) mem-record-seqbatch.o addcnt.o
	$(call cc-link-command)

addcnt-rtmseq-rec: $(RECORD_OBJS) mem-record-rtmseq.o addcnt.o
	$(call cc-link-command)

addcnt-play: $(MEMOBJS) mem-replay.o addcnt.o
	$(call cc-link-command)

racey-seqlock-rec: $(RECORD_OBJS) mem-record-seqlock.o racey.o
	$(call cc-link-command)

racey-rtmseq-rec: $(RECORD_OBJS) mem-record-rtmseq.o racey.o
	$(call cc-link-command)

racey-seqbatch-rec: $(RECORD_OBJS) mem-record-seqbatch.o racey.o
	$(call cc-link-command)

racey-play: $(MEMOBJS) mem-replay.o racey.o
	$(call cc-link-command)

infer: infer.o
	$(call cc-link-command)
	cp $@ log/

reorder-memop: reorder-memop.o mem.o log.o
	$(call cxx-link-command)

merge-memop: merge-memop.o mem.o log.o
	$(call cxx-link-command)

test-seqlock: $(TARG)
	./test.sh addcnt seqlock 4 10
	./test.sh racey seqlock 4 10

test-seqbatch: $(TARG)
	./test.sh addcnt seqbatch 4 10
	./test.sh racey seqbatch 4 10

test-rtmseq: $(TARG)
	./test.sh addcnt rtmseq 4 10
	./test.sh racey rtmseq 4 10

clean:
	-rm -f *.o
	-rm -rf .*.d .*.dpp
	-rm -f $(TARG)

include rules.make
