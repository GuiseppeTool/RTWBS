#include "../src/timedautomaton.h"
#include "../src/rtwbs.h"
#include <iostream>
#include <exception>

int main(int argc, char* argv[]) {
    try {
        std::vector<std::string> filenames = {
            "assets/uppaal_benchmarks/modelConfStandard.xml"
        };
        rtwbs::RTWBSChecker checker;
        for (const auto& filename : filenames) {
            std::cout << "Processing " << filename << std::endl;
            rtwbs::System s(filename);
            //print all the variables and constants
            std::cout << "Running self-equivalence check..." << std::endl;
            bool equivalent = checker.check_rtwbs_equivalence(s, s);
            std::cout << "Self-equivalence result: " << (equivalent ? "EQUIVALENT" : "NOT EQUIVALENT") << std::endl;
            checker.print_statistics();
        }

        
        

        
        
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
