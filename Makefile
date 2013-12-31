CC = gcc
CFLAGS = -std=gnu99 -g -O2 -Wall
CXX = g++
CXXFLAGS = -g -O2 -Wall

VPATH=tsx

LDFLAGS = -lpthread
MEMOBJS = mem.o log.o
RECORD_OBJS = $(MEMOBJS) mem-record.o
TARG = reorder-memop merge-memop merge-commit \
	   addcnt-seqlock-rec \
	   addcnt-seqbatch-rec \
	   addcnt-rtmseq-rec \
	   addcnt-rtmcluster-rec \
	   addcnt-rtmcommit-rec \
	   addcnt-play \
	   addcnt-rtmcommit-play \
	   \
	   racey-seqlock-rec \
	   racey-seqbatch-rec \
	   racey-rtmseq-rec \
	   racey-rtmcluster-rec \
	   racey-rtmcommit-rec \
	   racey-play \
	   racey-rtmcommit-play \
	   
all: $(TARG)

addcnt-seqlock-rec: $(RECORD_OBJS) mem-record-seqlock.o addcnt.o
	$(call cc-link-command)

addcnt-seqbatch-rec: $(RECORD_OBJS) mem-record-seqbatch.o addcnt.o
	$(call cc-link-command)

addcnt-rtmseq-rec: $(RECORD_OBJS) mem-record-rtmseq.o addcnt.o tsx-assert.o
	$(call cc-link-command)

addcnt-rtmcluster-rec: $(RECORD_OBJS) mem-record-rtmcluster.o addcnt.o tsx-assert.o
	$(call cc-link-command)

addcnt-rtmcommit-rec: $(MEMOBJS) mem-record-rtmcommit.o addcnt.o tsx-assert.o
	$(call cc-link-command)

addcnt-play: $(MEMOBJS) mem-replay.o addcnt.o
	$(call cc-link-command)

addcnt-rtmcommit-play: $(MEMOBJS) mem-replay-rtmcommit.o addcnt.o
	$(call cc-link-command)

racey-seqlock-rec: $(RECORD_OBJS) mem-record-seqlock.o racey.o
	$(call cc-link-command)

racey-rtmseq-rec: $(RECORD_OBJS) mem-record-rtmseq.o racey.o tsx-assert.o
	$(call cc-link-command)

racey-seqbatch-rec: $(RECORD_OBJS) mem-record-seqbatch.o racey.o
	$(call cc-link-command)

racey-rtmcluster-rec: $(RECORD_OBJS) mem-record-rtmcluster.o racey.o tsx-assert.o
	$(call cc-link-command)

racey-rtmcommit-rec: $(MEMOBJS) mem-record-rtmcommit.o racey.o tsx-assert.o
	$(call cc-link-command)

racey-play: $(MEMOBJS) mem-replay.o racey.o
	$(call cc-link-command)

racey-rtmcommit-play: $(MEMOBJS) mem-replay-rtmcommit.o racey.o
	$(call cc-link-command)

infer: infer.o
	$(call cc-link-command)
	cp $@ log/

reorder-memop: reorder-memop.o mem.o log.o
	$(call cxx-link-command)

merge-memop: merge-memop.o mem.o log.o
	$(call cxx-link-command)

merge-commit: merge-commit.o log.o
	$(call cxx-link-command)

TEST_NRUN = 30
TEST_NTHR = 4

test-seqlock: $(TARG)
	./test.sh addcnt seqlock $(TEST_TEST_NTHR) $(TEST_NRUN)
	./test.sh racey seqlock $(TEST_NTHR) $(TEST_NRUN)

test-seqbatch: $(TARG)
	./test.sh addcnt seqbatch $(TEST_NTHR) $(TEST_NRUN)
	./test.sh racey seqbatch $(TEST_NTHR) $(TEST_NRUN)

test-rtmseq: $(TARG)
	./test.sh addcnt rtmseq $(TEST_NTHR) $(TEST_NRUN)
	./test.sh racey rtmseq $(TEST_NTHR) $(TEST_NRUN)

test-rtmcluster: $(TARG)
	./test.sh addcnt rtmcluster $(TEST_NTHR) $(TEST_NRUN)
	./test.sh racey rtmcluster $(TEST_NTHR) $(TEST_NRUN)

test-rtmcommit: $(TARG)
	./test.sh addcnt rtmcommit $(TEST_NTHR) $(TEST_NRUN)
	./test.sh racey rtmcommit $(TEST_NTHR) $(TEST_NRUN)

clean:
	-rm -f *.o
	-rm -rf .*.d .*.dpp
	-rm -f $(TARG)

include rules.make
