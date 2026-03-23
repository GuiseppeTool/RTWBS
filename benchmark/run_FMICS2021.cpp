#include <iostream>
#include "rtwbs/benchmarks/common.h"

int main(int argc, char* argv[]) {
    try {
        std::string results_folder = rtwbs::RESULTS_FOLDER;
        int n_workers = 0;
        rtwbs::RunningMode parallel_mode = rtwbs::RunningMode::SERIAL;
        rtwbs::AlgorithmMode algo = rtwbs::AlgorithmMode::GFP;
        rtwbs::parse_arguments(argc, argv, &results_folder, &n_workers, &parallel_mode, nullptr, true, &algo);

        std::cout << "\n=== Testing RTWBS with different file types ===" << std::endl;
        
        // Test 1: Truly identical files (should be EQUIVALENT)
        std::cout << "\n--- Test 1: Truly identical files ---" << std::endl;
        std::vector<std::string> identical_files = {
            "modelConfStandard.xml", 
            "modelConfStandard_identical.xml"
        };
        rtwbs::comparison_checks(identical_files,"assets/FMICS2021/",results_folder.c_str(),"identical_test_", parallel_mode, n_workers, algo);
        
        // Test 2: Files with formatting differences (currently DIFFERENT)
        std::cout << "\n--- Test 2: Files with formatting differences ---" << std::endl;
        std::vector<std::string> formatted_files = {
            "modelConfStandard.xml", 
            "modelConfStandard copy.xml"
        };
        rtwbs::comparison_checks(formatted_files,"assets/FMICS2021/",results_folder.c_str(),"formatted_test_", parallel_mode, n_workers, algo);
        
      
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
