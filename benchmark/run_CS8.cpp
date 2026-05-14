/**
 * @file run_CS8.cpp
 * @brief CS8: Lift Plus Cruise eVTOL (NASA FRET + LMCPS / ASTM F3269-21 / SAE AS6968)
 *
 * Tests semantic alignment between the Physical Twin (LiftCruisePT) and the
 * Digital Twin (FlightEnvelopeDT) under the eVTOL operations domain ontology.
 *
 * The Lift Plus Cruise eVTOL is the canonical NASA/FAA benchmark for advanced air
 * mobility certification (NASA FRET verification, Lockheed Martin CPS Challenge).
 * The PT models onboard avionics / propulsion control (IEC 61508-3:2010 SIL-3 firmware).
 * The DT models a ground-based flight envelope and airspace integration monitor
 * using ASTM F3269-21 / RTCA DO-365 well-clear vocabulary.
 *
 * Standards addressed:
 *   ASTM F3269-21  (eVTOL well-clear and separation assurance)
 *   FAA AC 21-7A   (eVTOL certification basis)
 *   RTCA DO-365:2020 (ACAS sXu)
 *   SAE AS6968:2021 (eVTOL airworthiness standards)
 *   EUROCAE ED-269:2020 (minimum operational performance standards)
 *   IEC 61508-3:2010 (functional safety software)
 *
 * DT timing offset: +3 units (DO-365 telemetry latency budget).
 *
 * Research questions addressed:
 *  RQ1 - Ontology evolution (Theorem 1):
 *         Condition I:  Add NOx sort and nox_emission_rate function (SAE ARP5765A).
 *         Condition II: Tighten min_soc_operation from 20% to 22% (SAE AS6968 revised).
 *         Both interps reference min_soc_operation symbolically -- alignment preserved.
 *  RQ2 - Misalignment coverage:
 *         Variant A (well-clear gap): DT propulsion_mode_change! adds tau_mod >=
 *           tau_mod_threshold precondition absent from PT transition_cruise_start!.
 *           ASTM F3269-21 Sec.6.3: DT enforces airspace separation before mode change;
 *           PT command logic is traffic-agnostic. Asymmetric contract.
 *           Expected: NOT ALIGNED (Condition III -- DT-to-PT direction).
 *         Variant B (SOC threshold drift): low_energy_alert! fires at SOC < 25%
 *           instead of < min_soc_operation (= 20%). SAE AS6968 5% safety margin drift.
 *           Z3 counterexample: battery_soc = 22 satisfies DT formula but not PT formula.
 *           Expected: NOT ALIGNED (Condition I -- state interpretation inconsistency).
 *  RQ3 - Zone-graph sizes and check times.
 *  RQ4 - Engineering cost: syntactic bisimulation vs semantic alignment.
 *         eVTOL avionics vocabulary vs airspace integration vocabulary gap means
 *         zero shared label strings. Syntactic baseline fails; semantic alignment
 *         succeeds via ASTM F3269-21 / SAE AS6968 domain ontology.
 *
 * Usage:
 *   ./run_CS8 [--folder <path>]
 */

#include <iostream>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <string>
#include <stdexcept>

#include "rtwbs/timedautomaton.h"
#include "rtwbs/core.h"
#include "rtwbs/ontology.h"
#include "rtwbs/interpretation.h"
#include "rtwbs/semantic_checker.h"
#include "rtwbs/domain_parser.h"
#include "rtwbs/benchmarks/common.h"

static const std::string ASSET_DIR = "assets/CS8_eVTOL/";

// ============================================================================
// Helpers
// ============================================================================

static rtwbs::DomainKnowledge load_domain(const std::string& ont_path,
                                           const std::string& pt_interp_path,
                                           const std::string& dt_interp_path)
{
    rtwbs::OntFileParser    ont_parser;
    rtwbs::InterpFileParser interp_parser;

    auto ontology = ont_parser.parse(ont_path);
    auto pt_imap  = interp_parser.parse(pt_interp_path, ontology);
    auto dt_imap  = interp_parser.parse(dt_interp_path, ontology);

    rtwbs::DomainKnowledge dk(ontology);
    dk.pt_interp = std::move(pt_imap);
    dk.dt_interp = std::move(dt_imap);
    return dk;
}

// ============================================================================
// main
// ============================================================================

