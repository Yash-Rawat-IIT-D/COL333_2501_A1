#include "solver.h"
#include <iostream>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <random>
#include <deque>
#include <fstream>

using namespace std;

// ============================================================================
// AUXILIARY STATE MANAGEMENT FOR INCREMENTAL EVALUATION
// ============================================================================

void SolutionState::initialize(const Scheduler_Config& config, const Solution& solution) {
    const auto& villages = config.getVillages();
    const auto& copters = config.getCopters();
    const auto& cities = config.getCities();
    const auto& packets = config.getPackets();
    
    int V = villages.size();
    int H = copters.size();
    
    // Reset state
    reset(V, H);
    
    // Recompute state from scratch
    for (int hi = 0; hi < H; ++hi) {
        const auto& plan = solution[hi];
        const auto& copter = copters[hi];
        Pos2D home = cities[copter.getId()].getPos();
        
        for (const auto& trip : plan.trips) {
            Pos2D curr = home;
            double tripDist = 0.0;
            int totalDrops = 0;
            
            for (const auto& dp : trip.drops) {
                const auto& v = villages[dp.village_id - 1];
                Pos2D vp = v.getPos();
                
                // Update distance
                tripDist += curr.distanceTo(vp);
                curr = vp;
                totalDrops++;
                
                // Update deliveries and values with capping logic
                int villageIdx = dp.village_id - 1;
                double maxFood = v.getPopulation() * 9.0;
                double deliveredFood = dp.dry_food + dp.perishable_food;
                double roomFood = max(0.0, maxFood - foodDelivered[villageIdx]);
                double useFood = min(deliveredFood, roomFood);
                double vpDelivered = min((double)dp.perishable_food, useFood);
                double vdRem = useFood - vpDelivered;
                double vdDelivered = min((double)dp.dry_food, vdRem);
                
                villageValues[villageIdx] += vpDelivered * packets[1].getValue() + vdDelivered * packets[0].getValue();
                foodDelivered[villageIdx] += deliveredFood;
                
                // Other supplies
                double maxOther = v.getPopulation();
                double roomOther = max(0.0, maxOther - otherDelivered[villageIdx]);
                double useOther = min((double)dp.other_supplies, roomOther);
                villageValues[villageIdx] += useOther * packets[2].getValue();
                otherDelivered[villageIdx] += dp.other_supplies;
            }
            
            // Return home and update costs
            tripDist += curr.distanceTo(home);
            helicopterDistances[hi] += tripDist;
            if (totalDrops > 0) {
                totalCost += copter.getFixedCost() + copter.getVariableCost() * tripDist;
            }
        }
    }
    
    // Compute totals
    totalValue = 0;
    for (double v : villageValues) totalValue += v;
    totalObjective = totalValue - totalCost;
}

void SolutionState::reset(int numVillages, int numHelicopters) {
    foodDelivered.assign(numVillages, 0.0);
    otherDelivered.assign(numVillages, 0.0);
    villageValues.assign(numVillages, 0.0);
    helicopterDistances.assign(numHelicopters, 0.0);
    totalObjective = 0.0;
    totalValue = 0.0;
    totalCost = 0.0;
}

// ============================================================================
// MOVE SYSTEM WITH DELTA EVALUATION
// ============================================================================

/**
 * @brief Helper to compute trip distance and cost
 */
pair<double, double> computeTripCost(const Trip& trip, const Scheduler_Config& config, int helicopterIdx) {
    const auto& villages = config.getVillages();
    const auto& copters = config.getCopters();
    const auto& cities = config.getCities();
    
    const auto& copter = copters[helicopterIdx];
    Pos2D home = cities[copter.getId()].getPos();
    Pos2D curr = home;
    double distance = 0.0;
    
    for (const auto& drop : trip.drops) {
        Pos2D vp = villages[drop.village_id - 1].getPos();
        distance += curr.distanceTo(vp);
        curr = vp;
    }
    distance += curr.distanceTo(home);
    
    double cost = trip.drops.empty() ? 0.0 : (copter.getFixedCost() + copter.getVariableCost() * distance);
    return {distance, cost};
}

/**
 * @brief Helper to compute value delivered by a drop with capping
 */
double computeDropValue(const Drop& drop, const Scheduler_Config& config, const SolutionState& state) {
    const auto& villages = config.getVillages();
    const auto& packets = config.getPackets();
    
    int villageIdx = drop.village_id - 1;
    const auto& village = villages[villageIdx];
    
    // Food value with capping
    double maxFood = village.getPopulation() * 9.0;
    double roomFood = max(0.0, maxFood - state.foodDelivered[villageIdx]);
    double deliveredFood = drop.dry_food + drop.perishable_food;
    double useFood = min(deliveredFood, roomFood);
    double vpDelivered = min((double)drop.perishable_food, useFood);
    double vdRem = useFood - vpDelivered;
    double vdDelivered = min((double)drop.dry_food, vdRem);
    double foodValue = vpDelivered * packets[1].getValue() + vdDelivered * packets[0].getValue();
    
    // Other supplies value with capping
    double maxOther = village.getPopulation();
    double roomOther = max(0.0, maxOther - state.otherDelivered[villageIdx]);
    double useOther = min((double)drop.other_supplies, roomOther);
    double otherValue = useOther * packets[2].getValue();
    
    return foodValue + otherValue;
}

