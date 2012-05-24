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
cecho "Record with $nthr threads   Result:========"
./record $nthr | tee result-record
cecho "End result==============================="

cecho "Log processing ..."
(cd log; process_log)

cecho "Replay with $nthr threads    Result:======="
./play $nthr | tee result-play
cecho "End result==============================="

diff result-record result-play

if [ $? == 0 ]; then
	cecho "Replay result correct"
else
    echo -e "\e[1;31mReplay result wrong\e[0m"
fi
