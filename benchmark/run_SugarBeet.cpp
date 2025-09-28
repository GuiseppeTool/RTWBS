#include <iostream>
#include "rtwbs/benchmarks/common.h"

int main(int argc, char* argv[]) {
    try {
        std::vector<std::string> filenames = {
            "sugarBeetFieldAbstract.xml", "sugarBeetFieldRefined.xml"
        };
        rtwbs::self_equivalence_checks(filenames,"assets/SugarBeetField/", rtwbs::RESULTS_FOLDER,"SugarBeet_benchmark_results_");
        rtwbs::comparison_checks(filenames,"assets/SugarBeetField/",rtwbs::RESULTS_FOLDER,"SugarBeet_comparison_results_");
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
