#!/bin/bash

hexdump -e '/1 "%c" /4 "%i" /1  " obj %i" /4 " @%i" /4 " val %i" "\n"' $1
