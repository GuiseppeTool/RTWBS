#include <iostream>
#include <vector>
#include "dbm/dbm.h"
#include "dbm/print.h"
#include "dbm/constraints.h"

int main() {
    std::cout << "UDBM Library Test\n";
    std::cout << "==================\n\n";
    
    // Create a simple DBM with 3 clocks (dimension 4 including reference clock x0)
    const cindex_t dim = 4; // x0 (reference), x1, x2, x3
    std::vector<raw_t> dbm(dim * dim);
    
    // Initialize the DBM
    dbm_init(dbm.data(), dim);
    
    std::cout << "1. Initialized DBM (dimension " << dim << "):\n";
    dbm_print(stdout, dbm.data(), dim);
    std::cout << "\n";
    
    // Check if DBM contains zero point
    bool hasZero = dbm_hasZero(dbm.data(), dim);
    std::cout << "2. DBM contains zero point: " << (hasZero ? "Yes" : "No") << "\n\n";
    
    // Add some constraints: x1 - x0 <= 5 (x1 <= 5)
    // DBM format: dbm[i*dim + j] represents constraint xi - xj <= c
    // For x1 <= 5, we set dbm[1*dim + 0] = constraint for x1 - x0 <= 5
    dbm[1 * dim + 0] = dbm_bound2raw(5, dbm_WEAK); // x1 - x0 <= 5
    
    // Add constraint: x2 - x0 <= 10 (x2 <= 10)
    dbm[2 * dim + 0] = dbm_bound2raw(10, dbm_WEAK); // x2 - x0 <= 10
    
    // Add constraint: x1 - x2 <= -2 (x2 - x1 >= 2, or x1 - x2 <= -2)
    dbm[1 * dim + 2] = dbm_bound2raw(-2, dbm_WEAK); // x1 - x2 <= -2
    
    std::cout << "3. DBM after adding constraints:\n";
    std::cout << "   x1 <= 5\n";
    std::cout << "   x2 <= 10\n";
    std::cout << "   x1 - x2 <= -2 (equivalent to x2 - x1 >= 2)\n";
    dbm_print(stdout, dbm.data(), dim);
    std::cout << "\n";
    
    // Close the DBM (compute transitive closure)
    bool isConsistent = dbm_close(dbm.data(), dim);
    
    std::cout << "4. DBM after closure (transitive closure computed):\n";
    std::cout << "   Is consistent: " << (isConsistent ? "Yes" : "No") << "\n";
    if (isConsistent) {
        dbm_print(stdout, dbm.data(), dim);
    }
    std::cout << "\n";
    
    // Check if DBM is empty
    bool isEmpty = dbm_isEmpty(dbm.data(), dim);
    std::cout << "5. DBM is empty: " << (isEmpty ? "Yes" : "No") << "\n\n";
    
    // Test constraint satisfaction
    if (isConsistent && !isEmpty) {
        std::cout << "6. Testing constraint properties:\n";
        
        // Test if the DBM still contains zero
        hasZero = dbm_hasZero(dbm.data(), dim);
        std::cout << "   Contains zero point: " << (hasZero ? "Yes" : "No") << "\n";
        
        // Extract bounds for individual clocks
        raw_t x1_bound = dbm[1 * dim + 0]; // x1 - x0 <= bound
        raw_t x2_bound = dbm[2 * dim + 0]; // x2 - x0 <= bound
        
        std::cout << "   Upper bound for x1: " << dbm_raw2bound(x1_bound) 
                  << (dbm_rawIsStrict(x1_bound) ? " (strict)" : " (weak)") << "\n";
        std::cout << "   Upper bound for x2: " << dbm_raw2bound(x2_bound)
                  << (dbm_rawIsStrict(x2_bound) ? " (strict)" : " (weak)") << "\n";
    }
    
    std::cout << "\nTest completed successfully!\n";
    return 0;
}
