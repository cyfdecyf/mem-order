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
