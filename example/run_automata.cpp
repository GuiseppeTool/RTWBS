#include "rtwbs.h"
#include <iostream>
#include <exception>

int main(int argc, char* argv[]) {
    try {
        std::string filename = "assets/FMICS2021/modelConfStandard.xml";
        if (argc > 1) {
            filename = std::string(argv[1]);
        }
        
        std::cout << "Loading " << filename << std::endl;
        
        std::cout << "Loading XML file..." << std::endl;
        rtwbs::TimedAutomaton automaton(filename);
        
        std::cout << "Automaton loaded successfully!" << std::endl;
        std::cout << "Dimension: " << automaton.get_dimension() << " clocks" << std::endl;
        
        std::cout << "Constructing zone graph..." << std::endl;
        automaton.construct_zone_graph();
        
        std::cout << "Zone graph construction completed!" << std::endl;
        automaton.print_statistics();

        rtwbs::RTWBSChecker checker;
        std::cout << "Running self-equivalence check..." << std::endl;
        bool equivalent = checker.check_rtwbs_equivalence(automaton, automaton);
        std::cout << "Self-equivalence result: " << (equivalent ? "EQUIVALENT" : "NOT EQUIVALENT") << std::endl;
        checker.print_statistics();
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
