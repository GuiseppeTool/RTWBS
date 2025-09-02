#include "../src/timedautomaton.h"
#include <iostream>

int main() {
    std::cout << "Simple Synchronization Test\n";
    std::cout << "===========================\n\n";
    
    try {
        // Create a minimal timed automaton
        rtwbs::TimedAutomaton automaton(2); // reference clock + 1 clock
        
        // Add two locations
        automaton.add_location(0, "Start");
        automaton.add_location(1, "End");
        
        // Add a simple transition
        automaton.add_transition(0, 1, "go");
        
        std::cout << "Created simple automaton:\n";
        std::cout << "- 2 locations (Start -> End)\n";
        std::cout << "- 1 transition (go)\n";
        std::cout << "- 1 clock\n\n";
        
        // Test zone graph construction with minimal setup
        std::vector<raw_t> initial_zone(automaton.get_dimension() * automaton.get_dimension());
        // Initialize as zero zone
        for (size_t i = 0; i < initial_zone.size(); ++i) {
            initial_zone[i] = dbm_LE_ZERO;
        }
        
        std::cout << "Constructing zone graph...\n";
        automaton.construct_zone_graph(0, initial_zone);
        
        size_t num_states = automaton.get_num_states();
        std::cout << "Zone graph complete. Total states: " << num_states << "\n";
        
        if (num_states > 0 && num_states < 50) {
            std::cout << "\nReachable states:\n";
            automaton.print_all_states();
        }
        
        std::cout << "\nSimple test completed successfully!\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
