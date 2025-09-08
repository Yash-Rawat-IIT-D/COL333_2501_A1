#include "solver.h"
#include <iostream>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <random>
#include <deque>

using namespace std;

inline double euclid(const Point &a, const Point &b)
{
    double dx = a.x - b.x;
    double dy = a.y - b.y;
    return sqrt(dx * dx + dy * dy);
}

// Evaluate a solution's objective: total value - total cost
static double computeObjective(const ProblemData &problem, const Solution &sol)
{
    int V = problem.villages.size();
    int H = problem.helicopters.size();
    vector<double> foodDelivered(V, 0.0), otherDelivered(V, 0.0);
    vector<double> villageVal(V, 0.0);
    vector<double> heliDist(H, 0.0);
    double totalTripCost = 0.0;
    // for each helicopter plan
    for (int hi = 0; hi < H; ++hi)
    {
        const auto &plan = sol[hi];
        const auto &heli = problem.helicopters[hi];
        Point home = problem.cities[heli.home_city_id - 1];
        Point curr;
        for (const auto &trip : plan.trips)
        {
            curr = home;
            double tripDist = 0.0;
            int totalDrops = 0;
            // weight check omitted here
            for (const auto &dp : trip.drops)
            {
                const auto &v = problem.villages[dp.village_id - 1];
                Point vp = v.coords;
                // distance to village
                tripDist += euclid(curr, vp);
                curr = vp;
                // evaluate value capping
                double maxFood = v.population * 9.0;
                double deliveredFood = dp.dry_food + dp.perishable_food;
                double roomFood = max(0.0, maxFood - foodDelivered[v.id - 1]);
                double useFood = min(deliveredFood, roomFood);
                double vpDelivered = min((double)dp.perishable_food, useFood);
                double vdRem = useFood - vpDelivered;
                double vdDelivered = min((double)dp.dry_food, vdRem);
                villageVal[v.id - 1] += vpDelivered * problem.packages[1].value + vdDelivered * problem.packages[0].value;
                foodDelivered[v.id - 1] += deliveredFood;
                // other
                double maxOther = v.population;
                double roomOther = max(0.0, maxOther - otherDelivered[v.id - 1]);
                double useOther = min((double)dp.other_supplies, roomOther);
                villageVal[v.id - 1] += useOther * problem.packages[2].value;
                otherDelivered[v.id - 1] += dp.other_supplies;
                totalDrops++;
            }
            // return home
            tripDist += euclid(curr, home);
            heliDist[hi] += tripDist;
            if (totalDrops > 0)
            {
                totalTripCost += heli.fixed_cost + heli.alpha * tripDist;
            }
        }
    }
    double totalValue = 0;
    for (double v : villageVal)
        totalValue += v;
    return totalValue - totalTripCost;
}

// Check solution feasibility against capacity and distance constraints
static bool isFeasible(const ProblemData &problem, const Solution &sol)
{
    int H = problem.helicopters.size();
    // per-helicopter cumulative distance
    vector<double> cumDist(H, 0.0);
    for (int hi = 0; hi < H; ++hi)
    {
        const auto &plan = sol[hi];
        const auto &heli = problem.helicopters[hi];
        Point home = problem.cities[heli.home_city_id - 1];
        double totalHeliDist = 0.0;
        for (const auto &trip : plan.trips)
        {
            // weight
            double w = trip.dry_food_pickup * problem.packages[0].weight + trip.perishable_food_pickup * problem.packages[1].weight + trip.other_supplies_pickup * problem.packages[2].weight;
            if (w > heli.weight_capacity + 1e-9)
                return false;
            // distance
            Point curr = home;
            double d = 0.0;
            for (const auto &dp : trip.drops)
            {
                const Point &vp = problem.villages[dp.village_id - 1].coords;
                d += euclid(curr, vp);
                curr = vp;
            }
            d += euclid(curr, home);
            if (d > heli.distance_capacity + 1e-9)
                return false;
            totalHeliDist += d;
        }
        if (totalHeliDist > problem.d_max + 1e-9)
            return false;
    }
    return true;
}

struct Move
{
    int type;
    int h1, t1, d1;
    int h2, t2, d2;
    bool operator==(Move const &o) const
    {
        return type == o.type && h1 == o.h1 && t1 == o.t1 && d1 == o.d1 && h2 == o.h2 && t2 == o.t2 && d2 == o.d2;
    }
};

