#!/bin/bash

if [ $# != 2 ]; then
    echo "Usage: $0 <nthr> <num>"
    exit 1
fi

nthr=$1
n=$2

for i in `seq $n`; do
    ./test.sh $nthr || exit 1
done
