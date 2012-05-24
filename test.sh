#!/bin/bash

COLOR='\e[1;32m'

function cecho() {
    echo -e "$COLOR$*\e[0m"
}

function process_log() {
    let maxid=$nthr-1
    for i in `seq 0 $maxid`; do
        echo Infer WAR for thread $i
        ./infer $i rec-rd-$i
    done
    cat war-* | sort -n -k1,1 -k2,2 > war
}

if [ $# != 1 ]; then
    echo "Usage: run.sh <nthr>"
    exit 1
fi

nthr=$1

rm -f log/rec* log/war*
cecho "Start record with $nthr threads\n"
./record $nthr | tee > result-record
echo

cecho Log processing
(cd log; process_log)

cecho "Start replay\n"
./play $nthr | tee > result-play

diff result-record result-play
