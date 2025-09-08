#!/bin/bash
# Summary script for running solver & format_checker
if [ "$#" -ne 1 ]; then
    echo "Usage: $0 [1|2]"
    exit 1
fi

param=$1
inputs=(SampleInputOutput/input1.txt SampleInputOutput/input2.txt SampleInputOutput/input3.txt)

echo "Running with param=$param"
if [ "$param" -eq 1 ]; then
    for inp in "${inputs[@]}"; do
        out="tmp_out.txt"
        ./main "$inp" "$out" >/dev/null 2>&1
        score=$(./format_checker "$inp" "$out" | awk '/FINAL SCORE/ {print $3}')
        echo "$inp: $score"
    done
elif [ "$param" -eq 2 ]; then
    for inp in SampleInputOutput/*.txt; do
        out="tmp_out.txt"
        ./main "$inp" "$out" >/dev/null 2>&1
        score=$(./format_checker "$inp" "$out" | awk '/FINAL SCORE/ {print $3}')
        echo "$inp: $score"
    done
else
    echo "Invalid parameter: $param"
    exit 1
fi
