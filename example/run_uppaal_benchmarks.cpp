#include <iostream>
#include "rtwbs/benchmarks/common.h"

int main(int argc, char* argv[]) {
    try {
        std::vector<std::string> filenames = {
            "modelMitigation.xml", "modelLowerMaxLostMsgFastVerification.xml", "modelLowerMaxLostMsg.xml", "modelConfStandard.xml", "modelNoTransmissionDelayThreat.xml", "modelFastVerification.xml", "modelLowerSNMax.xml",
            "ASTRail/FMICS2029/model.xml","ASTRail/FMICS2029/model4_1_22.xml",
            "ASTRail/ISOLA2018/MovBlock.xml",
            "ASTRail/STTT2021/model - demonic completion.xml",
            "ASTRail/STTT2021/model - scenario acceleration.xml",
            "ASTRail/STTT2021/model - scenario braking.xml",
            "ASTRail/STTT2021/model - scenario crash.xml",
            "ASTRail/STTT2021/model - scenario location not fresh.xml",
            "ASTRail/STTT2021/model - scenario slower leading train.xml",
            "ASTRail/STTT2021/model.xml"

        };
        rtwbs::self_equivalence_checks(filenames,"assets/uppaal_benchmarks/", "results/","benchmark_results_");
        rtwbs::comparison_checks(filenames,"assets/uppaal_benchmarks/","results/","comparison_results_");
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
