#ifndef RTWBS_BENCHMARK_COMMON_H
#define RTWBS_BENCHMARK_COMMON_H

#include <iostream>
#include <exception>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>

#include "rtwbs.h"

namespace rtwbs {

void write_csv_header(std::ofstream& file);
void append_to_csv(std::ofstream& file, 
    const std::string& sys1, 
    const std::string& sys2, 
    const rtwbs::CheckStatistics& stats,
    bool are_equivalent);


void self_equivalence_checks(const std::vector<std::string>& filenames,
                            const char* benchmark_folder = "assets/uppaal_benchmarks/",
                            const char* results_folder = "results/",
                            const char* benchmark_prefix = "benchmark_results_",
                            size_t num_workers = 0 );


void comparison_checks(const std::vector<std::string>& filenames,
    const char* benchmark_folder = "assets/uppaal_benchmarks/",
    const char* results_folder = "results/",
    const char* benchmark_prefix = "comparison_results",
size_t num_workers = 0  );


void parse_arguments(int argc, char* argv[], std::string& results_folder, int& n_workers);

} // namespace rtwbs
#endif // RTWBS_BENCHMARK_COMMON_H