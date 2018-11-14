#!/usr/local/bin/bash
# 
# Times runs of the workload benchmark with N samples and averages their results.
#     - Uses a constant scaling factor to run each benchmark M times,
#       so that lower times like 0.00, which are some value but are too small to
#       display, appear when scaled.
#
# Arguments:
# $1: workload to run ("io", "cpu", or "split")

make clean
make
echo

CONSTANT_SCALING=5  # Number of iterations we run to gather time from.
SAMPLES=10 # Run benchmark program N times, then average them.

# Keep track of our total times as we sample.
total_real=0.00
total_user=0.00
total_sys=0.00

for sample in $(seq 1 $SAMPLES); do
    echo --- Sample $sample ---

    # Captures time only.
    time_output=$( { /usr/bin/time -p ./run.sh $CONSTANT_SCALING $1; } 2>&1 )
    for line in $time_output; do
        if [ "$line" == "real" ] || [ "$line" == "user" ] || [ "$line" == "sys" ]; then
            category=$line
        else
            echo $category $line
            if [ "$category" == "real" ]; then
                total_real=$(echo "scale=2; $total_real + $line" | bc -l)
            elif [ "$category" == "user" ]; then
                total_user=$(echo "scale=2; $total_user + $line" | bc -l)
            elif [ "$category" == "sys" ]; then
                total_sys=$(echo "scale=2; $total_sys + $line" | bc -l)
            fi
        fi
    done
    echo
done

echo ================
avg_real=$(echo "scale=2; $total_real / $SAMPLES" | bc -l)
avg_user=$(echo "scale=2; $total_user / $SAMPLES" | bc -l)
avg_sys=$(echo "scale=2; $total_sys / $SAMPLES" | bc -l)
echo avg_real $avg_real
echo avg_user $avg_user
echo avg_sys $avg_sys

