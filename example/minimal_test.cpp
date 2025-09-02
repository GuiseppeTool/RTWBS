#include "../src/timedautomaton.h"
#include <iostream>

int main() {
    std::cout << "Minimal Zone Graph Test\n";
    std::cout << "=======================\n\n";
    
    try {
        // Create the simplest possible automaton
        rtwbs::TimedAutomaton automaton(2); // reference + 1 clock
        
        // Add two locations with NO transitions
        automaton.add_location(0, "L0");
        automaton.add_location(1, "L1");
        
        std::cout << "Created automaton with 2 locations, NO transitions\n";
        
        // Test zone graph construction
        std::vector<raw_t> initial_zone(4, dbm_LE_ZERO); // 2x2 matrix
        
        std::cout << "Constructing zone graph...\n";
        automaton.construct_zone_graph(0, initial_zone, 10);
        
        size_t num_states = automaton.get_num_states();
        std::cout << "Total states: " << num_states << " (should be 1)\n";
        
        if (num_states == 1) {
            std::cout << "✅ PASS: Correct number of states\n";
        } else {
            std::cout << "❌ FAIL: Expected 1 state, got " << num_states << "\n";
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
