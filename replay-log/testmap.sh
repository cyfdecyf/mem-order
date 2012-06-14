#!/bin/bash

truncate file -s 0
truncate file -s 504857600
time ./mmap-test

