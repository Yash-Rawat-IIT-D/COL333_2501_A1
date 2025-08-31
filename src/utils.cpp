#include "utils.h"

// ------------ Pos2D ------------
Pos2D::Pos2D(double x, double y) : x(x), y(y) {}

double Pos2D::getX() const { return x; }
double Pos2D::getY() const { return y; }

double Pos2D::distanceTo(const Pos2D& other) const {
    return sqrt(pow(x - other.getX(), 2) + pow(y - other.getY(), 2));
}

// ------------ Packet ------------
Packet::Packet(double weight, double value) : weight(weight), value(value) {}

double Packet::getWeight() const { return weight; }
double Packet::getValue() const { return value; }

// ------------ City ------------
City::City(double x, double y) : pos_city(x, y) {}

double City::getX() const { return pos_city.getX(); }
double City::getY() const { return pos_city.getY(); }

double City::distanceTo(const City& other) const {
    return pos_city.distanceTo(other.pos_city);
}

// ------------ Village ------------
Village::Village(int id, double x, double y, int population) 
    : id(id), pos_village(x, y), population(population) {}

int Village::getId() const { return id; }
int Village::getPopulation() const { return population; }
double Village::getX() const { return pos_village.getX(); }
double Village::getY() const { return pos_village.getY(); }

double Village::distanceTo(const Village& other) const {
    return pos_village.distanceTo(other.pos_village);
}

// ------------ Copter ------------
Copter::Copter(int id, double max_weight, double max_distance, double fixed_cost, double variable_cost) 
    : id(id), max_weight(max_weight), max_distance(max_distance), 
      fixed_cost(fixed_cost), variable_cost(variable_cost) {}

int Copter::getId() const { return id; }
double Copter::getMaxWeight() const { return max_weight; }
double Copter::getMaxDistance() const { return max_distance; }

// ------------ Scheduler_Config ------------
Scheduler_Config::Scheduler_Config()
    : processing_time(ZERO_DOUBLE), max_travel_distance(ZERO_DOUBLE),
      packets(vector<Packet>(NUM_PACKET_TYPES)), copters({}), cities({}), villages({}) {}

void Scheduler_Config::init(const string& input_fpath) {
    ifstream fin(input_fpath);
    if (!fin.is_open()) {
        cerr << "Error: Could not open input file: " << input_fpath << endl;
        exit(1);
    }

    fin >> processing_time;
    fin >> max_travel_distance;

    for (int i = 0; i < NUM_PACKET_TYPES; i++) {
        double wt, val; fin >> wt >> val;
        packets[i] = Packet(wt, val);
    }

    int num_cities; fin >> num_cities; cities.clear();
    for (int i = 0; i < num_cities; ++i) {
        double x, y;
        fin >> x >> y;
        cities.emplace_back(x, y);
    }

    int num_villages; fin >> num_villages; villages.clear();
    for (int i = 0; i < num_villages; ++i) {
        double x, y; int n;
        fin >> x >> y >> n;
        villages.emplace_back(i, x, y, n);
    }

    int num_copters; fin >> num_copters; copters.clear();
    for (int i = 0; i < num_copters; ++i) {
        int home_city_id;
        double wcap, dcap, F, alpha;
        fin >> home_city_id >> wcap >> dcap >> F >> alpha;
        home_city_id--;
        copters.emplace_back(home_city_id, wcap, dcap, F, alpha);
    }

    fin.close();
}

void Scheduler_Config::print() {
    cout << "Processing Time: " << processing_time << " minutes" << endl;
    cout << "Max Travel Distance: " << max_travel_distance << " km" << endl;

    cout << "Packets (weight, value):" << endl;
    for (size_t i = 0; i < packets.size(); ++i) {
        cout << "  Type " << i+1 << ": (" 
             << packets[i].getWeight() << ", " << packets[i].getValue() << ")" << endl;
    }

    cout << "Cities:" << endl;
    for (size_t i = 0; i < cities.size(); ++i) {
        cout << "  City " << i+1 << ": (" 
             << cities[i].getX() << ", " << cities[i].getY() << ")" << endl;
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
