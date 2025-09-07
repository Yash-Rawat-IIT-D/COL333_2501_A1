# New Class-Based Implementation (nsrc)

This directory contains a re-imagined implementation of the assignment solution using a proper object-oriented design with the classes defined in `utils.h` and `utils.cpp`.

## Key Features

### Class Structure
- **Pos2D**: Represents 2D coordinates with distance calculation capabilities
- **Packet**: Encapsulates package information (weight, value)  
- **City**: Represents city locations
- **Village**: Represents village data (ID, position, population)
- **Copter**: Represents helicopter specifications and capabilities
- **Scheduler_Config**: Main configuration class that reads and manages all problem data

### Solution Structures
- **Drop**: Represents a delivery drop at a village
- **Trip**: Represents a single trip with multiple drops
- **CopterPlan**: Complete plan for one copter
- **Solution**: Collection of all copter plans

## Algorithm
The solver implements a hybrid approach:
1. **Greedy Construction**: Creates initial feasible solution by greedily assigning villages to trips
2. **Simulated Annealing**: Refines the solution using local search with:
   - Drop reordering within trips
   - Moving drops between trips/copters  
   - Load mix adjustments

## Building and Running

```bash
# Build the project
make clean && make

# Or use the build script
./build.sh

# Run with input file
../target/nsolver <input_file> <output_file>

# Example
../target/nsolver ../SampleInputOutput/input1.txt ../output/output1.txt
```

## Files
- `utils.h/cpp`: Core data structures and classes
- `solver.h/cpp`: Main solver algorithm and solution evaluation
- `main.cpp`: Entry point and I/O handling
- `Makefile`: Build configuration
- `build.sh`: Convenience build script

## Compatibility
The implementation maintains full compatibility with the original format checker and produces valid solutions that satisfy all constraints. The output format matches the expected specification.

## Performance
- All test cases pass validation
- Achieves competitive objective scores:
  - Input1: 27660
  - Input2: 3550.13  
  - Input3: 2978.31

The class-based design provides better code organization, maintainability, and extensibility compared to the original struct-based approach.
