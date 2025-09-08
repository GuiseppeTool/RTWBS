#include <iostream>
#include "rtwbs/benchmarks/common.h"

int main(int argc, char* argv[]) {
    try {
        std::vector<std::string> filenames = {
            "modelMitigation.xml", "modelLowerMaxLostMsgFastVerification.xml", "modelLowerMaxLostMsg.xml", "modelConfStandard.xml", "modelNoTransmissionDelayThreat.xml", "modelFastVerification.xml", "modelLowerSNMax.xml",


        };
        rtwbs::self_equivalence_checks(filenames,"assets/FMICS2021/", "results/","benchmark_results_");
        rtwbs::comparison_checks(filenames,"assets/FMICS2021/","results/","comparison_results_");
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
