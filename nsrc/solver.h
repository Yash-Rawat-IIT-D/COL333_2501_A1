#ifndef SOLVER_H
#define SOLVER_H

#include "utils.h"
#include <random>
#include <deque>

// Forward declarations for modular SA components
struct SolutionState;
struct Move;
struct InitializationParams;
struct CoolingParams;

/**
 * @brief Auxiliary state for incremental evaluation
 * Maintains running totals for efficient delta computation
 */
struct SolutionState {
    vector<double> foodDelivered;     // Food delivered per village (index by village_id-1)
    vector<double> otherDelivered;    // Other supplies per village
    vector<double> villageValues;     // Accumulated value per village
    vector<double> helicopterDistances; // Total distance per helicopter
    double totalObjective;            // Current objective value
    double totalValue;               // Total value delivered
    double totalCost;                // Total trip costs
    
    /**
     * @brief Initialize state from solution
     * @param config Problem configuration
     * @param solution Current solution
     */
    void initialize(const Scheduler_Config& config, const Solution& solution);
    
    /**
     * @brief Reset state to zero
     * @param numVillages Number of villages
     * @param numHelicopters Number of helicopters
     */
    void reset(int numVillages, int numHelicopters);
};

/**
 * @brief Abstract base for move operations
 * Supports delta evaluation and undo functionality
 */
struct Move {
    enum Type { SWAP_DROPS, RELOCATE_DROP, MIX_TWEAK };
    Type type;
    mutable int h1, t1, d1;  // Primary helicopter, trip, drop indices (mutable for modifications during apply)
    mutable int h2, t2, d2;  // Secondary indices (for relocate moves)
    
    Move() : type(SWAP_DROPS), h1(-1), t1(-1), d1(-1), h2(-1), t2(-1), d2(-1) {}
    
    /**
     * @brief Apply move to solution and update state
     * @param config Problem configuration
     * @param solution Solution to modify
     * @param state State to update incrementally
     * @return Delta in objective value
     */
    double apply(const Scheduler_Config& config, Solution& solution, SolutionState& state) const;
    
    /**
     * @brief Undo move and restore previous state
     * @param config Problem configuration  
     * @param solution Solution to restore
     * @param state State to restore
     * @param deltaObjective Previous delta to reverse
     */
    void undo(const Scheduler_Config& config, Solution& solution, SolutionState& state, double deltaObjective) const;
    
    /**
     * @brief Check if move is locally feasible without full evaluation
     * @param config Problem configuration
     * @param solution Current solution
     * @return true if move satisfies capacity/distance constraints
     */
    bool isLocallyFeasible(const Scheduler_Config& config, const Solution& solution) const;
    
    bool operator==(const Move& other) const;
};

/**
 * @brief Configuration for solution initialization strategies
 */
struct InitializationParams {
    enum Strategy { GREEDY, RANDOMIZED, NEAREST_NEIGHBOR };
    Strategy strategy = GREEDY;
    int randomSeed = 12345;
    double greedyBias = 1.0;  // Bias factor for greedy selection
};

/**
 * @brief Configuration for cooling schedule strategies  
 */
struct CoolingParams {
    enum Schedule { EXPONENTIAL, LINEAR, ADAPTIVE };
    Schedule schedule = EXPONENTIAL;
    double initialTemp = 10000.0;
    double coolingRate = 0.95;
    double minTemp = 0.01;
    bool adaptive = false;    // Adapt based on acceptance rate
};

/**
 * @brief Modular solution initialization
 * @param config Problem configuration
 * @param params Initialization strategy parameters
 * @return Initial feasible solution
 */
Solution generateInitialSolution(const Scheduler_Config& config, const InitializationParams& params);

/**
 * @brief Generate random move for SA neighborhood
 * @param solution Current solution  
 * @param rng Random number generator
 * @param config Problem configuration
 * @return Randomly generated move
 */
Move generateRandomMove(const Solution& solution, mt19937_64& rng, const Scheduler_Config& config);

/**
 * @brief Update temperature according to cooling schedule
 * @param currentTemp Current temperature
 * @param iteration Current iteration number
 * @param params Cooling parameters
 * @param acceptanceRate Recent acceptance rate (for adaptive)
 * @return Next temperature value
 */
double updateTemperature(double currentTemp, int iteration, const CoolingParams& params, double acceptanceRate = 0.0);

/**
 * @brief Determine if move should be accepted
 * @param deltaObjective Change in objective value
 * @param temperature Current temperature
 * @param rng Random number generator
 * @return true if move should be accepted
 */
bool acceptMove(double deltaObjective, double temperature, mt19937_64& rng);

/**
 * @brief Main modular simulated annealing solver
 * @param config Problem configuration containing all input data
 * @return Optimized solution
 */
Solution solve(const Scheduler_Config& config);

/**
 * @brief Legacy objective computation for validation/comparison
 * @param config The problem configuration
 * @param solution The solution to evaluate
 * @return The objective value (total value - total cost)
 */
double computeObjective(const Scheduler_Config& config, const Solution& solution);

/**
 * @brief Legacy feasibility check for validation/comparison
 * @param config The problem configuration
 * @param solution The solution to check
 * @return true if feasible, false otherwise
 */
bool isFeasible(const Scheduler_Config& config, const Solution& solution);

/**
 * @brief Write solution to output file in the required format
 * @param filename Output file path
 * @param solution The solution to write
 */
void writeOutputData(const string& filename, const Solution& solution);

#endif // SOLVER_H
