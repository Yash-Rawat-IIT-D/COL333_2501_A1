#ifndef UTILS_H
#define UTILS_H

#include <bits/stdc++.h>
using namespace std;

const double ZERO_DOUBLE = 0.0;
const int ZERO_INT = 0;
const int NUM_PACKET_TYPES = 3; // dry, perishable, others
const int NUM_FOOD_PACKS = 9;   // 9 type of food packets per person
const int OTHER_PACKS = 1;      // 1 type of other packets per person

class Pos2D {
private:
    double x, y;

public:
    Pos2D(double x=0, double y=0);

    double getX() const;
    double getY() const;

    double distanceTo(const Pos2D& other) const;
};

class Packet {
    private:
        double weight;
        double value;

    public:
        Packet(double weight=0, double value=0);

        double getWeight() const;
        double getValue() const;
};

class City {
    private:
        Pos2D pos_city;

    public:
        City(double x, double y);

        double getX() const;
        double getY() const;
        double distanceTo(const City& other) const;
};

class Village {
private:
    int id;
    Pos2D pos_village;
    int population;

public:
    Village(int id, double x, double y, int population);

    int getId() const;
    int getPopulation() const;
    double getX() const;
    double getY() const;
    double distanceTo(const Village& other) const;
};

class Copter {
    private:
        int id;
        double max_weight;
        double max_distance;
        double fixed_cost;
        double variable_cost;

    public:
        Copter(int id, double max_weight, double max_distance, double fixed_cost, double variable_cost);

        int getId() const;
        double getMaxWeight() const;
        double getMaxDistance() const;
};

class Scheduler_Config {
    private:
        double processing_time;
        double max_travel_distance;
        vector<Packet> packets;
        vector<Copter> copters;
        vector<City> cities;
        vector<Village> villages;

    public:
        Scheduler_Config();

        void init(const string& input_fpath);
        void print();
};

#endif // UTILS_H
