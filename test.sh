#!/bin/bash

COLOR='\e[1;32m'

function cecho() {
    echo -e "$COLOR$*\e[0m"
}

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

rm -f log/rec* log/war*
cecho "Start record with $nthr threads, $nobj shared objects"
./record $nthr > result-record

cecho Log processing
(cd log; process_log)

cecho Start replay
./play $nthr > result-play

diff result-record result-play
