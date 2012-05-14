package main

import (
	"fmt"
	"testing"
)

func TestNumberOfThreads(t *testing.T) {
	nthr := numberOfThreads("testdata")
	fmt.Println("Number of threads", nthr)
	for i := 0; i < nthr; i++ {
		if r, _ := fileExists(fmt.Sprintf("testdata/rec-rd-%d", i)); !r {
			t.Fatal("Number of thread not correct")
		}
	}

}

func TestReadEntry(t *testing.T) {
	log := openLog("testdata", RDLOG_PATTERN, 0)
	objid, ent, err := readOneEntry(log)
	if err != nil {
		t.Fatal("Reading entry got error", err)
	}

	if objid != 0 || ent.memop != 0 || ent.version != 4 {
		t.Log("Get entry:", ent)
		t.Fatal("Entry wrong")
	}

	objid, ent, err = readOneEntry(log)
	if objid != 1 || ent.memop != 2 || ent.version != 38 {
		t.Log("Get entry:", ent)
		t.Fatal("Entry wrong")
	}
}
