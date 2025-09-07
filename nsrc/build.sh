#!/bin/bash

# Build script for the new class-based implementation

echo "Building new implementation in nsrc directory..."
cd /home/dbringery/Sem_5/COL333/A1/nsrc

# Clean previous build
make clean

# Build the project
make

if [ $? -eq 0 ]; then
    echo "Build successful!"
    echo "Executable created: ../target/nsolver"
    echo ""
    echo "Usage: ../target/nsolver <input_file> <output_file>"
    echo ""
    echo "Example: ../target/nsolver ../SampleInputOutput/input1.txt ../output/output1.txt"
else
    echo "Build failed!"
    exit 1
fi
