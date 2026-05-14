/**
 * @file run_semantic_alignment.cpp
 * @brief CLI entry point for the SemAlign semantic alignment checker.
 *
 * Usage:
 *   run_semantic_alignment --pt <pt.xml> --dt <dt.xml>
 *                          [--ontology <ont.ont|ont.json>]
 *                          [--pt-interp <pt.interp>] [--dt-interp <dt.interp>]
 *                          [--output <results.csv>] [--syntactic] [--verbose]
 *
 * Options:
 *   --pt <file>          UPPAAL XML for the Physical Twin (or refined model).
 *   --dt <file>          UPPAAL XML for the Digital Twin (or abstract model).
 *   --ontology <file>    Ontology file: .ont (text format) or .json (bundled).
 *   --pt-interp <file>   PT interpretation file (.interp); required with .ont.
 *   --dt-interp <file>   DT interpretation file (.interp); required with .ont.
 *   --output <file>      Append one CSV row to this file.
 *   --syntactic          Run standard RTWBS (no SMT); baseline for RQ2.
 *   --verbose            Print zone-graph and checker statistics.
 */

#include <iostream>
#include <string>
#include <filesystem>

#include "rtwbs/timedautomaton.h"
#include "rtwbs/core.h"
#include "rtwbs/ontology_parser.h"
#include "rtwbs/domain_parser.h"
#include "rtwbs/semantic_checker.h"

static void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog
              << " --pt <pt.xml> --dt <dt.xml>"
                 " [--ontology <file.ont|file.json>]"
                 " [--pt-interp <pt.interp>] [--dt-interp <dt.interp>]"
                 " [--output <results.csv>] [--syntactic] [--verbose]\n";
}

