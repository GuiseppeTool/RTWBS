#include <iostream>
#include "rtwbs/benchmarks/common.h"

int main(int argc, char* argv[]) {
    try {

        std::string results_folder = rtwbs::RESULTS_FOLDER;
        int n_workers = 0;
        rtwbs::RunningMode parallel_mode = rtwbs::RunningMode::SERIAL;
        rtwbs::AlgorithmMode algo = rtwbs::AlgorithmMode::GFP;

        std::string input_folder = "";

        rtwbs::parse_arguments(argc, argv, &results_folder, &n_workers, &parallel_mode, &input_folder, false, &algo);

        //std::vector<std::string> use_case_filenames = {
        //    "use_case/V1.xml",
        //    "use_case/V2_nob.xml",
        //};
        // load the use_case_filenames from a folder 
        std::vector<std::string> use_case_filenames;
        std::string reference_file;
        for (const auto& entry : std::filesystem::directory_iterator(input_folder)) {
       
            if (entry.path().extension() == ".xml") {
                if (entry.path().filename() == "V1.xml") {
                    reference_file = entry.path().string();
                } else {
                    use_case_filenames.push_back(entry.path().string());
                }
            }
        }



        std::sort(use_case_filenames.begin(), use_case_filenames.end(),
            [](const std::string& a, const std::string& b) {

                auto get_number = [](const std::string& s) {
                    auto start = s.find("V2_nob_") + 7;
                    auto end = s.find(".xml", start);
                    return std::stoi(s.substr(start, end - start));
                };

                return get_number(a) < get_number(b);
        });



        rtwbs::self_equivalence_checks(use_case_filenames,"", results_folder.c_str(),"use_case_benchmark_results_", parallel_mode, n_workers, -1, algo);
        //rtwbs::comparison_checks(use_case_filenames,"",results_folder.c_str(),"use_case_comparison_results_", parallel_mode, n_workers, algo);
        rtwbs::comparison_checks_one_vs_many(reference_file, std::vector<std::string>(use_case_filenames.begin(), use_case_filenames.end()), "", results_folder.c_str(), "use_case_comparison_results_", parallel_mode, n_workers, algo);

         std::cout << "Results folder: " << results_folder << std::endl;
        std::cout << "Number of workers: " << n_workers << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
