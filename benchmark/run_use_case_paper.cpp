#include <iostream>
#include <filesystem>
#include "rtwbs/core.h"
#include "rtwbs/timedautomaton.h"

/**
 * run_use_case_paper.cpp
 *
 * Loads DA_abstract.xml and DA_refined.xml from the use_case_paper/
 * directory and runs the RTWBS equivalence checker (Algorithm 1).
 *
 * Usage:
 *   ./run_use_case_paper [path/to/use_case_paper]
 *
 * Output: verdict, statistics, and key state-pair information.
 */

int main(int argc, char* argv[]) {
    try {
        std::string base_dir = "use_case_paper";
        if (argc > 1) base_dir = argv[1];

        std::string abs_file = base_dir + "/DA_abstract.xml";
        std::string ref_file = base_dir + "/DA_refined.xml";

        if (!std::filesystem::exists(abs_file)) {
            std::cerr << "File not found: " << abs_file << std::endl;
            return 1;
        }
        if (!std::filesystem::exists(ref_file)) {
            std::cerr << "File not found: " << ref_file << std::endl;
            return 1;
        }

        std::cout << "=== Sugar Beet Monitoring — RTWBS Checker ===" << std::endl;
        std::cout << "  Abstract model: " << abs_file << std::endl;
        std::cout << "  Refined  model: " << ref_file << std::endl;
        std::cout << std::endl;

        // Load automata from UPPAAL XML
        rtwbs::TimedAutomaton ta_abstract(abs_file);
        rtwbs::TimedAutomaton ta_refined(ref_file);

        // Construct zone graphs
        std::cout << "Constructing zone graphs..." << std::endl;
        ta_abstract.construct_zone_graph();
        ta_refined.construct_zone_graph();

        std::cout << "  DA_abstract: " << ta_abstract.get_num_states()
                  << " zone states, " << ta_abstract.get_num_locations()
                  << " locations, " << ta_abstract.get_num_transitions()
                  << " transitions" << std::endl;
        std::cout << "  DA_refined:  " << ta_refined.get_num_states()
                  << " zone states, " << ta_refined.get_num_locations()
                  << " locations, " << ta_refined.get_num_transitions()
                  << " transitions" << std::endl;
        std::cout << std::endl;

        // Run RTWBS equivalence check
        rtwbs::RTWBSChecker checker;

        std::cout << "--- RTWBS Equivalence Check: DA_refined ≤ DA_abstract ---"
                  << std::endl;
        bool verdict = checker.check_rtwbs_equivalence(ta_refined, ta_abstract);
        std::cout << "Verdict: " << (verdict ? "TRUE (refinement holds)" : "FALSE (refinement fails)") << std::endl;
        std::cout << std::endl;
        checker.print_statistics();

        std::cout << std::endl;
        std::cout << "--- Self-Equivalence: DA_abstract ≡ DA_abstract ---" << std::endl;
        checker.reset();
        bool self_eq = checker.check_rtwbs_equivalence(ta_abstract, ta_abstract);
        std::cout << "Verdict: " << (self_eq ? "TRUE" : "FALSE") << std::endl;
        checker.print_statistics();

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
