#include "../src/timedautomaton.h"
#include <iostream>

int main() {
    std::cout << "Single Transition Test\n";
    std::cout << "======================\n\n";
    
    try {
        // Create automaton with one simple transition
        rtwbs::TimedAutomaton automaton(2); // reference + 1 clock
        
        automaton.add_location(0, "Start");
        automaton.add_location(1, "End");
        
        // Add ONE transition with NO guards, NO resets
        automaton.add_transition(0, 1, "simple");
        
        std::cout << "Created: Start --simple--> End (no guards, no resets)\n";
        
        // Test zone graph construction
        std::vector<raw_t> initial_zone(4, dbm_LE_ZERO);
        
        std::cout << "Constructing zone graph...\n";
        automaton.construct_zone_graph(0, initial_zone, 10);
        
        size_t num_states = automaton.get_num_states();
        std::cout << "Total states: " << num_states << " (should be 2)\n";
        
        if (num_states == 2) {
            std::cout << "✅ PASS: Correct number of states\n";
        } else {
            std::cout << "❌ FAIL: Expected 2 states, got " << num_states << "\n";
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
