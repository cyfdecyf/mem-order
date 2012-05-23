#!/bin/bash

function process_log() {
    let maxid=$nthr-1
    for i in `seq 0 $maxid`; do
        echo Infer WAR for thread $i
        ./infer $i $nobj rec-rd-$i
    done
    cat war-* | ./sort.lua > war
}

if [ $# != 2 ]; then
    echo "Usage: run.sh <nthr> <nobj>"
    exit 1
fi

nthr=$1
nobj=$2

echo "$nthr threads, $nobj shared objects"

rm -f log/rec* log/war*
./record $nthr

(cd log; process_log)
