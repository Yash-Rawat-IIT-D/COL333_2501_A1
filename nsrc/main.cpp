#include <iostream>
#include <string>
#include <chrono>
#include "utils.h"
#include "solver.h"

using namespace std;

int main(int argc, char *argv[]) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <input_filename> <output_filename>" << endl;
        return 1;
    }

    string input_filename = argv[1];
    string output_filename = argv[2];

    try {
        // 1. Read problem data
        Scheduler_Config config;
        config.init(input_filename);
        cout << "Successfully read input file: " << input_filename << endl;

        // 2. Solve the problem
        auto start = chrono::steady_clock::now();
        Solution solution = solve(config);
        auto end = chrono::steady_clock::now();
        double elapsed = chrono::duration_cast<chrono::seconds>(end - start).count();
        cout << "Solver completed in " << elapsed << " seconds." << endl;

        // 3. Write output
        writeOutputData(output_filename, solution);
        cout << "Solution written to: " << output_filename << endl;

        // 4. Print objective value
        double objective = computeObjective(config, solution);
        cout << "Final objective value: " << objective << endl;
    }
    catch (const exception &e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }
    return 0;
}