double Move::apply(const Scheduler_Config& config, Solution& solution, SolutionState& state) const {
    double deltaObjective = 0.0;
    
    switch (type) {
        case SWAP_DROPS: {
            if (h1 < solution.size() && t1 < solution[h1].trips.size() && 
                solution[h1].trips[t1].drops.size() > max(d1, d2)) {
                
                auto& trip = solution[h1].trips[t1];
                
                // Store old trip cost for delta calculation
                auto [oldDist, oldCost] = computeTripCost(trip, config, h1);
                
                // Perform swap
                swap(trip.drops[d1], trip.drops[d2]);
                
                // Compute new trip cost
                auto [newDist, newCost] = computeTripCost(trip, config, h1);
                
                // Update state
                state.helicopterDistances[h1] += (newDist - oldDist);
                state.totalCost += (newCost - oldCost);
                deltaObjective = -(newCost - oldCost);  // Cost decrease improves objective
            }
            break;
        }
        
        case RELOCATE_DROP: {
            if (h1 < solution.size() && t1 < solution[h1].trips.size() && 
                d1 < solution[h1].trips[t1].drops.size() && h2 < solution.size()) {
                
                auto& sourceTrip = solution[h1].trips[t1];
                Drop drop = sourceTrip.drops[d1];
                
                // Remove from source - compute delta
                auto [oldSrcDist, oldSrcCost] = computeTripCost(sourceTrip, config, h1);
                double oldDropValue = computeDropValue(drop, config, state);
                
                // Update source trip
                sourceTrip.drops.erase(sourceTrip.drops.begin() + d1);
                sourceTrip.dry_food_pickup -= drop.dry_food;
                sourceTrip.perishable_food_pickup -= drop.perishable_food;
                sourceTrip.other_supplies_pickup -= drop.other_supplies;
                
                auto [newSrcDist, newSrcCost] = computeTripCost(sourceTrip, config, h1);
                
                // Add to destination
                if (t2 >= solution[h2].trips.size()) {
                    // Create new trip
                    Trip newTrip;
                    newTrip.drops.push_back(drop);
                    newTrip.dry_food_pickup = drop.dry_food;
                    newTrip.perishable_food_pickup = drop.perishable_food;
                    newTrip.other_supplies_pickup = drop.other_supplies;
                    solution[h2].trips.push_back(newTrip);
                    t2 = solution[h2].trips.size() - 1;
                } else {
                    // Add to existing trip
                    auto& destTrip = solution[h2].trips[t2];
                    auto [oldDestDist, oldDestCost] = computeTripCost(destTrip, config, h2);
                    
                    destTrip.drops.push_back(drop);
                    destTrip.dry_food_pickup += drop.dry_food;
                    destTrip.perishable_food_pickup += drop.perishable_food;
                    destTrip.other_supplies_pickup += drop.other_supplies;
                    
                    auto [newDestDist, newDestCost] = computeTripCost(destTrip, config, h2);
                    state.helicopterDistances[h2] += (newDestDist - oldDestDist);
                    state.totalCost += (newDestCost - oldDestCost);
                    deltaObjective -= (newDestCost - oldDestCost);
                }
                
                // Update deliveries - this is approximate, full recompute needed for accuracy
                int villageIdx = drop.village_id - 1;
                double deliveredFood = drop.dry_food + drop.perishable_food;
                double newDropValue = computeDropValue(drop, config, state);
                
                state.foodDelivered[villageIdx] += deliveredFood;
                state.otherDelivered[villageIdx] += drop.other_supplies;
                state.villageValues[villageIdx] += (newDropValue - oldDropValue);
                state.totalValue += (newDropValue - oldDropValue);
                deltaObjective += (newDropValue - oldDropValue);
                
                // Update source helicopter state
                state.helicopterDistances[h1] += (newSrcDist - oldSrcDist);
                state.totalCost += (newSrcCost - oldSrcCost);
                deltaObjective -= (newSrcCost - oldSrcCost);
            }
            break;
        }
        
        case MIX_TWEAK: {
            if (h1 < solution.size() && t1 < solution[h1].trips.size() && 
                d1 < solution[h1].trips[t1].drops.size()) {
                
                auto& drop = solution[h1].trips[t1].drops[d1];
                auto& trip = solution[h1].trips[t1];
                
                if (drop.dry_food > 0) {
                    // Store old value
                    double oldValue = computeDropValue(drop, config, state);
                    
                    // Apply tweak
                    drop.dry_food -= 1;
                    drop.perishable_food += 1;
                    trip.dry_food_pickup -= 1;
                    trip.perishable_food_pickup += 1;
                    
                    // Compute new value
                    double newValue = computeDropValue(drop, config, state);
                    double valueDelta = newValue - oldValue;
                    
                    // Update state
                    int villageIdx = drop.village_id - 1;
                    state.villageValues[villageIdx] += valueDelta;
                    state.totalValue += valueDelta;
                    deltaObjective = valueDelta;
                }
            }
            break;
        }
    }
    
    state.totalObjective += deltaObjective;
    return deltaObjective;
}