Solution solve(const ProblemData &problem)
{
    cout << "Starting solver..." << endl;
    // greedy seed
    Solution greedy;
    int V = problem.villages.size();
    // remaining demand: food (9 per person), other (1 per person)
    vector<double> remFood(V), remOther(V);
    for (int i = 0; i < V; ++i)
    {
        remFood[i] = problem.villages[i].population * 9.0;
        remOther[i] = problem.villages[i].population * 1.0;
    }

    // for each helicopter: build multi-stop trips
    for (const auto &heli : problem.helicopters)
    {
        HelicopterPlan plan;
        plan.helicopter_id = heli.id;
        double usedDist = 0.0;
        Point home = problem.cities[heli.home_city_id - 1];

        // repeatedly start new trips
        while (true)
        {
            // initialize one trip
            Trip trip;
            trip.dry_food_pickup = trip.perishable_food_pickup = trip.other_supplies_pickup = 0;
            trip.drops.clear();
            double tripWeight = 0.0;
            double tripDist = 0.0;
            Point curr = home;
            vector<bool> visited(V, false);
            bool added = false;

            // greedy add villages to this trip
            while (true)
            {
                double bestScore = -1.0;
                int bestV = -1;
                double addDist = 0.0;
                // find best next village
                for (int i = 0; i < V; ++i)
                {
                    if (remFood[i] <= 0 && remOther[i] <= 0)
                        continue;
                    if (visited[i])
                        continue;
                    const Point &vp = problem.villages[i].coords;
                    double d1 = euclid(curr, vp);
                    double d2 = euclid(vp, home);
                    // check distance feasibility
                    if (tripDist + d1 + d2 > heli.distance_capacity)
                        continue;
                    if (usedDist + tripDist + d1 + d2 > problem.d_max)
                        continue;
                    // compute how much can load for this stop
                    double remW = heli.weight_capacity - tripWeight;
                    const auto &pkg = problem.packages;
                    long loadP = min((long)floor(remW / pkg[1].weight), (long)ceil(remFood[i]));
                    double wP = loadP * pkg[1].weight;
                    long loadD = min((long)floor((remW - wP) / pkg[0].weight), (long)ceil(remFood[i] - loadP));
                    double wD = loadD * pkg[0].weight;
                    long loadO = min((long)floor((remW - wP - wD) / pkg[2].weight), (long)ceil(remOther[i]));
                    if (loadP + loadD + loadO == 0)
                        continue;
                    // score: marginal value per marginal distance
                    double deltaDist = d1 + d2;
                    double deltaValue = loadP * pkg[1].value + loadD * pkg[0].value + loadO * pkg[2].value;
                    double score = deltaValue / deltaDist;
                    if (score > bestScore)
                    {
                        bestScore = score;
                        bestV = i;
                        addDist = d1;
                    }
                }
                if (bestV < 0)
                    break;
                // add bestV to trip
                visited[bestV] = true;
                added = true;
                const auto &vp = problem.villages[bestV].coords;
                double d1 = euclid(curr, vp);
                // compute loads again
                double remW = heli.weight_capacity - tripWeight;
                const auto &pkg = problem.packages;
                long loadP = min((long)floor(remW / pkg[1].weight), (long)ceil(remFood[bestV]));
                double wP = loadP * pkg[1].weight;
                long loadD = min((long)floor((remW - wP) / pkg[0].weight), (long)ceil(remFood[bestV] - loadP));
                double wD = loadD * pkg[0].weight;
                long loadO = min((long)floor((remW - wP - wD) / pkg[2].weight), (long)ceil(remOther[bestV]));
                // update trip totals
                trip.perishable_food_pickup += loadP;
                trip.dry_food_pickup += loadD;
                trip.other_supplies_pickup += loadO;
                tripWeight += wP + wD + loadO * pkg[2].weight;
                tripDist += d1;
                // record drop
                Drop dp;
                dp.village_id = problem.villages[bestV].id;
                dp.perishable_food = loadP;
                dp.dry_food = loadD;
                dp.other_supplies = loadO;
                trip.drops.push_back(dp);
                // update remaining demands & position
                remFood[bestV] = max(0.0, remFood[bestV] - (loadP + loadD));
                remOther[bestV] = max(0.0, remOther[bestV] - loadO);
                curr = vp;
            }
            // finish trip if any drop made
            if (!added)
                break;
            // return home distance
            double back = euclid(curr, home);
            tripDist += back;
            usedDist += tripDist;
            plan.trips.push_back(trip);
        }
        greedy.push_back(plan);
    }

    // simulated annealing refinement
    Solution current = greedy, best = greedy;
    double bestObj = computeObjective(problem, best);
    double curObj = bestObj;
    double T = max(1.0, fabs(bestObj));
    auto start = chrono::steady_clock::now();
    double timeLim = problem.time_limit_minutes * 60.0;
    mt19937_64 rng(123456);
    uniform_real_distribution<double> unif(0.0, 1.0);
    int H = problem.helicopters.size();

    // tabu list
    const int TABU_SIZE = 100;
    deque<Move> tabu;

    // SA main loop
    while (chrono::duration<double>(chrono::steady_clock::now() - start).count() < timeLim)
    {
        Move mv{0, 0, 0, 0, 0, 0, 0};
        Solution cand;
        // generate a tabu-free neighbor
        do
        {
            cand = current;
            // weighted move selection: 0=swap, 1=move-drop, 2=duplicate-trip, 3=remove-empty, 4=add-drop, 5=split/merge
            double r = unif(rng);
            if (r < 0.05)
                mv.type = 0;
            else if (r < 0.3)
                mv.type = 1;
            else if (r < 0.5)
                mv.type = 2;
            else if (r < 0.6)
                mv.type = 3;
            else if (r < 0.8)
                mv.type = 4;
            else
                mv.type = 5;
            if (mv.type == 0)
            {
                // swap two drops in a random trip
                int hi = rng() % H;
                auto &trips = cand[hi].trips;
                if (!trips.empty())
                {
                    int ti = rng() % trips.size();
                    auto &drops = trips[ti].drops;
                    if (drops.size() > 1)
                    {
                        int i = rng() % drops.size();
                        int j = rng() % drops.size();
                        swap(drops[i], drops[j]);
                    }
                }
            }
            else if (mv.type == 1)
            {
                // move a drop to another trip
                // check for any trip with at least one drop; else fallback to swap
                bool hasDrops = false;
                for (int h = 0; h < H && !hasDrops; h++)
                {
                    for (auto &t : cand[h].trips)
                        if (!t.drops.empty())
                        {
                            hasDrops = true;
                            break;
                        }
                }
                if (hasDrops)
                {
                    int hi1 = rng() % H;
                    auto &tr1 = cand[hi1].trips;
                    if (!tr1.empty())
                    {
                        int t1 = rng() % tr1.size();
                        if (!tr1[t1].drops.empty())
                        {
                            mv.h1 = hi1;
                            mv.t1 = t1;
                            mv.d1 = rng() % tr1[t1].drops.size();
                            Drop dp = tr1[t1].drops[mv.d1];
                            tr1[t1].drops.erase(tr1[t1].drops.begin() + mv.d1);
                            tr1[t1].perishable_food_pickup -= dp.perishable_food;
                            tr1[t1].dry_food_pickup -= dp.dry_food;
                            tr1[t1].other_supplies_pickup -= dp.other_supplies;
                            int hi2 = rng() % H;
                            auto &tr2 = cand[hi2].trips;
                            if (tr2.empty())
                            {
                                Trip nt;
                                nt.drops.push_back(dp);
                                nt.perishable_food_pickup = dp.perishable_food;
                                nt.dry_food_pickup = dp.dry_food;
                                nt.other_supplies_pickup = dp.other_supplies;
                                tr2.push_back(nt);
                                mv.h2 = hi2;
                                mv.t2 = 0;
                                mv.d2 = 0;
                            }
                            else
                            {
                                mv.h2 = hi2;
                                int t2 = rng() % tr2.size();
                                mv.t2 = t2;
                                tr2[t2].drops.push_back(dp);
                                tr2[t2].perishable_food_pickup += dp.perishable_food;
                                tr2[t2].dry_food_pickup += dp.dry_food;
                                tr2[t2].other_supplies_pickup += dp.other_supplies;
                                mv.d2 = tr2[t2].drops.size() - 1;
                            }
                        }
                    }
                }
            }
            else if (mv.type == 2)
            {
                // duplicate a random trip into a (possibly other) helicopter
                int hi = rng() % H;
                auto &trips1 = cand[hi].trips;
                if (!trips1.empty())
                {
                    int ti = rng() % trips1.size();
                    Trip copy = trips1[ti];
                    int hj = rng() % H;
                    cand[hj].trips.push_back(copy);
                    mv.h1 = hi;
                    mv.t1 = ti;
                    mv.h2 = hj;
                    mv.t2 = cand[hj].trips.size() - 1;
                }
            }
            else if (mv.type == 3)
            {
                // remove an empty trip if any
                int hi = rng() % H;
                auto &trips = cand[hi].trips;
                vector<int> empties;
                for (int ti = 0; ti < (int)trips.size(); ++ti)
                    if (trips[ti].drops.empty())
                        empties.push_back(ti);
                if (!empties.empty())
                {
                    int idx = empties[rng() % empties.size()];
                    trips.erase(trips.begin() + idx);
                    mv.h1 = hi;
                    mv.t1 = idx;
                }
            }
            else if (mv.type == 4)
            {
                // add a minimal drop to a trip (1 perishable)
                int hi = rng() % H;
                auto &trips = cand[hi].trips;
                int ti;
                if (trips.empty())
                {
                    trips.emplace_back();
                    ti = 0;
                }
                else
                    ti = rng() % trips.size();
                // create drop
                Drop dp;
                dp.village_id = problem.villages[rng() % V].id;
                dp.perishable_food = 1;
                dp.dry_food = 0;
                dp.other_supplies = 0;
                trips[ti].drops.push_back(dp);
                trips[ti].perishable_food_pickup += 1;
                mv.h1 = hi;
                mv.t1 = ti;
            }
            else // mv.type == 5
            {
                // split or merge a trip
                int hi = rng() % H;
                auto &trips = cand[hi].trips;
                if (!trips.empty() && unif(rng) < 0.5)
                {
                    // split a trip with >=2 drops
                    vector<int> can;
                    for (int ti = 0; ti < (int)trips.size(); ++ti)
                        if (trips[ti].drops.size() >= 2)
                            can.push_back(ti);
                    if (!can.empty())
                    {
                        int ti = can[rng() % can.size()];
                        int n = trips[ti].drops.size();
                        int split = 1 + rng() % (n - 1);
                        Trip newt;
                        newt.perishable_food_pickup = newt.dry_food_pickup = newt.other_supplies_pickup = 0;
                        // move second half to new trip
                        for (int i = split; i < n; ++i)
                        {
                            auto dp = trips[ti].drops[i];
                            newt.drops.push_back(dp);
                            newt.perishable_food_pickup += dp.perishable_food;
                            newt.dry_food_pickup += dp.dry_food;
                            newt.other_supplies_pickup += dp.other_supplies;
                        }
                        trips[ti].drops.erase(trips[ti].drops.begin() + split, trips[ti].drops.end());
                        // adjust pickups of original
                        trips[ti].perishable_food_pickup -= newt.perishable_food_pickup;
                        trips[ti].dry_food_pickup -= newt.dry_food_pickup;
                        trips[ti].other_supplies_pickup -= newt.other_supplies_pickup;
                        trips.insert(trips.begin() + ti + 1, newt);
                        mv.h1 = hi;
                        mv.t1 = ti;
                        mv.h2 = hi;
                        mv.t2 = ti + 1;
                    }
                }
                else if (trips.size() >= 2)
                {
                    // merge two trips
                    int i1 = rng() % trips.size();
                    int i2 = rng() % trips.size();
                    if (i1 == i2)
                        i2 = (i1 + 1) % trips.size();
                    int ti = min(i1, i2), tj = max(i1, i2);
                    // append drops
                    for (auto &dp : trips[tj].drops)
                    {
                        trips[ti].drops.push_back(dp);
                        trips[ti].perishable_food_pickup += dp.perishable_food;
                        trips[ti].dry_food_pickup += dp.dry_food;
                        trips[ti].other_supplies_pickup += dp.other_supplies;
                    }
                    trips.erase(trips.begin() + tj);
                    mv.h1 = hi;
                    mv.t1 = ti;
                }
            }
        } while (find(tabu.begin(), tabu.end(), mv) != tabu.end());

        // skip infeasible candidates
        if (!isFeasible(problem, cand))
        {
            // reduce temperature and continue
            T *= 0.95;
            continue;
        }
        double candObj = computeObjective(problem, cand);
        double delta = candObj - curObj;
        if (delta > 0 || unif(rng) < exp(delta / T))
        {
            current = cand;
            curObj = candObj;
            tabu.push_back(mv);
            if (tabu.size() > TABU_SIZE)
                tabu.pop_front();
            if (curObj > bestObj)
            {
                best = current;
                bestObj = curObj;
            }
            // bulk convert dry -> perishable where possible
            for (int hi = 0; hi < H; ++hi)
            {
                const auto &heli = problem.helicopters[hi];
                double wcap = heli.weight_capacity;
                for (auto &trip : current[hi].trips)
                {
                    // compute used weight
                    double used = trip.dry_food_pickup * problem.packages[0].weight + trip.perishable_food_pickup * problem.packages[1].weight + trip.other_supplies_pickup * problem.packages[2].weight;
                    double space = wcap - used;
                    // each dry->perishable swap frees (w0-w1)
                    double deltaW = problem.packages[0].weight - problem.packages[1].weight;
                    if (deltaW <= 0)
                        continue;
                    for (auto &dp : trip.drops)
                    {
                        int canSwap = min(dp.dry_food,
                                          (int)floor(space / deltaW));
                        if (canSwap <= 0)
                            continue;
                        dp.dry_food -= canSwap;
                        dp.perishable_food += canSwap;
                        trip.dry_food_pickup -= canSwap;
                        trip.perishable_food_pickup += canSwap;
                        space -= canSwap * deltaW;
                    }
                }
            }
        }
        T *= 0.95;
    }
    cout << "Solver finished." << endl;
    return best;
}
