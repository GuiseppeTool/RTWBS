#include <iostream>
#include "rtwbs/benchmarks/common.h"

int main(int argc, char* argv[]) {
    try {

        std::string results_folder = rtwbs::RESULTS_FOLDER;
        int n_workers = 0;
        rtwbs::RunningMode parallel_mode = rtwbs::RunningMode::SERIAL;

        rtwbs::parse_arguments(argc, argv, results_folder, n_workers, parallel_mode);

        std::vector<std::string> FMICS_filenames = {
            "FMICS2021/modelMitigation.xml", 
            "FMICS2021/modelLowerMaxLostMsgFastVerification.xml",
             "FMICS2021/modelLowerMaxLostMsg.xml", 
             "FMICS2021/modelConfStandard.xml",
              "FMICS2021/modelNoTransmissionDelayThreat.xml",
               "FMICS2021/modelFastVerification.xml", 
        };


        std::vector<std::string> ASTRail_filenames = {
            "ASTRail/FMICS2019/model.xml",
            "ASTRail/FMICS2019/model4_1_22.xml",
            "ASTRail/ISOLA2018/MovBlock.xml",
            "ASTRail/STTT2021/model - demonic completion.xml",
            "ASTRail/STTT2021/model - scenario acceleration.xml",
            "ASTRail/STTT2021/model - scenario braking.xml",
            "ASTRail/STTT2021/model - scenario crash.xml",
            "ASTRail/STTT2021/model - scenario location not fresh.xml",
            "ASTRail/STTT2021/model - scenario slower leading train.xml",
            "ASTRail/STTT2021/model.xml"
        };
        rtwbs::self_equivalence_checks(FMICS_filenames,"assets/", results_folder.c_str(),"FMICS_benchmark_results_", parallel_mode, n_workers);
        rtwbs::comparison_checks(FMICS_filenames,"assets/",results_folder.c_str(),"FMICS_comparison_results_", parallel_mode, n_workers);
        rtwbs::self_equivalence_checks(ASTRail_filenames,"assets/", results_folder.c_str(),"ASTRail_benchmark_results_", parallel_mode, n_workers);
        rtwbs::comparison_checks(ASTRail_filenames,"assets/",results_folder.c_str(),"ASTRail_comparison_results_", parallel_mode, n_workers);

         std::cout << "Results folder: " << results_folder << std::endl;
        std::cout << "Number of workers: " << n_workers << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