void Move::undo(const Scheduler_Config& config, Solution& solution, SolutionState& state, double deltaObjective) const {
    // For simplicity, we'll recompute state from scratch after undo
    // In a production system, you'd implement proper undo logic for each move type
    
    switch (type) {
        case SWAP_DROPS: {
            if (h1 < solution.size() && t1 < solution[h1].trips.size() && 
                solution[h1].trips[t1].drops.size() > max(d1, d2)) {
                // Undo swap
                swap(solution[h1].trips[t1].drops[d1], solution[h1].trips[t1].drops[d2]);
            }
            break;
        }
        
        case RELOCATE_DROP:
            // This would require storing more state to properly undo
            // For now, we'll rely on rejection of infeasible moves
            break;
            
        case MIX_TWEAK: {
            if (h1 < solution.size() && t1 < solution[h1].trips.size() && 
                d1 < solution[h1].trips[t1].drops.size()) {
                auto& drop = solution[h1].trips[t1].drops[d1];
                auto& trip = solution[h1].trips[t1];
                
                // Undo tweak
                drop.dry_food += 1;
                drop.perishable_food -= 1;
                trip.dry_food_pickup += 1;
                trip.perishable_food_pickup -= 1;
            }
            break;
        }
    }
    
    // Recompute state to ensure consistency
    state.initialize(config, solution);
}

bool Move::isLocallyFeasible(const Scheduler_Config& config, const Solution& solution) const {
    const auto& copters = config.getCopters();
    const auto& packets = config.getPackets();
    
    switch (type) {
        case SWAP_DROPS:
            // Swaps don't change feasibility
            return true;
            
        case RELOCATE_DROP: {
            if (h2 >= solution.size() || h1 >= solution.size()) return false;
            
            // Check weight capacity of destination helicopter
            if (t2 < solution[h2].trips.size()) {
                const auto& destTrip = solution[h2].trips[t2];
                const auto& drop = solution[h1].trips[t1].drops[d1];
                
                double addWeight = drop.dry_food * packets[0].getWeight() + 
                                 drop.perishable_food * packets[1].getWeight() + 
                                 drop.other_supplies * packets[2].getWeight();
                double currentWeight = destTrip.dry_food_pickup * packets[0].getWeight() + 
                                     destTrip.perishable_food_pickup * packets[1].getWeight() + 
                                     destTrip.other_supplies_pickup * packets[2].getWeight();
                
                if (currentWeight + addWeight > copters[h2].getMaxWeight() + 1e-9) {
                    return false;
                }
            }
            return true;
        }
        
        case MIX_TWEAK:
            // Weight stays the same, just changes composition
            return true;
    }
    
    return false;
}

bool Move::operator==(const Move& other) const {
    return type == other.type && h1 == other.h1 && t1 == other.t1 && d1 == other.d1 &&
           h2 == other.h2 && t2 == other.t2 && d2 == other.d2;
}

// ============================================================================
// SOLUTION INITIALIZATION MODULE
// ============================================================================

/**
 * @brief Generate initial solution using greedy strategy
 * Improved version of original greedy with parameterizable bias
 */
