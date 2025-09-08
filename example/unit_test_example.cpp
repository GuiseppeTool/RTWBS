#include "rtwbs.h"
#include <iostream>
#include <exception>

int main() {
    try {
        std::cout << "Testing RTWBS Unit Test XML Model" << std::endl;
        std::cout << "=================================" << std::endl;
        
        // Test the simplest model first
        std::cout << "\n1. Loading simple sequential automaton..." << std::endl;
        rtwbs::TimedAutomaton automaton(std::string("../unit_tests/test01_simple_sequential.xml"));
        
        std::cout << "   Automaton loaded successfully!" << std::endl;
        std::cout << "   Dimension: " << automaton.get_dimension() << " clocks" << std::endl;
        
        std::cout << "\n2. Constructing zone graph..." << std::endl;
        automaton.construct_zone_graph();
        
        std::cout << "   Zone graph construction completed!" << std::endl;
        
        std::cout << "\n3. Printing statistics..." << std::endl;
        automaton.print_statistics();
        
        std::cout << "\n4. Printing all transitions..." << std::endl;
        automaton.print_all_transitions();
        
        std::cout << "\n✓ Test completed successfully!" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Error: " << e.what() << std::endl;
        return 1;
    }
}
