#!/bin/bash
if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <input_file> <output_file>"
    exit 1
fi

INPUT=$1
OUTPUT=$2
./main "$INPUT" "$OUTPUT"