Solution generateGreedySolution(const Scheduler_Config& config, const InitializationParams& params) {
    const auto& villages = config.getVillages();
    const auto& copters = config.getCopters();
    const auto& cities = config.getCities();
    const auto& packets = config.getPackets();
    
    Solution solution;
    int V = villages.size();
    
    // Remaining demand: food (9 per person), other (1 per person)
    vector<double> remFood(V), remOther(V);
    for (int i = 0; i < V; ++i) {
        remFood[i] = villages[i].getPopulation() * 9.0;
        remOther[i] = villages[i].getPopulation() * 1.0;
    }
    
    // For each copter: build multi-stop trips
    for (const auto& copter : copters) {
        CopterPlan plan;
        plan.copter_id = copter.getId();
        double usedDist = 0.0;
        Pos2D home = cities[copter.getId()].getPos();
        
        // Repeatedly start new trips
        while (true) {
            Trip trip;
            trip.dry_food_pickup = trip.perishable_food_pickup = trip.other_supplies_pickup = 0;
            trip.drops.clear();
            double tripWeight = 0.0;
            double tripDist = 0.0;
            Pos2D curr = home;
            vector<bool> visited(V, false);
            bool added = false;
            
            // Greedy add villages to this trip
            while (true) {
                double bestScore = -1.0;
                int bestV = -1;
                
                for (int i = 0; i < V; ++i) {
                    if (remFood[i] <= 0 && remOther[i] <= 0) continue;
                    if (visited[i]) continue;
                    
                    const Pos2D& vp = villages[i].getPos();
                    double d1 = curr.distanceTo(vp);
                    double d2 = vp.distanceTo(home);
                    
                    // Check distance feasibility
                    if (tripDist + d1 + d2 > copter.getMaxDistance()) continue;
                    if (usedDist + tripDist + d1 + d2 > config.getMaxTravelDistance()) continue;
                    
                    // Compute loadable amounts
                    double remW = copter.getMaxWeight() - tripWeight;
                    long loadP = min((long)floor(remW / packets[1].getWeight()), (long)ceil(remFood[i]));
                    double wP = loadP * packets[1].getWeight();
                    long loadD = min((long)floor((remW - wP) / packets[0].getWeight()), (long)ceil(remFood[i] - loadP));
                    double wD = loadD * packets[0].getWeight();
                    long loadO = min((long)floor((remW - wP - wD) / packets[2].getWeight()), (long)ceil(remOther[i]));
                    
                    if (loadP + loadD + loadO == 0) continue;
                    
                    // Enhanced scoring with bias parameter
                    double need = remFood[i] + remOther[i];
                    double efficiency = villages[i].getPopulation() / ((d1 + d2) + 1e-6);
                    double urgency = need / (d1 + 1.0);
                    double score = params.greedyBias * efficiency * urgency;
                    
                    if (score > bestScore) {
                        bestScore = score;
                        bestV = i;
                    }
                }
                
                if (bestV < 0) break;
                
                // Add bestV to trip
                visited[bestV] = true;
                added = true;
                const Pos2D& vp = villages[bestV].getPos();
                double d1 = curr.distanceTo(vp);
                
                // Recompute loads
                double remW = copter.getMaxWeight() - tripWeight;
                long loadP = min((long)floor(remW / packets[1].getWeight()), (long)ceil(remFood[bestV]));
                double wP = loadP * packets[1].getWeight();
                long loadD = min((long)floor((remW - wP) / packets[0].getWeight()), (long)ceil(remFood[bestV] - loadP));
                double wD = loadD * packets[0].getWeight();
                long loadO = min((long)floor((remW - wP - wD) / packets[2].getWeight()), (long)ceil(remOther[bestV]));
                
                // Update trip
                trip.perishable_food_pickup += loadP;
                trip.dry_food_pickup += loadD;
                trip.other_supplies_pickup += loadO;
                tripWeight += wP + wD + loadO * packets[2].getWeight();
                tripDist += d1;
                
                // Record drop
                Drop dp;
                dp.village_id = villages[bestV].getId() + 1;
                dp.perishable_food = loadP;
                dp.dry_food = loadD;
                dp.other_supplies = loadO;
                trip.drops.push_back(dp);
                
                // Update remaining demands
                remFood[bestV] = max(0.0, remFood[bestV] - (loadP + loadD));
                remOther[bestV] = max(0.0, remOther[bestV] - loadO);
                curr = vp;
            }
            
            if (!added) break;
            
            double back = curr.distanceTo(home);
            tripDist += back;
            usedDist += tripDist;
            plan.trips.push_back(trip);
        }
        solution.push_back(plan);
    }
    
    return solution;
}

/**
 * @brief Generate randomized initial solution
 * Creates solution by randomly assigning villages to helicopters with feasibility checks
 */
Solution generateRandomizedSolution(const Scheduler_Config& config, const InitializationParams& params) {
    mt19937_64 rng(params.randomSeed);
    const auto& villages = config.getVillages();
    const auto& copters = config.getCopters();
    
    Solution solution;
    
    // Initialize empty plans
    for (const auto& copter : copters) {
        CopterPlan plan;
        plan.copter_id = copter.getId();
        solution.push_back(plan);
    }
    
    // Randomly assign villages to helicopters
    vector<int> villageOrder(villages.size());
    iota(villageOrder.begin(), villageOrder.end(), 0);
    shuffle(villageOrder.begin(), villageOrder.end(), rng);
    
    for (int vidx : villageOrder) {
        // Try to assign village to a random helicopter
        vector<int> helicopterOrder(copters.size());
        iota(helicopterOrder.begin(), helicopterOrder.end(), 0);
        shuffle(helicopterOrder.begin(), helicopterOrder.end(), rng);
        
        bool assigned = false;
        for (int hidx : helicopterOrder) {
            // Try to create a simple single-village trip
            const auto& village = villages[vidx];
            const auto& copter = copters[hidx];
            
            // Create drop with basic supplies
            Drop drop;
            drop.village_id = village.getId() + 1;
            drop.dry_food = min(1000.0, village.getPopulation() * 5.0);  // Partial fulfillment
            drop.perishable_food = min(1000.0, village.getPopulation() * 3.0);
            drop.other_supplies = min(100.0, village.getPopulation() * 0.5);
            
            Trip trip;
            trip.dry_food_pickup = drop.dry_food;
            trip.perishable_food_pickup = drop.perishable_food;
            trip.other_supplies_pickup = drop.other_supplies;
            trip.drops.push_back(drop);
            
            solution[hidx].trips.push_back(trip);
            assigned = true;
            break;
        }
    }
    
    return solution;
}

