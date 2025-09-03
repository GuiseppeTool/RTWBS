#include "../src/timedautomaton.h"
#include "../src/rtwbs.h"
#include <iostream>
#include <exception>

int main() {
    try {
        std::cout << "Testing problematic Test 9: Cyclic Resets..." << std::endl;
        
        std::cout << "Loading XML file..." << std::endl;
        rtwbs::TimedAutomaton automaton(std::string("unit_tests/test09_cyclic_resets.xml"));
        
        std::cout << "Automaton loaded successfully!" << std::endl;
        std::cout << "Dimension: " << automaton.get_dimension() << " clocks" << std::endl;
        
        std::cout << "Constructing zone graph..." << std::endl;
        automaton.construct_zone_graph();
        
        std::cout << "Zone graph construction completed!" << std::endl;
        automaton.print_statistics();
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
