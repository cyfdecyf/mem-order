#!/bin/bash

if [[ $# != 3 && $# != 4 ]]; then
    echo "Usage: run.sh <cmd> <impl> <nthr> [ntimes]"
    exit 1
fi

cmd=$1
impl=$2
nthr=$3
if [[ $# == 4 ]]; then
    ntimes=$4
else
    ntimes=10
fi

if [ $cmd = "addcnt" ]; then
    nobj=10
elif [ $cmd = "racey" ]; then
    nobj=64
fi

COLOR='\e[1;32m'

function cecho() {
    echo -e "$COLOR$*\e[0m"
}

function process_binary_log() {
    local maxid
    local i
    let maxid=$nthr-1
    for i in `seq 0 $maxid`; do
        ./reorder-memop $nobj $i
    done
    ./merge-memop $nobj $nthr
}

function process_log() {
    file log/memop-0 | grep 'ASCII' > /dev/null
    if [ $? == 0 ]; then
        process_text_log
    else
        process_binary_log $1
    fi
}

for i in `seq 1 $ntimes`; do
    rm -f replay-log/{memop*,version*,sorted-*}
    cecho "Record with $nthr threads"
    if ! ./$cmd-$impl-rec $nthr 2>debug-record > result-record; then
        cat debug-record result-record
        exit 1
    fi

    cecho "Processing log ..."

    process_log $nobj || exit 1

    cecho "Replay with $nthr threads"
    if ! ./$cmd-play $nthr 2>debug-play > result-play; then
        cat debug-play result-play
        exit 1
    fi

    diff result-record result-play

    if [ $? == 0 ]; then
        if [ ${i} != $ntimes ]; then
            echo
        fi
    else
        echo -e "\e[1;31mReplay result wrong\e[0m"
        exit 1
    fi
done

