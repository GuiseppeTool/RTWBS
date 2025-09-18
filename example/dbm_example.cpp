#include "rtwbs.h"
#include <iostream>

using namespace rtwbs;

// Demonstrates manual DBM operations and zone graph construction using an ad-hoc XML asset.
int main(){
    try {
        std::cout << "=== DBM Operations & Zone Graph Example ===\n";
        // Load system from example XML (single template DBMOps)
        System sys("assets/example/dbm_operations.xml");
        if(sys.size()!=1){ std::cerr << "Unexpected template count" << std::endl; return 1; }
        auto &ta = sys.get_automaton(0);
        ta.construct_zone_graph();
        std::cout << "States constructed: " << ta.get_num_states() << "\n";
        const auto &states = ta.get_all_zone_states();
        // Print each state's DBM upper bound for clock 1 (if 2D ref+1)
        for(const auto &st_ptr : states){
            auto *st = st_ptr.get();
            std::cout << "Location " << st->location_id << ":";
            // decode bound x - 0
            int dim = st->dimension;
            raw_t cell = st->zone[dim*1 + 0]; // index (1,0)
            if(dbm_rawIsStrict(cell)) std::cout << " x < " << dbm_raw2bound(cell); else std::cout << " x <= " << dbm_raw2bound(cell);
            std::cout << "\n";
        }
        // Demonstrate manual DBM guard tightening
        if(!states.empty()){
            auto &z = states.front()->zone; // copy for demo
            std::vector<raw_t> demo = z;
            cindex_t dim = states.front()->dimension;
            std::cout << "Apply guard x <= 5 then closure...\n";
            dbm_constrain1(demo.data(), dim, 1, 0, dbm_bound2raw(5, dbm_WEAK));
            dbm_close(demo.data(), dim);
            raw_t cell = demo[dim*1 + 0];
            std::cout << "Result bound: " << (dbm_rawIsStrict(cell)?"x < ":"x <= ") << dbm_raw2bound(cell) << "\n";
        }
    } catch(const std::exception &e){
        std::cerr << "Error: " << e.what() << "\n"; return 1; }
    return 0;
}
