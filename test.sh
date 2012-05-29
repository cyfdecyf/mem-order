#!/bin/bash

COLOR='\e[1;32m'

function cecho() {
    echo -e "$COLOR$*\e[0m"
}

function process_log() {
    pushd log
    let maxid=$nthr-1
    sort -n -k1,1 -k2,2 > "memop" <(for i in `seq 0 $maxid`; do
        awk "{ print \$0 \" $i\"}" < memop-$i
    done)
    popd
}

if [[ $# > 2 || $# == 0 ]]; then
    echo "Usage: run.sh <nthr> [ntimes]"
    exit 1
fi

nthr=$1
if [ $# == 2 ]; then
    ntimes=$2
else
    ntimes=1
fi

for i in `seq 1 $ntimes`; do
    rm -f log/memop* log/version*
    cecho "Record with $nthr threads   Result:=========="
    ./record $nthr | tee result-record
    cecho "End result==============================="

    cecho "Log processing ..."

    process_log

    cecho "Replay with $nthr threads    Result:========="
    ./play $nthr | tee result-play
    cecho "End result==============================="

    diff result-record result-play

    if [ $? == 0 ]; then
        cecho "Replay result correct"
    else
        echo -e "\e[1;31mReplay result wrong\e[0m"
        exit 1
    fi
done
