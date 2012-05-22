package main

import (
	"fmt"
	"io"
	"os"
	"strconv"
)

// This version is almost the same with the C code. But still very slow.

func main() {
	if len(os.Args) != 4 {
		fmt.Println("Usage: infer <tid> <nobj> <logfile>");
		os.Exit(1)
	}

	tid, err := strconv.Atoi(os.Args[1])
	if err != nil {
		fmt.Println("number of objects not correct", err)
		os.Exit(1)
	}
	nobj, err := strconv.Atoi(os.Args[2])
	if err != nil {
		fmt.Println("number of objects not correct", err)
		os.Exit(1)
	}

	rlog, err := os.Open(os.Args[3])
	if err != nil {
		fmt.Println("Can't open read log file:", err)
		os.Exit(1)
	}

	warpath := fmt.Sprintf("war-%d", tid)
	warlog, err := os.Create(warpath)
	if err != nil {
		fmt.Println("Can't open write log file:", err)
		os.Exit(1)
	}

	last_read_version := make([]int, nobj)

	var read_memop, objid, version, last_read_memop int
	for {
		_, err := fmt.Fscanln(rlog, &read_memop, &objid, &version,
			&last_read_memop)
		if err == io.EOF {
			break
		}

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
