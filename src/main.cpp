#include <iostream>
#include <string>
#include "structures.h"
#include "io_handler.h"
#include "solver.h"

using namespace std;

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        cerr << "Usage: " << argv[0] << " <input_filename> <output_filename>" << endl;
        return 1;
    }

    string input_filename = argv[1];
    string output_filename = argv[2];

    try
    {
        // 1. Read problem data
        ProblemData problem = readInputData(input_filename);
        cout << "Successfully read input file: " << input_filename << endl;

        // enforce time limit
        auto start = chrono::steady_clock::now();
        Solution solution = solve(problem);
        auto end = chrono::steady_clock::now();
        double elapsed = chrono::duration_cast<chrono::seconds>(end - start).count();
        cout << "Solver completed in " << elapsed << " seconds." << endl;

        // Write output
        writeOutputData(output_filename, solution);
        cout << "Solution written to: " << output_filename << endl;
    }
    catch (const exception &e)
    {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }
    return 0;
}