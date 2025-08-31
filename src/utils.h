#ifndef UTILS_H
#define UTILS_H

#include<stdio.h>
#include<bits/stdc++.h>
#include<filesystem>
#include "utils.h"

using namespace std;

const double ZERO_DOUBLE = 0.0;
const int ZERO_INT = 0;
const int NUM_PACKET_TYPES = 3; // dry, perishable, others

class Pos2D {
    private:
        // Private members
        double x, y;
    public:
        // Public constructor and methods
        Pos2D(double x=0, double y=0) : x(x), y(y) {}

        // Getters
        double getX() const { return x; }
        double getY() const { return y; }

        // Method to calculate distance to another Pos2D
        double distanceTo(const Pos2D& other) const {
            return sqrt(pow(x - other.getX(), 2) + pow(y - other.getY(), 2));
        }
};

class Packet {
    private:
        double weight;
        double value;
    public:
        // Public constructor and methods
        Packet(double weight=0, double value=0) : weight(weight), value(value) {}

        // Getters
        double getWeight() const { return weight; }
        double getValue() const { return value; }
};

class City {
    private:
        // Private members
        Pos2D pos_city;

    public:
        // Public constructor and methods
        City(double x, double y) 
            : pos_city(x, y) {}
        
        // Getters
        double getX() const { return pos_city.getX(); }
        double getY() const { return pos_city.getY(); }

        // Method to calculate distance to another city
        double distanceTo(const City& other) const {
            return pos_city.distanceTo(other.pos_city);
        }
};

class Village {

    private:
        int id;
        Pos2D pos_village;
        int population;

    public:
        // Public constructor and methods
        Village(int id, double x, double y, int population) 
            : id(id), pos_village(x, y), population(population) {}
        
        // Getters
        int getId() const { return id; }
        int getPopulation() const { return population; }
        double getX() const { return pos_village.getX(); }
        double getY() const { return pos_village.getY(); }

        // Method to calculate distance to another village
        double distanceTo(const Village& other) const {
            return pos_village.distanceTo(other.pos_village);
        }

};

class Copter {
    private:
        // Private Members
        int id; // ID of the copter = City ID it belongs to
        double max_weight;
        double max_distance; // Maximum Distance a Copter can Travel in a single trip
        double fixed_cost;
        double variable_cost;
    
    public:
        // Public constructor and methods
        Copter(int id, double max_weight, double max_distance, double fixed_cost, double variable_cost) 
            : id(id), max_weight(max_weight), max_distance(max_distance),
              fixed_cost(fixed_cost), variable_cost(variable_cost) {} 

        // Getters
        int getId() const { return id; }
        double getMaxWeight() const { return max_weight; }
        double getMaxDistance() const { return max_distance; }
};

class Scheduler_Config {
    private:
        // Private Members
        double processing_time;     // Total Time to Solve the Problem
        double max_travel_distance; // Maximum Distance a Copter can Travel across all trips
        vector<Packet> packets;     // List of Packets available - dry, perishable, others
        vector<Copter> copters;     // List of Copters available
        vector<City> cities;        // List of Cities
        vector<Village> villages;   // List of Villages


    public:
        // Public constructor and methods
        Scheduler_Config() : processing_time(ZERO_DOUBLE), max_travel_distance(ZERO_DOUBLE), 
        packets(vector<Packet>(NUM_PACKET_TYPES)), copters({}), cities({}), villages({}) {}

        // Initialiser Method

        void init(const string& input_fpath) {
            ifstream fin(input_fpath);
            
            if (!fin.is_open()) {
                cerr << "Error: Could not open input file: " << input_fpath << endl;
                exit(1);
            }

            // 1. Total processing time (minutes)
            fin >> processing_time;

            // 2. DMax (max distance in km)
            fin >> max_travel_distance;

            // 3. Six numbers: w(d) v(d) w(p) v(p) w(o) v(o)
            
            assert(packets.size() == NUM_PACKET_TYPES);
            for(int i = 0; i < NUM_PACKET_TYPES; i++) {
                double wt, val; fin >> wt >> val; packets[i] = Packet(wt, val);
            }

            // 4. Cities
            int num_cities; fin >> num_cities; cities.clear();

            for (int i = 0; i < num_cities; ++i) {
                double x, y;
                fin >> x >> y;
                cities.emplace_back(x, y);
            }

            // 5. Villages
            int num_villages; fin >> num_villages; villages.clear();
            for (int i = 0; i < num_villages; ++i) {
                double x, y;
                int n;
                fin >> x >> y >> n;
                villages.emplace_back(i, x, y, n); // Let ID of village be 0-indexed
            }

            // 6. Helicopters
            int num_copters; fin >> num_copters; copters.clear();
            for (int i = 0; i < num_copters; ++i) {
                int home_city_id;
                double wcap, dcap, F, alpha;
                fin >> home_city_id >> wcap >> dcap >> F >> alpha; home_city_id--;  // Convert to 0-indexed
                copters.emplace_back(home_city_id, wcap, dcap, F, alpha);
            }

            fin.close();
        }

        void print() {
            cout << "Processing Time: " << processing_time << " minutes" << endl;
            cout << "Max Travel Distance: " << max_travel_distance << " km" << endl;

            cout << "Packets (weight, value):" << endl;
            for (size_t i = 0; i < packets.size(); ++i) {
                cout << "  Type " << i+1 << ": (" << packets[i].getWeight() << ", " << packets[i].getValue() << ")" << endl;
            }

            cout << "Cities:" << endl;
            for (size_t i = 0; i < cities.size(); ++i) {
                cout << "  City " << i+1 << ": (" << cities[i].getX() << ", " << cities[i].getY() << ")" << endl;
            }

            cout << "Villages:" << endl;
            for (size_t i = 0; i < villages.size(); ++i) {
                cout << "  Village ID " << villages[i].getId() + 1 << ": (" 
                     << villages[i].getX() << ", " << villages[i].getY() 
                     << "), Population: " << villages[i].getPopulation() << endl;
            }

            cout << "Copters:" << endl;
            for (size_t i = 0; i < copters.size(); ++i) {
                cout << "  Copter ID " << copters[i].getId() + 1 
                     << ": Max Weight: " << copters[i].getMaxWeight() 
                     << ", Max Distance: " << copters[i].getMaxDistance() 
                     << endl;
            }
        }
};   



#endif // UTILS_H
