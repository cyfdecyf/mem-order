package main

import (
	"bufio"
	"fmt"
	"io"
	"log"
	"os"
	//"runtime/pprof"
	"strconv"
	"strings"
)

func main() {
	if len(os.Args) != 4 {
		log.Fatalln("Usage: infer <tid> <nobj> <logfile>")
	}

	tid, err := strconv.Atoi(os.Args[1])
	if err != nil {
		log.Fatalln("number of objects not correct", err)
	}
	nobj, err := strconv.Atoi(os.Args[2])
	if err != nil {
		log.Fatalln("number of objects not correct", err)
	}

	rlog, err := os.Open(os.Args[3])
	if err != nil {
		log.Fatalln("Can't open read log file:", err)
	}
	defer rlog.Close()

	reader := bufio.NewReader(rlog)

	warpath := fmt.Sprintf("war-%d", tid)
	warlog, err := os.Create(warpath)
	if err != nil {
		log.Fatalln("Can't open write log file:", err)
	}
	defer warlog.Close()

	last_read_version := make([]int, nobj)

	//f, err := os.Create("prof.infer")
	//if err != nil {
	//	log.Fatal(err)
	//}
	//pprof.StartCPUProfile(f)
	//defer pprof.StopCPUProfile()

	for {
		line, _, err := reader.ReadLine()
		if err == io.EOF {
			break
		}

		strs := strings.Split(string(line), " ")
		if len(strs) != 4 {
			log.Fatalln("malformed line")
		}

		objid, _ := strconv.Atoi(strs[1])
		version, _ := strconv.Atoi(strs[2])
		last_read_memop, _ := strconv.Atoi(strs[3])

		// If no previous read, no dependency is needed
		if last_read_memop == -1 {
			last_read_version[objid] = version
			continue
		}

		fmt.Fprintf(warlog, "%d %d %d\n", tid, last_read_memop,
			last_read_version[objid])
		last_read_version[objid] = version
	}
}
