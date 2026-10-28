#!/bin/bash
# Run solution N times and count total restart iterations
N=3
for i in $(seq 1 $N); do
    time ./solution < t100_fixed.txt > /dev/null 2>&1
done
