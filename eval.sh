#!/bin/bash

# check param
if [ $# != 2 ]
then
    echo "usage: evalu.sh <program> <method>"
    exit 1
fi


PROGRAM="./$1-$2-rec"
NTHR="1 2 4"
TEST_RUN=10

# check whether executable exits
if [ ! -x $PROGRAM ]
then
    echo Fail to run $PROGRAM
    echo Maybe it doesn\'t exist, or you have no permission!
    exit 1
fi


# run program and collect data
function run() {
    local log
    local prog
    local thr
    local runs
    log="log/$1.$2"
    prog=$1
    thr=$2
    runs=$3
    rm -f $log $log.tmp
    echo $thr > $log
    for i in `seq 1 $runs`
    do
        $prog $thr > $log.tmp
        awk '/tsc/ {print $2}' $log.tmp >> $log
    done
    rm $log.tmp
}


# assume there are $PROGAM.$THREAD_NUM files
function merge() {
    local prog
    local log
    prog=$1
    log="log/$prog"
    paste "log/$prog".* > $log
    rm "log/$prog".*
}

function main() {
    for thr in $NTHR
    do
        run $PROGRAM $thr $TEST_RUN
    done
    
    merge $PROGRAM
}

mkdir -p log
main
