#include<bits/stdc++.h>
#include<stdio.h>
#include "utils.h"


int main() {
    // Example usage of the classes and methods defined in utils.h and utils.cpp
    Scheduler_Config config;
    string input_file = "input/foo.txt"; // Update with the correct path to your input file
    config.init(input_file);
    config.print();
    return 0;
}