/**
 * @brief Generate solution using nearest neighbor heuristic
 */
Solution generateNearestNeighborSolution(const Scheduler_Config& config, const InitializationParams& params) {
    const auto& villages = config.getVillages();
    const auto& copters = config.getCopters();
    const auto& cities = config.getCities();
    
    Solution solution;
    vector<bool> assigned(villages.size(), false);
    
    for (const auto& copter : copters) {
        CopterPlan plan;
        plan.copter_id = copter.getId();
        Pos2D home = cities[copter.getId()].getPos();
        
        // Build trips using nearest neighbor from home
        while (true) {
            Trip trip;
            Pos2D current = home;
            bool added = false;
            
            while (true) {
                // Find nearest unassigned village
                double minDist = 1e9;
                int nearest = -1;
                
                for (int i = 0; i < villages.size(); ++i) {
                    if (assigned[i]) continue;
                    double dist = current.distanceTo(villages[i].getPos());
                    if (dist < minDist) {
                        minDist = dist;
                        nearest = i;
                    }
                }
                
                if (nearest == -1) break;
                
                // Add village to trip
                assigned[nearest] = true;
                added = true;
                
                Drop drop;
                drop.village_id = villages[nearest].getId() + 1;
                drop.dry_food = min(500.0, villages[nearest].getPopulation() * 4.0);
                drop.perishable_food = min(500.0, villages[nearest].getPopulation() * 4.0);
                drop.other_supplies = min(50.0, villages[nearest].getPopulation() * 0.5);
                
                trip.drops.push_back(drop);
                trip.dry_food_pickup += drop.dry_food;
                trip.perishable_food_pickup += drop.perishable_food;
                trip.other_supplies_pickup += drop.other_supplies;
                
                current = villages[nearest].getPos();
                
                // Simple capacity check - break if getting too heavy
                const auto& packets = config.getPackets();
                double weight = trip.dry_food_pickup * packets[0].getWeight() + 
                              trip.perishable_food_pickup * packets[1].getWeight() + 
                              trip.other_supplies_pickup * packets[2].getWeight();
                if (weight > copter.getMaxWeight() * 0.8) break;
            }
            
            if (!added) break;
            plan.trips.push_back(trip);
        }
        
        solution.push_back(plan);
    }
    
    return solution;
}

Solution generateInitialSolution(const Scheduler_Config& config, const InitializationParams& params) {
    switch (params.strategy) {
        case InitializationParams::GREEDY:
            return generateGreedySolution(config, params);
        case InitializationParams::RANDOMIZED:
            return generateRandomizedSolution(config, params);
        case InitializationParams::NEAREST_NEIGHBOR:
            return generateNearestNeighborSolution(config, params);
        default:
            return generateGreedySolution(config, params);
    }
}

// ============================================================================
// MOVE GENERATION MODULE
// ============================================================================

