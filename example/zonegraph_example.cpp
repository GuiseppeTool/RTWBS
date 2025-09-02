#include <iostream>
#include <vector>
#include "dbm/dbm.h"
#include "dbm/constraints.h"
#include "timedautomaton.h"

using namespace rtwbs;

int main() {
    std::cout << "Timed Automaton Zone Graph Example\n";
    std::cout << "===================================\n\n";
    
    // Create a simple timed automaton with 2 clocks (x, y) + reference clock
    const cindex_t dim = 3;  // x0 (reference), x1 (x), x2 (y)
    
    TimedAutomaton ta(dim);
    
    // Add locations
    ta.add_location(0, "Initial");
    ta.add_location(1, "Middle");
    ta.add_location(2, "Final");
    
    // Add invariants
    // Location 0: x <= 5
    ta.add_invariant(0, 1, 0, 5, dbm_WEAK);
    
    // Location 1: y <= 10
    ta.add_invariant(1, 2, 0, 10, dbm_WEAK);
    
    // Location 2: no invariants
    
    // Add transitions
    ta.add_transition(0, 1, "a");  // Initial -> Middle
    ta.add_transition(1, 2, "b");  // Middle -> Final
    ta.add_transition(2, 0, "c");  // Final -> Initial (cycle)
    
    // Add guards to transitions (using transition indices)
    // Transition 0 (Initial -> Middle): x >= 3
    ta.add_guard(0, 0, 1, -3, dbm_WEAK);  // x0 - x1 <= -3, i.e., x1 >= 3
    
    // Transition 1 (Middle -> Final): y >= 2
    ta.add_guard(1, 0, 2, -2, dbm_WEAK);  // x0 - x2 <= -2, i.e., x2 >= 2
    
    // Transition 2 (Final -> Initial): no guards
    
    // Add resets to transitions
    // Transition 0: reset y
    ta.add_reset(0, 2);  // Reset clock y (index 2)
    
    // Transition 1: reset x
    ta.add_reset(1, 1);  // Reset clock x (index 1)
    
    // Transition 2: reset both x and y
    ta.add_reset(2, 1);  // Reset clock x
    ta.add_reset(2, 2);  // Reset clock y
    
    // Create initial zone (all clocks = 0)
    std::vector<raw_t> initial_zone(dim * dim);
    dbm_init(initial_zone.data(), dim);
    dbm_close(initial_zone.data(), dim);
    
    std::cout << "Initial zone:\n";
    dbm_print(stdout, initial_zone.data(), dim);
    std::cout << "\n";
    
    // Construct the zone graph
    std::cout << "Constructing zone graph...\n\n";
    ta.construct_zone_graph(0, initial_zone);
    
    // Print statistics
    ta.print_statistics();
    
    // Print all states
    std::cout << "All states in the zone graph:\n";
    std::cout << "=============================\n";
    ta.print_all_states();
    
    // Show reachability information
    std::cout << "Reachability information:\n";
    std::cout << "========================\n";
    for (size_t i = 0; i < ta.get_num_states(); ++i) {
        const auto& successors = ta.get_successors(i);
        std::cout << "State " << i << " -> {";
        for (size_t j = 0; j < successors.size(); ++j) {
            std::cout << successors[j];
            if (j < successors.size() - 1) std::cout << ", ";
        }
        std::cout << "}\n";
    }
    
    return 0;
}
