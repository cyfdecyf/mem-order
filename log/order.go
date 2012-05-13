package main

import (
	"fmt"
	"io"
	"os"
	"path/filepath"
)

func logFilePath(dir string, thrid int) string {
	return fmt.Sprintf("%s/rec-rd-%d", dir, thrid)
}

func numberOfThreads(dir string) int {
	mat, err := filepath.Glob(fmt.Sprintf("%s/rec-rd-*", dir))
	if err != nil {
		fmt.Println("Can't find memory order file:", err)
		os.Exit(1)
	}
	return len(mat)
}

func readOneEntry(reader io.Reader) ([3]int, error) {
	var ent [3]int
	_, err := fmt.Fscanf(reader, "%d %d %d\n", &ent[0], &ent[1], &ent[2])
	if err != nil {
		return [3]int{0, 0, 0}, err
	}
	return ent, err
}

func openLog(dir string, thr int) *os.File {
	logPath := logFilePath(dir, thr)
	log, err := os.Open(logPath)
	if err != nil {
		fmt.Println("Can't open read log file:", err)
		os.Exit(1)
	}
	return log
}

type memopVersion struct {
	version int
	memop   int
}

const NOBJS = 20

type objHistory []memopVersion

func loadReadLog(dir string, thr int) []objHistory {
	log := openLog(dir, thr)
	defer log.Close()

	// In test program, we have on 20 objects
	objhis := make([]objHistory, NOBJS)
	for i := 0; i < NOBJS; i++ {
		objhis[i] = make(objHistory, 1000)
	}

	for {
		ent, err := readOneEntry(log)
		if err != nil {
			break
		}
		objid := ent[1]
		memv := memopVersion{memop: ent[0], version: ent[2]}
		objhis[objid] = append(objhis[objid], memv)
	}
	return objhis
}

func inferWriteAfterRead(dir string, thr int) {
	
}

func main() {
	logDir := "."
	nthr := numberOfThreads(logDir)
	fmt.Println("Number of threads:", nthr)
	loadReadLog(logDir, 0)
}