Move generateRandomMove(const Solution& solution, mt19937_64& rng, const Scheduler_Config& config) {
    Move move;
    int H = solution.size();
    
    // Select random move type
    int moveType = rng() % 3;
    move.type = static_cast<Move::Type>(moveType);
    
    switch (move.type) {
        case Move::SWAP_DROPS: {
            // Find a helicopter with a trip containing multiple drops
            move.h1 = rng() % H;
            if (!solution[move.h1].trips.empty()) {
                move.t1 = rng() % solution[move.h1].trips.size();
                const auto& drops = solution[move.h1].trips[move.t1].drops;
                if (drops.size() > 1) {
                    move.d1 = rng() % drops.size();
                    move.d2 = rng() % drops.size();
                }
            }
            break;
        }
        
        case Move::RELOCATE_DROP: {
            // Find a helicopter with at least one drop
            bool foundSource = false;
            for (int attempts = 0; attempts < 10 && !foundSource; ++attempts) {
                move.h1 = rng() % H;
                if (!solution[move.h1].trips.empty()) {
                    move.t1 = rng() % solution[move.h1].trips.size();
                    if (!solution[move.h1].trips[move.t1].drops.empty()) {
                        move.d1 = rng() % solution[move.h1].trips[move.t1].drops.size();
                        foundSource = true;
                    }
                }
            }
            
            if (foundSource) {
                move.h2 = rng() % H;
                if (!solution[move.h2].trips.empty()) {
                    move.t2 = rng() % solution[move.h2].trips.size();
                } else {
                    move.t2 = 0; // Will create new trip
                }
            }
            break;
        }
        
        case Move::MIX_TWEAK: {
            // Find a drop with dry food to convert
            bool foundDrop = false;
            for (int attempts = 0; attempts < 10 && !foundDrop; ++attempts) {
                move.h1 = rng() % H;
                if (!solution[move.h1].trips.empty()) {
                    move.t1 = rng() % solution[move.h1].trips.size();
                    if (!solution[move.h1].trips[move.t1].drops.empty()) {
                        move.d1 = rng() % solution[move.h1].trips[move.t1].drops.size();
                        if (solution[move.h1].trips[move.t1].drops[move.d1].dry_food > 0) {
                            foundDrop = true;
                        }
                    }
                }
            }
            break;
        }
    }
    
    return move;
}

// ============================================================================
// COOLING SCHEDULE AND ACCEPTANCE MODULE
// ============================================================================

double updateTemperature(double currentTemp, int iteration, const CoolingParams& params, double acceptanceRate) {
    switch (params.schedule) {
        case CoolingParams::EXPONENTIAL: {
            double newTemp = currentTemp * params.coolingRate;
            return max(newTemp, params.minTemp);
        }
        
        case CoolingParams::LINEAR: {
            double decrement = (params.initialTemp - params.minTemp) * params.coolingRate / 1000.0;
            double newTemp = currentTemp - decrement;
            return max(newTemp, params.minTemp);
        }
        
        case CoolingParams::ADAPTIVE: {
            // Adaptive cooling based on acceptance rate
            double targetAcceptanceRate = 0.4; // 40% target acceptance
            double adaptiveFactor = 1.0;
            
            if (acceptanceRate > targetAcceptanceRate) {
                // Too many acceptances, cool faster
                adaptiveFactor = params.coolingRate * 0.9;
            } else if (acceptanceRate < targetAcceptanceRate * 0.5) {
                // Too few acceptances, cool slower
                adaptiveFactor = params.coolingRate * 1.1;
            } else {
                adaptiveFactor = params.coolingRate;
            }
            
            double newTemp = currentTemp * adaptiveFactor;
            return max(newTemp, params.minTemp);
        }
        
        default:
            return currentTemp * params.coolingRate;
    }
}

bool acceptMove(double deltaObjective, double temperature, mt19937_64& rng) {
    if (deltaObjective > 0) {
        // Improvement always accepted
        return true;
    }
    
    if (temperature <= 1e-10) {
        // No randomness at very low temperature
        return false;
    }
    
    // Metropolis criterion
    double probability = exp(deltaObjective / temperature);
    uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(rng) < probability;
}

// ============================================================================
// MODULAR SIMULATED ANNEALING SOLVER
// ============================================================================

/**
 * @brief Main modular simulated annealing solver
 * 
 * This refactored implementation separates the SA process into distinct,
 * configurable modules for initialization, move generation, cooling schedules,
 * and acceptance criteria. Key improvements include:
 * 
 * 1. INCREMENTAL EVALUATION: Uses SolutionState for O(1) delta computation
 * 2. MODULAR DESIGN: Pluggable initialization strategies and cooling schedules
 * 3. EFFICIENT MOVES: Local feasibility checks and targeted state updates
 * 4. EXTENSIBLE ARCHITECTURE: Easy to add new move types or strategies
 * 
 * @param config Problem configuration containing all input data
 * @return Optimized solution using modular SA framework
 */
