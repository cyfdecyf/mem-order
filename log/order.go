package main

import (
	"fmt"
	"io"
	"os"
	"path/filepath"
	"sort"
)

const (
	NOBJS         = 20
	RDLOG_PATTERN = "rec-rd"
	WRLOG_PATTERN = "rec-wr"
	PLLOG_PATTERN = "play-wr"
)

type memopVersion struct {
	memop   int
	version int
}

type objHistory []memopVersion

type thrObjHistory struct {
	thrid  int
	objhis []objHistory
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

func readOneEntry(reader io.Reader) (objid int, ent memopVersion, err error) {
	ent = memopVersion{}
	_, err = fmt.Fscanf(reader, "%d %d %d", &ent.memop, &objid, &ent.version)
	if err != nil {
		return -1, ent, err
	}
	return objid, ent, err
}

func openLog(dir, pattern string, thr int) *os.File {
	logPath := logFilePath(dir, pattern, thr)
	log, err := os.Open(logPath)
	if err != nil {
		fmt.Println("Can't open read log file:", err)
		os.Exit(1)
	}
	return log
}

func loadReadLog(dir string, thr int, ch chan *thrObjHistory) {
	log := openLog(dir, RDLOG_PATTERN, thr)
	defer log.Close()

	// In the test program, we have only 20 objects
	objhis := make([]objHistory, NOBJS)
	for i := 0; i < NOBJS; i++ {
		objhis[i] = make(objHistory, 1000)
	}

	for {
		objid, ent, err := readOneEntry(log)
		if err == io.EOF {
			break
		}
		if objid > NOBJS {
			fmt.Println("objid not correct: thread", thr, "objid", objid)
			os.Exit(1)
		}
		objhis[objid] = append(objhis[objid], ent)
	}
	ch <- &thrObjHistory{thrid: thr, objhis: objhis}
}

func searchObjVersion(objhis objHistory, version int) *memopVersion {
	i := sort.Search(len(objhis), func(objid int) bool { return objhis[objid].version >= version })
	if i < len(objhis) && objhis[i].version == version {
		return &objhis[i]
	}
	return nil
}

func inferWriteAfterRead(dir string, thrid, nthr int, readDb [][]objHistory, ch chan int) {
	fmt.Println("Inferring read->write order", thrid)
	log := openLog(dir, WRLOG_PATTERN, thrid)
	outlogPath := logFilePath(dir, "play-wr", thrid)
	outlog, err := os.Create(outlogPath)
	if err != nil {
		fmt.Println("Can't open ")
	}

	for {
		objid, ent, err := readOneEntry(log)
		if err == io.EOF {
			break
		}
		// Search each other CPU's read log and find the 
		for i := 0; i < nthr; i++ {
			if i == thrid {
				continue
			}
			memopVersion := searchObjVersion(readDb[i][objid], ent.version)
			if memopVersion == nil {
				continue
			}
			fmt.Fprintf(outlog, "thread %d memop %d access obj %d should wait thread %d memop %d\n",
				thrid, ent.memop, objid, i, memopVersion.memop)
		}
	}

	ch <- 1
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

func main() {
	logDir := "."
	nthr := numberOfThreads(logDir)
	fmt.Println("Number of threads:", nthr)

	checkLogFile(logDir, nthr)

	readDb := make([][]objHistory, nthr)

	loadCh := make(chan *thrObjHistory)
	for i := 0; i < nthr; i++ {
		go loadReadLog(logDir, i, loadCh)
	}
	for i := 0; i < nthr; i++ {
		thrdb := <-loadCh
		readDb[thrdb.thrid] = thrdb.objhis
	}
	fmt.Println("All read log loaded")

	ch := make(chan int)
	for i := 0; i < nthr; i++ {
		go inferWriteAfterRead(logDir, i, nthr, readDb, ch)
	}
	// Wait all goroutines to finish
	for i := 0; i < nthr; i++ {
		<-ch
	}
}