int main(int argc, char* argv[])
{
    try {
        std::string results_folder = "results/";
        int n_workers = 0;
        rtwbs::RunningMode parallel_mode = rtwbs::RunningMode::SERIAL;
        rtwbs::AlgorithmMode algo = rtwbs::AlgorithmMode::GFP;
        rtwbs::parse_arguments(argc, argv, &results_folder,
                               &n_workers, &parallel_mode, nullptr, true, &algo);

        if (!std::filesystem::is_directory(results_folder))
            std::filesystem::create_directories(results_folder);

        std::string csv_path = results_folder + "CS8_results.csv";
        std::ofstream csv(csv_path);
        if (!csv.is_open())
            throw std::runtime_error("Cannot open result file: " + csv_path);

        rtwbs::SemanticAlignmentResult::write_csv_header(csv);

        // ────────────────────────────────────────────────────────────────────
        // Load PT and DT zone graphs (shared across all runs)
        // ────────────────────────────────────────────────────────────────────
        std::cout << "=== CS8: Lift Plus Cruise eVTOL ===" << std::endl;
        std::cout << "    (ASTM F3269-21 / FAA AC 21-7A / RTCA DO-365:2020 / SAE AS6968:2021)" << std::endl;
        std::cout << "    PT: LiftCruisePT  (avionics / propulsion control, IEC 61508-3 SIL-3)" << std::endl;
        std::cout << "    DT: FlightEnvelopeDT (ground flight envelope + airspace monitor)" << std::endl;
        std::cout << "Loading PT and DT models..." << std::endl;

        rtwbs::TimedAutomaton pt(ASSET_DIR + "V1_PT.xml");
        rtwbs::TimedAutomaton dt(ASSET_DIR + "V2_DT.xml");
        pt.construct_zone_graph();
        dt.construct_zone_graph();

        std::cout << "  PT zones: " << pt.get_num_states()
                  << "  |  DT zones: " << dt.get_num_states() << "\n" << std::endl;

        // ────────────────────────────────────────────────────────────────────
        // RUN 1: Semantic alignment -- correctly aligned pair
        //
        // PT labels (avionics vocabulary):
        //   takeoff_initiated!, transition_cruise_start!, cruise_established!,
        //   transition_hover_start!, hover_stable!, descent_initiated!,
        //   landing_initiated!, touchdown!, battery_critical!, emergency_descent!,
        //   rotor_fault!, emergency_landing!
        //
        // DT labels (airspace integration / DO-365 vocabulary):
        //   vehicle_departure_event!, propulsion_mode_change!, cruise_mode_confirmed!,
        //   hover_mode_initiation!, hover_confirmed_telemetry!, descent_sequence_start!,
        //   final_approach_event!, ground_contact_event!, low_energy_alert!,
        //   forced_descent_command!, propulsion_fault_event!, emergency_landing_command!
        //
        // Zero shared label strings. Semantic alignment bridges the vocabulary gap via
        // the ASTM F3269-21 / SAE AS6968 domain ontology.
        //
        // Key label equivalences established via ontology (12 label-equiv pairs):
        //   takeoff_initiated!        ~ vehicle_departure_event!
        //     both: (and (= altitude 0) (>= battery_soc min_soc_operation))
        //   transition_cruise_start!  ~ propulsion_mode_change!
        //     both: (and (>= altitude 500) (>= battery_soc min_soc_operation))
        //   cruise_established!       ~ cruise_mode_confirmed!
        //     both: (and (>= airspeed 50) (<= transition_time max_transition_time) ...)
        //   transition_hover_start!   ~ hover_mode_initiation!
        //     both: (and (>= battery_soc min_soc_operation) (>= altitude 500))
        //   hover_stable!             ~ hover_confirmed_telemetry!
        //     both: (and (<= airspeed 5) (<= transition_time max_transition_time) ...)
        //   descent_initiated!        ~ descent_sequence_start!
        //     both: (>= battery_soc min_soc_landing)
        //   landing_initiated!        ~ final_approach_event!
        //     both: (and (<= altitude 500) (>= battery_soc min_soc_landing))
        //   touchdown!                ~ ground_contact_event!
        //     both: (= altitude 0)
        //   battery_critical!         ~ low_energy_alert!
        //     both: (< battery_soc min_soc_operation)
        //   emergency_descent!        ~ forced_descent_command!
        //     both: (< battery_soc min_soc_operation)
        //   rotor_fault!              ~ propulsion_fault_event!
        //     both: (> lift_motor_current max_motor_current)
        //   emergency_landing!        ~ emergency_landing_command!
        //     both: (> lift_motor_current max_motor_current)
        //
        // Expected: ALIGNED = true
        // ────────────────────────────────────────────────────────────────────
        std::cout << "[RUN 1] Semantic alignment (aligned pair)..." << std::endl;

        auto dk_base = load_domain(ASSET_DIR + "domain.ont",
                                   ASSET_DIR + "pt.interp",
                                   ASSET_DIR + "dt.interp");

        rtwbs::SemanticAlignmentChecker checker_r1;
        rtwbs::SemanticAlignmentResult  r1;
        bool ok_r1 = checker_r1.check_semantic_alignment(pt, dt, dk_base, r1);

        std::cout << "Verdict: " << (ok_r1 ? "TRUE (semantically aligned)"
                                           : "FALSE (NOT aligned)") << std::endl;
        r1.print();
        r1.append_to_csv(csv, "CS8_aligned");

        if (!ok_r1) {
            std::cerr << "[WARNING] RUN 1: expected ALIGNED but got NOT ALIGNED.\n";
        }

        // ────────────────────────────────────────────────────────────────────
        // RUN 2: Syntactic bisimulation baseline -- same pair
        //
        // PT labels: takeoff_initiated!, transition_cruise_start!, ...
        // DT labels: vehicle_departure_event!, propulsion_mode_change!, ...
        //
        // Zero shared label strings. Syntactic bisimulation must fail.
        // Core RQ4 result: the avionics firmware vocabulary (IEC 61508) and the
        // airspace integration vocabulary (DO-365 / ASTM F3269-21) are deliberately
        // disjoint -- they describe the same flight operation from different regulatory
        // perspectives (vehicle cert vs airspace cert). Semantic alignment resolves
        // the vocabulary gap via the shared eVTOL domain ontology.
        //
        // Expected: FALSE
        // ────────────────────────────────────────────────────────────────────
        std::cout << "\n[RUN 2] Syntactic bisimulation baseline (same pair)..." << std::endl;

        rtwbs::SemanticAlignmentChecker syntactic_checker;
        rtwbs::SemanticAlignmentResult result_r2;
        bool ok_r2 = syntactic_checker.check_weak_timed_bisimulation(pt, dt, result_r2);

        std::cout << "Verdict: " << (ok_r2 ? "TRUE (syntactically bisimilar)"
                                           : "FALSE (avionics vs airspace vocabulary gap -- expected for RQ4)")
                  << std::endl;
        result_r2.print();

        if (ok_r2) {
            std::cerr << "[WARNING] RUN 2: expected syntactic FAILURE but got TRUE.\n";
        } else {
            std::cout << "[RQ4] Avionics (IEC 61508) vs airspace (DO-365/ASTM F3269-21) vocabulary gap confirmed.\n"
                      << "      Syntactic bisimulation fails with zero matching label strings.\n"
                      << "      Semantic alignment (RUN 1) resolves via ASTM F3269-21 ontology.\n";
        }

        // ────────────────────────────────────────────────────────────────────
        // RUN 3: Misalignment Variant A -- well-clear precondition gap
        //
        // dt_wc_gap.interp: propulsion_mode_change! adds tau_mod >= tau_mod_threshold
        // precondition. The PT's transition_cruise_start! requires only altitude >= 500
        // and battery_soc >= min_soc_operation (no traffic separation requirement).
        //
        // ASTM F3269-21 Sec.6.3 context:
        //   The DT enforces ACAS sXu / ASTM well-clear separation assurance (tau_mod >= 35s)
        //   before any propulsion mode change. The PT command logic is traffic-agnostic:
        //   the pilot may command a transition regardless of nearby traffic.
        //   This creates an asymmetric contract: the DT would refuse to match a PT
        //   mode-change event when tau_mod < 35s (close traffic scenario).
        //
        // Condition III violation (DT-to-PT direction):
        //   DT propulsion_mode_change! is strictly STRONGER than PT transition_cruise_start!.
        //   Counterexample: altitude = 600, battery_soc = 25, tau_mod = 20
        //     PT formula: true (altitude >= 500, soc >= min_soc_operation)
        //     DT formula: false (tau_mod = 20 < 35 = tau_mod_threshold)
        //   The DT would refuse to transition when the PT transitions.
        //
        // Expected: NOT ALIGNED (Condition III -- DT precondition stricter than PT)
        // ────────────────────────────────────────────────────────────────────
        std::cout << "\n[RUN 3] Misalignment Variant A: well-clear precondition gap..." << std::endl;
        std::cout << "        DT adds tau_mod >= 35s precondition absent from PT" << std::endl;
        std::cout << "        (ASTM F3269-21 Sec.6.3 separation assurance asymmetry)" << std::endl;

        auto dk_wc_gap = load_domain(ASSET_DIR + "domain.ont",
                                      ASSET_DIR + "pt.interp",
                                      ASSET_DIR + "dt_wc_gap.interp");

        rtwbs::SemanticAlignmentChecker checker_r3;
        rtwbs::SemanticAlignmentResult  r3;
        bool ok_r3 = checker_r3.check_semantic_alignment(pt, dt, dk_wc_gap, r3);

        std::cout << "Verdict: " << (ok_r3 ? "TRUE (unexpected -- DT should have stricter precondition)"
                                           : "FALSE (NOT aligned -- well-clear precondition gap detected)")
                  << std::endl;
        r3.print();
        r3.append_to_csv(csv, "CS8_wc_gap");

        if (!ok_r3) {
            std::cout << "[RQ2] Well-clear precondition gap correctly detected.\n"
                      << "      DT propulsion_mode_change! requires tau_mod >= 35s;\n"
                      << "      PT transition_cruise_start! has no traffic separation requirement.\n"
                      << "      ASTM F3269-21 Sec.6.3 asymmetric contract confirmed.\n";
        } else {
            std::cerr << "[WARNING] RUN 3: expected NOT ALIGNED but got ALIGNED.\n";
        }

        // ────────────────────────────────────────────────────────────────────
        // RUN 4: Misalignment Variant B -- SOC threshold drift
        //
        // dt_thresh_drift.interp: low_energy_alert! fires at battery_soc < 25 instead
        // of < min_soc_operation (= 20%). SAE AS6968 5% safety margin drift.
        //
        // Operational consequence:
        //   In battery_soc range [20%, 25%), the DT declares a low-energy emergency
        //   while the PT has NOT entered BATTERY_CRITICAL. This triggers unnecessary
        //   forced-descent commands, disrupting the flight mission and potentially
        //   causing airspace conflicts with other vehicles.
        //
        //   Drift fraction = 5 / 100 = 5 percentage points of SOC.
        //   SAE AS6968 certified limit: battery_soc < 20% triggers mandatory descent.
        //   DT threshold: battery_soc < 25% (25% excess caution area).
        //
        // Z3 counterexample: battery_soc = 22
        //   PT formula: (< 22 min_soc_operation) = (< 22 20) = false
        //   DT formula: (< 22 25) = true
        //   Semantically inequivalent at battery_critical! ~ low_energy_alert!
        //
        // Expected: NOT ALIGNED (Condition I -- state interpretation inconsistency)
        // ────────────────────────────────────────────────────────────────────
        std::cout << "\n[RUN 4] Misalignment Variant B: SOC threshold drift..." << std::endl;
        std::cout << "        PT threshold: battery_soc < 20% (SAE AS6968 min_soc_operation)" << std::endl;
        std::cout << "        DT threshold: battery_soc < 25% (5pp drift -- false alarms in [20,25))" << std::endl;

        auto dk_thresh = load_domain(ASSET_DIR + "domain.ont",
                                      ASSET_DIR + "pt.interp",
                                      ASSET_DIR + "dt_thresh_drift.interp");

        rtwbs::SemanticAlignmentChecker checker_r4;
        rtwbs::SemanticAlignmentResult  r4;
        bool ok_r4 = checker_r4.check_semantic_alignment(pt, dt, dk_thresh, r4);

        std::cout << "Verdict: " << (ok_r4 ? "TRUE (drift not bisimulation-critical)"
                                           : "FALSE (NOT aligned -- SOC threshold drift detected)")
                  << std::endl;
        r4.print();
        r4.append_to_csv(csv, "CS8_thresh_drift");

        if (!ok_r4) {
            double drift_pp = 25.0 - 20.0;
            std::cout << "[RQ2] SOC threshold drift correctly detected.\n"
                      << "      Drift: " << std::fixed << std::setprecision(0)
                      << drift_pp << " percentage points (SAE AS6968 5pp excess margin).\n"
                      << "      Operational risk: false forced-descent commands in SOC [20%, 25%).\n";
        } else {
            std::cerr << "[WARNING] RUN 4: expected NOT ALIGNED but got ALIGNED.\n";
        }

        // ────────────────────────────────────────────────────────────────────
        // RUN 5: Ontology evolution -- RQ1 (Theorem 1)
        //
        // domain_v2.ont changes:
        //   Condition I:  Add NOx sort + nox_emission_rate function (SAE ARP5765A)
        //                 eVTOL environmental certification new requirement.
        //   Condition II: min_soc_operation: 20% -> 22%
        //                 (SAE AS6968 revised recommendation for battery cycle life)
        //
        // pt_v2.interp and dt_v2.interp reference min_soc_operation symbolically.
        // Theorem 1: alignment preserved under Condition I (sort addition) and
        //            Condition II (axiom tightening) because all formulas use
        //            min_soc_operation rather than literal 20 or 22.
        //
        // Expected: ALIGNED = true
        // ────────────────────────────────────────────────────────────────────
        std::cout << "\n[RUN 5] Ontology evolution (RQ1, min_soc_operation: 20 -> 22%)..." << std::endl;
        std::cout << "        Condition I:  NOx sort + nox_emission_rate (SAE ARP5765A)" << std::endl;
        std::cout << "        Condition II: min_soc_operation tightened (SAE AS6968 revised)" << std::endl;

        auto dk_v2 = load_domain(ASSET_DIR + "domain_v2.ont",
                                  ASSET_DIR + "pt_v2.interp",
                                  ASSET_DIR + "dt_v2.interp");

        rtwbs::SemanticAlignmentChecker checker_r5;
        rtwbs::SemanticAlignmentResult  r5;
        bool ok_r5 = checker_r5.check_semantic_alignment(pt, dt, dk_v2, r5);

        std::cout << "Verdict under domain_v2.ont: "
                  << (ok_r5 ? "TRUE (alignment preserved -- Theorem 1 holds)"
                             : "FALSE (alignment broken by ontology evolution)")
                  << std::endl;
        r5.print();
        r5.append_to_csv(csv, "CS8_evo_v2");

        if (!ok_r5) {
            std::cerr << "[WARNING] RUN 5: ontology evolution broke alignment.\n";
        }

        // ────────────────────────────────────────────────────────────────────
        // Summary
        // ────────────────────────────────────────────────────────────────────
        std::cout << "\n[RQ2] CS8 Misalignment detection summary:\n";
        std::cout << std::left << std::setw(45) << "Variant"
                  << std::setw(20) << "SemAlign"
                  << "Note\n";
        std::cout << std::string(95, '-') << "\n";
        std::cout << std::setw(45) << "RUN 1: Aligned pair"
                  << std::setw(20) << (ok_r1 ? "ALIGNED" : "NOT ALIGNED")
                  << "12 label-equiv pairs via ASTM F3269-21 ontology\n";
        std::cout << std::setw(45) << "RUN 2: Syntactic bisimulation"
                  << std::setw(20) << (ok_r2 ? "ALIGNED" : "NOT ALIGNED")
                  << "zero shared labels (avionics vs airspace vocab)\n";
        std::cout << std::setw(45) << "RUN 3: Well-clear precondition gap"
                  << std::setw(20) << (ok_r3 ? "ALIGNED" : "NOT ALIGNED")
                  << "ASTM F3269-21 Sec.6.3 asymmetric contract\n";
        std::cout << std::setw(45) << "RUN 4: SOC threshold drift (+5pp)"
                  << std::setw(20) << (ok_r4 ? "ALIGNED" : "NOT ALIGNED")
                  << "SAE AS6968 5pp excess margin false alarms\n";
        std::cout << std::setw(45) << "RUN 5: Ontology evolution (Thm 1)"
                  << std::setw(20) << (ok_r5 ? "ALIGNED" : "NOT ALIGNED")
                  << "min_soc_operation 20->22% + NOx sort\n";

        std::cout << "\n[RQ4] Engineering cost summary:\n";
        std::cout << "  SemAlign one-time cost (RUN 1):       " << r1.time_ms << " ms\n";
        std::cout << "  Total SMT calls:                      " << r1.smt_calls_total << "\n";
        std::cout << "  Label-equiv pairs |E|:                " << r1.label_pairs_in_E << "\n";
        std::cout << "  Fixpoint iterations:                  " << r1.fixpoint_iterations << "\n";
        std::cout << "  PT zone count:                        " << pt.get_num_states() << "\n";
        std::cout << "  DT zone count:                        " << dt.get_num_states() << "\n";

        csv.close();
        std::cout << "\nResults written to: " << csv_path << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
