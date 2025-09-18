// We extend RTWBSChecker minimally to expose the cached helpers for demonstration.
#include "rtwbs.h"
#include <iostream>

using namespace rtwbs;



int main(){
    try {
        std::cout << "=== Weak Successors Example (Library Caching) ===\n";
        System sys("assets/example/weak_successors.xml");
        auto &ta = sys.get_automaton(0);
        ta.construct_zone_graph();
        const auto &states = ta.get_all_zone_states();
        if(states.empty()){ std::cerr << "No states" << std::endl; return 1; }
        const ZoneState* init = states.front().get();
        rtwbs::ExposedChecker checker; 
        auto &succ = checker.weak_successors(ta, init, "a!");
        std::cout << "Cached weak successors for action a!: " << succ.size() << " states\n";
        for(auto s: succ){ std::cout << "  loc " << s->location_id << "\n"; }
    } catch(const std::exception &e){ std::cerr << "Error: " << e.what() << "\n"; return 1; }
    return 0;
}
