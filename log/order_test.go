package main

import (
	"fmt"
	"os"
	"testing"
)

// exists returns whether the given file or directory exists or not
func fileExists(path string) (bool, error) {
	_, err := os.Stat(path)
	if err == nil {
		return true, nil
	}
	if os.IsNotExist(err) {
		return false, nil
	}
	return false, err
}

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
	log := openLog("testdata", 0)
	ent, err := readOneEntry(log)
	if err != nil {
		t.Fatal("Reading entry got error", err)
	}

	if ent != [3]int{0, 0, 4} {
		t.Log("Get entry:", ent)
		t.Fatal("Entry wrong")
	}

	ent, err = readOneEntry(log)
	if ent != [3]int{2, 1, 38} {
		t.Log("Get entry:", ent)
		t.Fatal("Entry wrong")
	}
}
