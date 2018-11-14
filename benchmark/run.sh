#!/usr/local/bin/bash
#
# Runs the benchmark C program.
#
# Arguments:
# $1: number of iterations to run benchmark
# $2: workload to run ("io", "cpu", or "split")

function num_benchmarks() {
    # Gets the number of benchmark processes currently running.
    match="./benchmark 123 $2"
    echo $(ps aux | grep -o "$match" | wc -l | awk '{print $1}')
}

for i in $(seq 1 $1); do 

    start_nprocs=$(num_benchmarks)

    ./benchmark 123 $2 1>/dev/null 2>/dev/null

    # Wait for the processes to clean up before starting another iteration.
    # Use a small delay so we don't impact the total time too much.
    until [ "$(num_benchmarks)" -le "$start_nprocs" ]; do
        sleep 0.1        
    done

done

