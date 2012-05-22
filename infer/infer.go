package main

import (
	"fmt"
	"io"
	"os"
	"path/filepath"
	"runtime"
	"strconv"
)

const (
	RDLOG_PATTERN = "rec-rd"
	PLLOG_PATTERN = "war"
)

type ReadLog struct {
	readlog io.Reader
	warlog  io.Writer

	tid int

	last_read_version []int
}

func NewReadLog(tid, nobj int, readlogpath, warlogpath string) (log *ReadLog) {
	log = new(ReadLog)

	var err error
	log.readlog, err = os.Open(readlogpath)
	if err != nil {
		fmt.Println("Can't open read log file:", err)
		os.Exit(1)
	}

	log.warlog, err = os.Create(warlogpath)
	if err != nil {
		fmt.Println("Can't open read log file:", err)
		os.Exit(1)
	}

	log.tid = tid

	log.last_read_version = make([]int, nobj)
	return
}

func (log *ReadLog) inferWriteAfterRead(ch chan int) {
	fmt.Println("Inferring write-after-read order", log.tid)

	for {
		var read_memop, objid, version, last_read_memop int
		_, err := fmt.Fscanln(log.readlog, &read_memop, &objid, &version,
			&last_read_memop)
		if err == io.EOF {
			break
		}

		// If no previous read, no dependency is needed
		if last_read_memop == -1 {
			log.last_read_version[objid] = version
			continue
		}
		fmt.Fprintf(log.warlog, "%d %d %d", log.tid, last_read_memop,
			log.last_read_version[objid])
		log.last_read_version[objid] = version
	}

	ch <- 1
}

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

func checkLogFile(logDir string, nthr int) {
	for i := 0; i < nthr; i++ {
		logPath := logFilePath(logDir, PLLOG_PATTERN, i)
		if exist, _ := fileExists(logPath); !exist {
			fmt.Println("Log file", logPath, "does not exits")
			os.Exit(1)
		}
	}
}

func logFilePath(dir, pattern string, thrid int) string {
	return fmt.Sprintf("%s/%s-%d", dir, pattern, thrid)
}

func numberOfThreads(dir string) int {
	mat, err := filepath.Glob(fmt.Sprintf("%s/%s-*", RDLOG_PATTERN, dir))
	if err != nil {
		fmt.Println("Can't find memory order file:", err)
		os.Exit(1)
	}
	return len(mat)
}

func main() {
	if len(os.Args) != 2 {
		fmt.Println("Usage: infer <nobjs>")
		os.Exit(1)
	}

	nobjs, err := strconv.Atoi(os.Args[1])
	if err != nil {
		fmt.Println("number of objects not correct", err)
		os.Exit(1)
	}

	logDir := "."
	nthr := numberOfThreads(logDir)
	fmt.Println("Number of threads:", nthr)
	checkLogFile(logDir, nthr)

	runtime.GOMAXPROCS(nthr)

	wait_infer := make(chan int)
	for i := 0; i < nthr; i++ {
		log := NewReadLog(i, nobjs,
			logFilePath(logDir, RDLOG_PATTERN, i),
			logFilePath(logDir, PLLOG_PATTERN, i))
		go log.inferWriteAfterRead(wait_infer)
	}

	// Wait all goroutines to finish
	for i := 0; i < nthr; i++ {
		<-wait_infer
	}
}
