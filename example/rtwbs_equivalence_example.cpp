#include "rtwbs.h"
#include <iostream>

using namespace rtwbs;

int main(){
    try {
        std::cout << "=== RTWBS Equivalence Example (XML Driven) ===\n";
        // Load combined system (Abstract + Refined)
        System sys("assets/example/rtwbs_example.xml");
        if(sys.size()!=2){ std::cerr << "Unexpected number of templates in rtwbs_example.xml" << std::endl; return 1; }
        auto &abs = sys.get_automaton(0); // order as defined in XML system section: A, R
        auto &ref = sys.get_automaton(1);
        abs.construct_zone_graph();
        ref.construct_zone_graph();
        RTWBSChecker checker;
        bool ok = checker.check_rtwbs_equivalence(ref, abs);
        std::cout << "Refined REF <= Abstract ABS ? " << (ok?"YES":"NO") << "\n";
        checker.print_statistics();
    } catch(const std::exception &e){ 
        std::cerr << "Error: " << e.what() << "\n"; 
        return 1; 
    }
    return 0;
}