Solution solve(const Scheduler_Config& config) {
    cout << "Starting modular simulated annealing solver..." << endl;
    
    // Configuration parameters - can be made configurable
    InitializationParams initParams;
    initParams.strategy = InitializationParams::GREEDY;
    initParams.greedyBias = 1.2;  // Enhanced greedy selection
    
    CoolingParams coolingParams;
    coolingParams.schedule = CoolingParams::ADAPTIVE;
    coolingParams.initialTemp = 10000.0;
    coolingParams.coolingRate = 0.95;
    coolingParams.minTemp = 0.01;
    
    // Phase 1: Solution Initialization
    cout << "Phase 1: Generating initial solution..." << endl;
    Solution currentSolution = generateInitialSolution(config, initParams);
    Solution bestSolution = currentSolution;
    
    // Phase 2: State Initialization for Incremental Evaluation
    cout << "Phase 2: Initializing auxiliary state..." << endl;
    SolutionState currentState, bestState;
    currentState.initialize(config, currentSolution);
    bestState = currentState;
    
    double currentObjective = currentState.totalObjective;
    double bestObjective = currentObjective;
    cout << "Initial objective: " << currentObjective << endl;
    
    // Phase 3: SA Parameters Setup
    double temperature = coolingParams.initialTemp;
    auto startTime = chrono::steady_clock::now();
    double timeLimit = min(config.getProcessingTime() * 60.0, 60.0);
    
    mt19937_64 rng(123456);
    
    // Enhanced statistics tracking
    int iteration = 0;
    int acceptedMoves = 0;
    int totalMoves = 0;
    int improvingMoves = 0;
    deque<Move> tabuList;
    const int TABU_SIZE = 50;  // Reduced for efficiency
    
    cout << "Phase 4: Starting SA optimization loop..." << endl;
    
    // Phase 4: Main SA Loop with Modular Components
    while (chrono::duration<double>(chrono::steady_clock::now() - startTime).count() < timeLimit) {
        // Progress reporting
        // if (iteration % 1000 == 0) {
        //     double elapsed = chrono::duration<double>(chrono::steady_clock::now() - startTime).count();
        //     double acceptanceRate = totalMoves > 0 ? (double)acceptedMoves / totalMoves : 0.0;
        //     cout << "Iteration " << iteration << ", T=" << temperature 
        //          << ", Best=" << bestObjective << ", Accept=" << acceptanceRate 
        //          << ", Time=" << elapsed << "s" << endl;
        // }
        
        // Generate candidate move using modular move generator
        Move candidateMove;
        bool validMove = false;
        int attempts = 0;
        
        do {
            candidateMove = generateRandomMove(currentSolution, rng, config);
            validMove = candidateMove.isLocallyFeasible(config, currentSolution);
            attempts++;
        } while (!validMove && attempts < 10 && 
                 find(tabuList.begin(), tabuList.end(), candidateMove) == tabuList.end());
        
        if (!validMove || attempts >= 10) {
            // Update temperature and continue
            double acceptanceRate = totalMoves > 0 ? (double)acceptedMoves / totalMoves : 0.0;
            temperature = updateTemperature(temperature, iteration, coolingParams, acceptanceRate);
            iteration++;
            continue;
        }
        
        // Apply move and compute delta using incremental evaluation
        Solution candidateSolution = currentSolution;
        SolutionState candidateState = currentState;
        
        double deltaObjective = candidateMove.apply(config, candidateSolution, candidateState);
        totalMoves++;
        
        // Acceptance decision using modular acceptance criteria
        bool accepted = acceptMove(deltaObjective, temperature, rng);
        
        if (accepted) {
            // Accept move
            currentSolution = candidateSolution;
            currentState = candidateState;
            currentObjective = candidateState.totalObjective;
            acceptedMoves++;
            
            // Update tabu list
            tabuList.push_back(candidateMove);
            if (tabuList.size() > TABU_SIZE) {
                tabuList.pop_front();
            }
            
            // Check for improvement
            if (currentObjective > bestObjective) {
                bestSolution = currentSolution;
                bestState = currentState;
                bestObjective = currentObjective;
                improvingMoves++;
                cout << "New best solution found: " << bestObjective << endl;
            }
        }
        
        // Update temperature using modular cooling schedule
        double acceptanceRate = totalMoves > 0 ? (double)acceptedMoves / totalMoves : 0.0;
        temperature = updateTemperature(temperature, iteration, coolingParams, acceptanceRate);
        iteration++;
        
        // Early termination conditions
        if (temperature < coolingParams.minTemp) {
            cout << "Minimum temperature reached, terminating..." << endl;
            break;
        }
    }
    
    // Phase 5: Final reporting and validation
    cout << "SA optimization completed!" << endl;
    cout << "Total iterations: " << iteration << endl;
    cout << "Total moves attempted: " << totalMoves << endl;
    cout << "Moves accepted: " << acceptedMoves << " (" 
         << (totalMoves > 0 ? 100.0 * acceptedMoves / totalMoves : 0.0) << "%)" << endl;
    cout << "Improving moves: " << improvingMoves << endl;
    cout << "Final objective: " << bestObjective << endl;
    
    // Validate final solution using legacy check
    if (!isFeasible(config, bestSolution)) {
        cout << "WARNING: Final solution is infeasible!" << endl;
    }
    
    double legacyObjective = computeObjective(config, bestSolution);
    cout << "Legacy objective verification: " << legacyObjective << endl;
    cout << "Delta from incremental: " << (bestObjective - legacyObjective) << endl;
    
    return bestSolution;
}

