

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
            "ASTRail/FMICS2019/model.xml",
            "ASTRail/FMICS2019/model4_1_22.xml",
            //"ASTRail/ISOLA2018/MovBlock.xml",
            //"ASTRail/STTT2021/model - demonic completion.xml",
            //"ASTRail/STTT2021/model - scenario acceleration.xml",
            //"ASTRail/STTT2021/model - scenario braking.xml",
            //"ASTRail/STTT2021/model - scenario crash.xml",
            //"ASTRail/STTT2021/model - scenario location not fresh.xml",
            //"ASTRail/STTT2021/model - scenario slower leading train.xml",
            //"ASTRail/STTT2021/model.xml"
        };
        rtwbs::self_equivalence_checks(filenames,"assets/", results_folder.c_str(),"benchmark_results_", parallel_mode, n_workers, -1, algo);
        rtwbs::comparison_checks(filenames,"assets/",results_folder.c_str(),"comparison_results_", parallel_mode, n_workers, algo);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}