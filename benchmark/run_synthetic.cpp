#include <iostream>
#include <string>
#include "rtwbs.h"
#include "rtwbs/benchmarks/common.h"
int main(int argc, char* argv[]) {
    try {
        std::string results_folder = "results/";
        int n_workers = 0;

        rtwbs::parse_arguments(argc, argv, results_folder, n_workers);
        

        std::string path = "assets/eval";
        
            std::vector<std::string> files;
            for (const auto& entry : std::filesystem::directory_iterator(path)) {
                if (entry.path().extension() == ".xml") {
                    files.push_back(entry.path().string());
                }
            }
            if (files.empty()) {
                std::cerr << "Error: No .xml files found in directory: " << path << std::endl;
                return 1;
            }
            try {
                //rtwbs::System sys(file);
                //sys.print_system_overview();
                //sys.construct_all_zone_graphs();
                //sys.print_all_statistics();
                rtwbs::self_equivalence_checks(files,"./", results_folder.c_str(),"syn_benchmark_results_", n_workers);
            } catch (const std::exception& e) {
                std::cerr << "Error processing files: " << e.what() << std::endl;
            }
        
            return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }  
}
