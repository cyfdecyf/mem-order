#!/bin/bash

if [[ $# != 3 && $# != 4 ]]; then
    echo "Usage: time.sh <cmd> <impl> <nthr> [ntimes]"
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

echo $ntimes

rm -f time-record
for i in `seq 1 $ntimes`; do
    /usr/bin/time -p -a -o time-record ./$cmd-$impl-rec $nthr
    sleep 1
done

grep real time-record | awk '{ sum += $2 } END { print sum/NR }'
