#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <chrono>
#include "rtwbs.h"

/**
 * Generates a CSV with structural parameters for each benchmark instance,
 * including the number of zones generated after zone-graph construction.
 *
 * Columns produced:
 *   name                – file stem (e.g. "s_1")
 *   num_automata        – number of template components
 *   total_locations     – sum of locations across all automata
 *   total_edges         – sum of edges (template transitions) across all automata
 *   total_clocks        – sum of clock dimensions (dimension-1 per automaton)
 *   total_zones         – sum of zone-graph states across all automata
 *   total_zone_trans    – sum of zone-graph transitions across all automata
 *   max_constant        – maximum timing constant in the system
 *   zone_build_time_ms  – wall-clock time for zone-graph construction
 */

int main(int argc, char* argv[]) {
    try {
        // ---- paths -------------------------------------------------------
        std::string eval_dir   = "assets/syn_eval";
        std::string output_csv = "results/zone_stats.csv";

        // Simple argument handling
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--dir" && i + 1 < argc)
                eval_dir = argv[++i];
            else if (arg == "--output" && i + 1 < argc)
                output_csv = argv[++i];
        }

        // ---- collect XML files -------------------------------------------
        std::vector<std::filesystem::path> xml_files;
        for (const auto& entry : std::filesystem::directory_iterator(eval_dir)) {
            if (entry.path().extension() == ".xml")
                xml_files.push_back(entry.path());
        }

        if (xml_files.empty()) {
            std::cerr << "No .xml files found in " << eval_dir << std::endl;
            return 1;
        }

        // Sort so the CSV order is deterministic
        std::sort(xml_files.begin(), xml_files.end());

        // ---- ensure output directory exists ------------------------------
        std::filesystem::path out_path(output_csv);
        if (out_path.has_parent_path())
            std::filesystem::create_directories(out_path.parent_path());

        // ---- open CSV ----------------------------------------------------
        std::ofstream csv(output_csv);
        if (!csv.is_open()) {
            std::cerr << "Could not create " << output_csv << std::endl;
            return 1;
        }

        csv << "name,num_automata,total_locations,total_edges,total_clocks,"
               "total_zones,total_zone_trans,max_constant,zone_build_time_ms"
            << std::endl;

        // ---- process each file -------------------------------------------
        for (const auto& file : xml_files) {
            std::string stem = file.stem().string();
            std::cout << "Processing " << file.filename().string() << " ..." << std::flush;

            rtwbs::System sys(file.string());

            // Build zone graphs and measure time
            auto t0 = std::chrono::high_resolution_clock::now();
            sys.construct_all_zone_graphs();
            auto t1 = std::chrono::high_resolution_clock::now();
            double build_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

            // Aggregate structural stats across all automata
            size_t num_automata      = sys.size();
            size_t total_locations   = 0;
            size_t total_edges       = 0;
            size_t total_clocks      = 0;
            size_t total_zones       = 0;
            size_t total_zone_trans  = 0;
            int    max_constant      = 0;

            for (size_t i = 0; i < num_automata; ++i) {
                const auto& a = sys.get_automaton(i);
                total_locations  += a.get_num_locations();
                total_edges      += a.get_num_transitions();
                // dimension = num_clocks + 1 (reference clock)
                total_clocks     += (a.get_dimension() > 0 ? a.get_dimension() - 1 : 0);
                total_zones      += a.get_num_zones();
                total_zone_trans += a.get_num_transition_zg();
                int mc = a.get_max_timing_constant();
                if (mc > max_constant) max_constant = mc;
            }

            csv << stem << ","
                << num_automata << ","
                << total_locations << ","
                << total_edges << ","
                << total_clocks << ","
                << total_zones << ","
                << total_zone_trans << ","
                << max_constant << ","
                << build_ms
                << std::endl;

            std::cout << " done  (zones=" << total_zones
                      << ", zone_trans=" << total_zone_trans
                      << ", " << build_ms << " ms)" << std::endl;
        }

        csv.close();
        std::cout << "\nResults written to " << output_csv << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
