#include <iostream>
#include <vector>
#include "dbm/dbm.h"
#include "dbm/print.h"
#include "dbm/constraints.h"

// Simple example demonstrating basic DBM operations
int main() {
    std::cout << "Simple UDBM Example\n";
    std::cout << "===================\n\n";
    
    // Create a DBM with 2 clocks x1, x2 (plus reference clock x0)
    const cindex_t dim = 3;
    std::vector<raw_t> dbm(dim * dim);
    
    std::cout << "Creating a zone with 2 clocks (x1, x2)...\n\n";
    
    // Initialize DBM to unconstrained state
    dbm_init(dbm.data(), dim);
    
    std::cout << "1. Initial unconstrained DBM:\n";
    dbm_print(stdout, dbm.data(), dim);
    std::cout << "\n";
    
    // Add constraint: x1 <= 3
    dbm[1 * dim + 0] = dbm_bound2raw(3, dbm_WEAK);
    
    // Add constraint: x2 <= 5  
    dbm[2 * dim + 0] = dbm_bound2raw(5, dbm_WEAK);
    
    // Add constraint: x2 - x1 >= 1 (equivalent to x1 - x2 <= -1)
    dbm[1 * dim + 2] = dbm_bound2raw(-1, dbm_WEAK);
    
    std::cout << "2. After adding constraints:\n";
    std::cout << "   x1 <= 3\n";
    std::cout << "   x2 <= 5\n"; 
    std::cout << "   x2 - x1 >= 1\n";
    dbm_print(stdout, dbm.data(), dim);
    std::cout << "\n";
    
    // Compute closure (canonical form)
    bool consistent = dbm_close(dbm.data(), dim);
    
    std::cout << "3. After computing canonical form:\n";
    std::cout << "   Consistent: " << (consistent ? "Yes" : "No") << "\n";
    if (consistent) {
        dbm_print(stdout, dbm.data(), dim);
        std::cout << "\n";
        
        // Show derived constraints
        std::cout << "4. Analysis of the zone:\n";
        
        // Check bounds
        int32_t x1_upper = dbm_raw2bound(dbm[1 * dim + 0]);
        int32_t x2_upper = dbm_raw2bound(dbm[2 * dim + 0]);
        int32_t x1_x2_diff = dbm_raw2bound(dbm[1 * dim + 2]); 
        int32_t x2_x1_diff = dbm_raw2bound(dbm[2 * dim + 1]);
        
        std::cout << "   x1 <= " << x1_upper << "\n";
        std::cout << "   x2 <= " << x2_upper << "\n";
        std::cout << "   x1 - x2 <= " << x1_x2_diff << "\n";
        std::cout << "   x2 - x1 <= " << x2_x1_diff << "\n";
        
        std::cout << "\n   This represents the zone: ";
        std::cout << "{ (x1,x2) | " << x1_x2_diff << " <= x1-x2 <= " << -x2_x1_diff;
        std::cout << ", x1 <= " << x1_upper << ", x2 <= " << x2_upper << " }\n";
    }
    
    return 0;
}
