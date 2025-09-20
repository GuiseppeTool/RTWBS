#include "rtwbs/benchmarks/common.h"

namespace rtwbs
{


// Write CSV header
void write_csv_header(std::ofstream& file) {
    file << "system_1,system_2,refined_states,abstract_states,simulation_pairs,check_time_ms,check_time_s,memory_usage_bytes,memory_usage_kb,equivalent" << std::endl;
}

// Append statistics to CSV file
void append_to_csv(std::ofstream& file, 
    const std::string& sys1, 
    const std::string& sys2, 
    const rtwbs::CheckStatistics& stats,
    bool are_equivalent) {
    file << sys1 << "," << sys2 << ","
         << stats.refined_states << ","
         << stats.abstract_states << ","
         << stats.simulation_pairs << ","
         << stats.check_time_ms << ","
         << stats.check_time_ms / 1000 << ","
         << stats.memory_usage_bytes << ","
         << (stats.memory_usage_bytes / 1024.0) << ","
         << (are_equivalent ? ",EQUIVALENT" : ",DIFFERENT")
         << std::endl;
}

void self_equivalence_checks(const std::vector<std::string>& filenames,
                            const char* benchmark_folder ,
                            const char* results_folder ,
                            const char* benchmark_prefix, 
                            size_t num_workers)
{
            // Generate timestamp-based filename
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << results_folder<< benchmark_prefix<< std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S") << ".csv";
        std::string csv_filename = ss.str();

        if(!std::filesystem::is_directory(results_folder)) {
            std::filesystem::create_directories(results_folder);
        }

        // Create CSV file and write header
        std::ofstream csv_file(csv_filename);
        if (!csv_file.is_open()) {
            throw std::runtime_error("Error: Could not create CSV file " + csv_filename);
        }
        


        rtwbs::CheckStatistics::write_csv_header(csv_file);
        
        
        rtwbs::CheckStatistics total_stats{0, 0, 0, 0.0, 0};
        for (const auto& filename : filenames) {
            
            std::cout << "Processing " << benchmark_folder << filename << std::endl;
            rtwbs::System s(benchmark_folder + filename);
            s.construct_all_zone_graphs();
            std::cout << "Running self-equivalence check..." << std::endl;
            // Fresh checker per file to avoid cross-file contamination
            rtwbs::RTWBSChecker checker;
            bool equivalent = checker.check_rtwbs_equivalence(s, s, num_workers);
            std::cout << "Self-equivalence result: " << (equivalent ? "EQUIVALENT" : "DIFFERENT") << std::endl;
            if (!equivalent) {
                
                throw std::runtime_error("Error: System " + filename + " is not self-equivalent under RTWBS!");
            }
            
            
            // Get statistics for this run and write to CSV
            auto current_stats = checker.get_last_check_statistics();
            current_stats.append_to_csv(csv_file, filename);
            
            total_stats += current_stats;
        }

        // Write total stats to CSV
        total_stats.append_to_csv(csv_file, "TOTAL");
        csv_file.close();

        std::cout << "--------------------TOTAL STATS-------------------" << std::endl; 
        //print total stats
        total_stats.print();
        std::cout << "========================================" << std::endl;
        std::cout << "Results saved to: " << csv_filename << std::endl;
}



void comparison_checks(const std::vector<std::string>& filenames,
    const char* benchmark_folder,
    const char* results_folder ,
    const char* benchmark_prefix, size_t n_workers )
{
            // Generate timestamp-based filename
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << results_folder<< benchmark_prefix<< std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S") << ".csv";
        std::string csv_filename = ss.str();
         if(!std::filesystem::is_directory(results_folder)) {
            std::filesystem::create_directories(results_folder);
        }

        // Create CSV file and write header
        std::ofstream csv_file(csv_filename);
        if (!csv_file.is_open()) {
            throw std::runtime_error("Error: Could not create CSV file " + csv_filename);
        }
        
        write_csv_header(csv_file);
        
        rtwbs::RTWBSChecker checker;
        rtwbs::CheckStatistics total_stats{0, 0, 0, 0.0, 0};

        //create a map that fiven the file name gives me the system
        std::unordered_map<std::string, std::shared_ptr<rtwbs::System>> systems_map;
        for (const auto& filename : filenames) {
            std::cout << "Loading system from " << benchmark_folder << filename << std::endl;
            auto system_ptr = std::make_shared<rtwbs::System>(benchmark_folder + filename);
            systems_map[filename] = system_ptr;  // This copies the shared_ptr, not the System
        }
        
        for (size_t i = 0; i < filenames.size(); ++i) {
            const auto& filename_1 = filenames[i];
            std::cout << "Processing " << benchmark_folder << filename_1 << std::endl;

            for (size_t j = i+1; j < filenames.size(); ++j) {  // Start from i+1
                const auto& filename_2 = filenames[j];
                std::cout << "Comparing it to " << benchmark_folder << filename_2 << std::endl;
                
                std::cout << "Running equivalence check..." << std::endl;
                bool equivalent = checker.check_rtwbs_equivalence(*systems_map[filename_1], *systems_map[filename_2], n_workers);
                std::cout << "Equivalence result: " << (equivalent ? "EQUIVALENT" : "DIFFERENT") << std::endl;
                checker.print_statistics();
                
                // Get statistics for this run and write to CSV
                auto current_stats = checker.get_last_check_statistics();
                append_to_csv(csv_file, filename_1, filename_2, current_stats, equivalent);
                
                total_stats += current_stats;
            }
        }

        // Write total stats to CSV
        append_to_csv(csv_file, "TOTAL", "TOTAL", total_stats, false);
        csv_file.close();

        std::cout << "--------------------TOTAL STATS-------------------" << std::endl; 
        //print total stats
        total_stats.print();
        std::cout << "========================================" << std::endl;
        std::cout << "Results saved to: " << csv_filename << std::endl;
}

void parse_arguments(int argc, char* argv[], std::string& results_folder, int& n_workers) {
    results_folder = "results/";
    n_workers = 0; // Default to 0 (auto-detect)
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--folder" && i + 1 < argc) {
            results_folder = argv[++i] ;
        } else if (arg == "--n-workers" && i + 1 < argc)    {
            n_workers = std::stoi(argv[++i]);
        }
    }
    if (n_workers < 0) n_workers = 0; // 0 means auto-detect
    if (n_workers > std::thread::hardware_concurrency()) n_workers = std::thread::hardware_concurrency();
    if (results_folder.back() != '/') results_folder += '/';
    if (results_folder.empty()) results_folder = "results/";
    if (n_workers >0 && results_folder=="results/") results_folder = "results_"+std::to_string(n_workers)+"/";
}

    // Implementation of benchmark functions
} // namespace rtwbs