int main(int argc, char* argv[]) {
    std::string pt_file, dt_file, ont_file, pt_interp_file, dt_interp_file, out_file;
    bool syntactic = false;
    bool verbose   = false;

    // ---- Argument parsing ----
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto require_next = [&](const char* name) -> std::string {
            if (i + 1 >= argc) {
                std::cerr << "Error: " << name << " requires an argument.\n";
                std::exit(1);
            }
            return argv[++i];
        };

        if      (arg == "--pt")         pt_file         = require_next("--pt");
        else if (arg == "--dt")         dt_file         = require_next("--dt");
        else if (arg == "--ontology")   ont_file        = require_next("--ontology");
        else if (arg == "--pt-interp")  pt_interp_file  = require_next("--pt-interp");
        else if (arg == "--dt-interp")  dt_interp_file  = require_next("--dt-interp");
        else if (arg == "--output")     out_file        = require_next("--output");
        else if (arg == "--syntactic")  syntactic = true;
        else if (arg == "--verbose")    verbose   = true;
        else {
            std::cerr << "Unknown argument: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    if (pt_file.empty() || dt_file.empty()) {
        std::cerr << "Error: --pt and --dt are required.\n";
        print_usage(argv[0]);
        return 1;
    }

    // ---- Validate files ----
    for (const auto& [label, path] : std::vector<std::pair<std::string,std::string>>{
                                        {"PT", pt_file}, {"DT", dt_file}}) {
        if (!std::filesystem::exists(path)) {
            std::cerr << "Error: " << label << " file not found: " << path << "\n";
            return 1;
        }
    }

    try {
        // ---- Load automata ----
        std::cout << "=== SemAlign: Semantic Alignment Checker ===" << std::endl;
        std::cout << "  PT model:  " << pt_file << std::endl;
        std::cout << "  DT model:  " << dt_file << std::endl;

        rtwbs::TimedAutomaton ta_pt(pt_file);
        rtwbs::TimedAutomaton ta_dt(dt_file);

        std::cout << "Constructing zone graphs..." << std::endl;
        ta_pt.construct_zone_graph();
        ta_dt.construct_zone_graph();

        if (verbose) {
            std::cout << "  PT: " << ta_pt.get_num_states() << " zone states, "
                      << ta_pt.get_num_locations() << " locations, "
                      << ta_pt.get_num_transitions() << " transitions" << std::endl;
            std::cout << "  DT: " << ta_dt.get_num_states() << " zone states, "
                      << ta_dt.get_num_locations() << " locations, "
                      << ta_dt.get_num_transitions() << " transitions" << std::endl;
        }
        std::cout << std::endl;

        // ---- Syntactic baseline (no SMT) ----
        if (syntactic) {
            std::cout << "--- Syntactic RTWBS Baseline (no SMT) ---" << std::endl;
            rtwbs::SemanticAlignmentChecker base_checker;
            rtwbs::SemanticAlignmentResult base_sem_result;
            bool base_result = base_checker.check_weak_timed_bisimulation(ta_pt, ta_dt, base_sem_result);
            std::cout << "Verdict: "
                      << (base_result ? "TRUE (syntactically bisimilar)" : "FALSE")
                      << std::endl;
            if (verbose) base_sem_result.print();

            if (!out_file.empty()) {
                bool write_header = !std::filesystem::exists(out_file);
                std::ofstream ofs(out_file, std::ios::app);
                if (!ofs) {
                    std::cerr << "Warning: cannot open output file " << out_file << "\n";
                } else {
                    if (write_header) {
                        ofs << "model_name,mode,aligned,time_ms\n";
                    }
                    std::string name = std::filesystem::path(pt_file).stem().string()
                                     + "_vs_"
                                     + std::filesystem::path(dt_file).stem().string();
                    ofs << name << ",syntactic,"
                        << (base_result ? "true" : "false") << ","
                        << std::fixed << std::setprecision(3) << base_sem_result.time_ms
                        << "\n";
                }
            }
            return base_result ? 0 : 2;
        }

        // ---- Semantic alignment ----
        std::string ont_path = ont_file.empty() ? "" : ont_file;
        std::cout << "  Ontology:  "
                  << (ont_path.empty() ? "(none — trivial)" : ont_path)
                  << std::endl;
        if (!pt_interp_file.empty())
            std::cout << "  PT interp: " << pt_interp_file << std::endl;
        if (!dt_interp_file.empty())
            std::cout << "  DT interp: " << dt_interp_file << std::endl;
        std::cout << std::endl;

        rtwbs::DomainKnowledge dk = [&]() -> rtwbs::DomainKnowledge {
            if (ont_path.empty())
                return rtwbs::OntologyParser::make_trivial();
            // Detect .ont text format by extension
            bool is_ont_format = (ont_path.size() >= 4 &&
                                  ont_path.substr(ont_path.size() - 4) == ".ont");
            if (is_ont_format) {
                rtwbs::OntFileParser    ont_parser;
                rtwbs::InterpFileParser interp_parser;
                auto ontology = ont_parser.parse(ont_path);
                rtwbs::DomainKnowledge result(ontology);
                if (!pt_interp_file.empty())
                    result.pt_interp = interp_parser.parse(pt_interp_file, ontology);
                if (!dt_interp_file.empty())
                    result.dt_interp = interp_parser.parse(dt_interp_file, ontology);
                return result;
            } else {
                rtwbs::OntologyParser parser;
                return parser.parse(ont_path);
            }
        }();

        std::cout << "--- Semantic Alignment Check ---" << std::endl;

        rtwbs::SemanticAlignmentChecker checker;
        rtwbs::SemanticAlignmentResult  result;
        bool verdict = checker.check_semantic_alignment(ta_pt, ta_dt, dk, result);

        std::cout << "Verdict: "
                  << (verdict ? "TRUE (semantically aligned)" : "FALSE (not aligned)")
                  << std::endl << std::endl;
        result.print();

        // ---- CSV output ----
        if (!out_file.empty()) {
            bool write_header = !std::filesystem::exists(out_file);
            std::ofstream ofs(out_file, std::ios::app);
            if (!ofs) {
                std::cerr << "Warning: cannot open output file " << out_file << "\n";
            } else {
                if (write_header) {
                    rtwbs::SemanticAlignmentResult::write_csv_header(ofs);
                }
                std::string name = std::filesystem::path(pt_file).stem().string()
                                 + "_vs_"
                                 + std::filesystem::path(dt_file).stem().string();
                result.append_to_csv(ofs, name);
                std::cout << "Results appended to: " << out_file << std::endl;
            }
        }

        return verdict ? 0 : 2;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