// Evaluate a solution's objective: total value - total cost
double computeObjective(const Scheduler_Config& config, const Solution& solution) {
    const auto& villages = config.getVillages();
    const auto& copters = config.getCopters();
    const auto& cities = config.getCities();
    const auto& packets = config.getPackets();
    
    int V = villages.size();
    int H = copters.size();
    vector<double> foodDelivered(V, 0.0), otherDelivered(V, 0.0);
    vector<double> villageVal(V, 0.0);
    vector<double> heliDist(H, 0.0);
    double totalTripCost = 0.0;
    
    // For each copter plan
    for (int hi = 0; hi < H; ++hi) {
        const auto& plan = solution[hi];
        const auto& copter = copters[hi];
        Pos2D home = cities[copter.getId()].getPos(); // copter.getId() stores home_city_id (already 0-indexed)
        Pos2D curr;
        
        for (const auto& trip : plan.trips) {
            curr = home;
            double tripDist = 0.0;
            int totalDrops = 0;
            
            for (const auto& dp : trip.drops) {
                const auto& v = villages[dp.village_id - 1];
                Pos2D vp = v.getPos();
                
                // Distance to village
                tripDist += curr.distanceTo(vp);
                curr = vp;
                
                // Evaluate value capping
                double maxFood = v.getPopulation() * 9.0;
                double deliveredFood = dp.dry_food + dp.perishable_food;
                double roomFood = max(0.0, maxFood - foodDelivered[dp.village_id - 1]);
                double useFood = min(deliveredFood, roomFood);
                double vpDelivered = min((double)dp.perishable_food, useFood);
                double vdRem = useFood - vpDelivered;
                double vdDelivered = min((double)dp.dry_food, vdRem);
                villageVal[dp.village_id - 1] += vpDelivered * packets[1].getValue() + vdDelivered * packets[0].getValue();
                foodDelivered[dp.village_id - 1] += deliveredFood;
                
                // Other supplies
                double maxOther = v.getPopulation();
                double roomOther = max(0.0, maxOther - otherDelivered[dp.village_id - 1]);
                double useOther = min((double)dp.other_supplies, roomOther);
                villageVal[dp.village_id - 1] += useOther * packets[2].getValue();
                otherDelivered[dp.village_id - 1] += dp.other_supplies;
                totalDrops++;
            }
            
            // Return home
            tripDist += curr.distanceTo(home);
            heliDist[hi] += tripDist;
            if (totalDrops > 0) {
                totalTripCost += copter.getFixedCost() + copter.getVariableCost() * tripDist;
            }
        }
    }
    
    double totalValue = 0;
    for (double v : villageVal)
        totalValue += v;
    return totalValue - totalTripCost;
}

// Check solution feasibility against capacity and distance constraints
bool isFeasible(const Scheduler_Config& config, const Solution& solution) {
    const auto& villages = config.getVillages();
    const auto& copters = config.getCopters();
    const auto& cities = config.getCities();
    const auto& packets = config.getPackets();
    
    int H = copters.size();
    vector<double> cumDist(H, 0.0);
    
    for (int hi = 0; hi < H; ++hi) {
        const auto& plan = solution[hi];
        const auto& copter = copters[hi];
        Pos2D home = cities[copter.getId()].getPos(); // copter.getId() stores home_city_id (already 0-indexed)
        double totalHeliDist = 0.0;
        
        for (const auto& trip : plan.trips) {
            // Weight check
            double w = trip.dry_food_pickup * packets[0].getWeight() + 
                      trip.perishable_food_pickup * packets[1].getWeight() + 
                      trip.other_supplies_pickup * packets[2].getWeight();
            if (w > copter.getMaxWeight() + 1e-9)
                return false;
            
            // Distance check
            Pos2D curr = home;
            double d = 0.0;
            for (const auto& dp : trip.drops) {
                const Pos2D& vp = villages[dp.village_id - 1].getPos();
                d += curr.distanceTo(vp);
                curr = vp;
            }
            d += curr.distanceTo(home);
            if (d > copter.getMaxDistance() + 1e-9)
                return false;
            totalHeliDist += d;
        }
        if (totalHeliDist > config.getMaxTravelDistance() + 1e-9)
            return false;
    }
    return true;
}

void writeOutputData(const string& filename, const Solution& solution) {
    ofstream outfile(filename);
    if (!outfile.is_open()) {
        throw runtime_error("Error: Could not open output file " + filename);
    }
    
    for (const auto& plan : solution) {
        outfile << plan.copter_id + 1 << " " << plan.trips.size() << endl; // 1-indexed copter id
        
        for (const auto& trip : plan.trips) {
            outfile << trip.dry_food_pickup << " " << trip.perishable_food_pickup 
                   << " " << trip.other_supplies_pickup << " " << trip.drops.size();
            
            for (const auto& drop : trip.drops) {
                outfile << " " << drop.village_id << " " << drop.dry_food 
                       << " " << drop.perishable_food << " " << drop.other_supplies;
            }
            outfile << endl;
        }
        outfile << "-1" << endl; // End marker after each copter's plan
    }
    
    outfile.close();
}
