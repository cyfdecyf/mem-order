#!/bin/bash

sed -e 's/W/W /' -e 's/R/R /' < debug-record | sort -k5,5n -k8,8n -k 3,3n | \
    sed -e 's/W /W/' -e 's/R /R/' > debug-sorted-record
