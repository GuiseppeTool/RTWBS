#include <iostream>
#include "rtwbs/benchmarks/common.h"

int main(int argc, char* argv[]) {
    try {
        std::string results_folder = rtwbs::RESULTS_FOLDER;
        int n_workers = 0;
        rtwbs::RunningMode parallel_mode = rtwbs::RunningMode::SERIAL;
        rtwbs::AlgorithmMode algo = rtwbs::AlgorithmMode::GFP;
        rtwbs::parse_arguments(argc, argv, &results_folder, &n_workers, &parallel_mode, nullptr, true, &algo);

        std::vector<std::string> filenames = {
            "sugarBeetFieldAbstract.xml", "sugarBeetFieldRefined.xml"
        };
        rtwbs::self_equivalence_checks(filenames,"assets/SugarBeetField/", results_folder.c_str(),"SugarBeet_benchmark_results_", parallel_mode, n_workers, -1, algo);
        rtwbs::comparison_checks(filenames,"assets/SugarBeetField/",results_folder.c_str(),"SugarBeet_comparison_results_", parallel_mode, n_workers, algo);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
