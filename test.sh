#!/bin/bash

if [[ $# > 2 || $# == 0 ]]; then
    echo "Usage: run.sh <nthr> [ntimes]"
    exit 1
fi

COLOR='\e[1;32m'

function cecho() {
    echo -e "$COLOR$*\e[0m"
}

function process_text_log() {
    pushd log
    local maxid
    let maxid=$nthr-1
    sort -n -k1,1 -k2,2 > "memop" <(for i in `seq 0 $maxid`; do
        grep -v -- '-1$' memop-$i | awk "{ print \$0 \" $i\"}"
    done)
    popd
}

function process_binary_log() {
    local maxid
    local i
    let maxid=$nthr-1
    for i in `seq 0 $maxid`; do
        ./reorder-memop $i
    done
    ./merge-memop $nthr
}

function process_log() {
    file log/memop-0 | grep 'ASCII' > /dev/null
    if [ $? == 0 ]; then
        process_text_log
    else
        process_binary_log
    fi
}

nthr=$1
if [ $# == 2 ]; then
    ntimes=$2
else
    ntimes=1
fi

for i in `seq 1 $ntimes`; do
    rm -f replay-log/{memop*,version*,sorted-*}
    cecho "$i iteration"
    cecho "Record with $nthr threads"
    ./record $nthr 2>debug-record > result-record

    cecho "Processing log ..."

    process_log

    cecho "Replay with $nthr threads"
    ./play $nthr 2>debug-play > result-play

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
