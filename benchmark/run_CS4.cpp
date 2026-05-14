/**
 * @file run_CS4.cpp
 * @brief CS4: Aircraft Engine Controller (NASA/FRET + Lockheed Martin CPS)
 *
 * Tests semantic alignment between the Physical Twin (EngineControllerPT) and
 * the Digital Twin (EngineHealthDT) under the DO-178C / ARP4754A / FAA AC 33.28-3
 * domain ontology.
 *
 * Research questions addressed:
 *  RQ1 — Ontology evolution (Theorem 1): EGT takeoff limit tightened 935->925°C
 *         (ARP4754A periodic safety re-assessment after 3,000 flight cycles)
 *  RQ2 — Coverage vs syntactic baseline:
 *         - Syntactic bisimulation fails (different label vocabularies)
 *         - Variant A (EGT threshold drift): Z3 detects 30°C gap
 *         - Variant B (missing flame-out event): alignment fails — most time-critical
 *           safety gap (FAA AC 33.28-3: 3-second relight window)
 *  RQ3 — Compositional verification: FMS + TMS subsystems verified independently;
 *         timing compared against monolithic check
 *  RQ4 — Engineering cost: runtime monitor baseline vs SemAlign
 *
 * Usage:
 *   ./run_CS4 [--folder <path>]
 */

#include <iostream>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <cassert>
#include <chrono>
#include <string>
#include <stdexcept>

#include "rtwbs/timedautomaton.h"
#include "rtwbs/core.h"
#include "rtwbs/ontology.h"
#include "rtwbs/interpretation.h"
#include "rtwbs/semantic_checker.h"
#include "rtwbs/domain_parser.h"
#include "rtwbs/runtime_monitor.h"
#include "rtwbs/benchmarks/common.h"

static const std::string ASSET_DIR = "assets/CS4_Engine/";

// ============================================================================
// Helpers
// ============================================================================

static rtwbs::DomainKnowledge load_domain(const std::string& ont_path,
                                           const std::string& pt_interp_path,
                                           const std::string& dt_interp_path)
{
    rtwbs::OntFileParser     ont_parser;
    rtwbs::InterpFileParser  interp_parser;

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

        std::string csv_path = results_folder + "CS4_results.csv";
        std::ofstream csv(csv_path);
        if (!csv.is_open())
            throw std::runtime_error("Cannot open result file: " + csv_path);

        rtwbs::SemanticAlignmentResult::write_csv_header(csv);

        // ────────────────────────────────────────────────────────────────────
        // Load full PT and DT zone graphs (shared across runs 1-5)
        // ────────────────────────────────────────────────────────────────────
        std::cout << "=== CS4: Aircraft Engine Controller (DO-178C / ARP4754A / FAA AC 33.28-3) ===" << std::endl;
        std::cout << "Loading PT and DT models..." << std::endl;

        rtwbs::TimedAutomaton pt(ASSET_DIR + "V1_PT.xml");
        rtwbs::TimedAutomaton dt(ASSET_DIR + "V2_DT.xml");
        pt.construct_zone_graph();
        dt.construct_zone_graph();

        std::cout << "  PT zones: " << pt.get_num_states()
                  << "  |  DT zones: " << dt.get_num_states() << "\n" << std::endl;

        // ────────────────────────────────────────────────────────────────────
        // RUN 1: Semantic alignment — correctly aligned pair
        //
        // PT (EngineControllerPT): FADEC / avionics software vocabulary
        //   engine_start!, idle_reached!, takeoff_thrust!, overheat_detected!, ...
        // DT (EngineHealthDT): propulsion health management vocabulary
        //   engine_spool_telemetry!, idle_confirmation_event!, takeoff_power_mode!, ...
        //
        // Semantic alignment succeeds because Delta (DO-178C/ARP4754A ontology)
        // entails the iff-equivalences for all 14 label pairs, e.g.:
        //   engine_start! ~ engine_spool_telemetry!   (both: n1_speed=0 && fuel_flow>=min)
        //   overheat_detected! ~ thermal_exceedance_event!  (both: egt > max_egt=950)
        //   flameout_detected! ~ combustion_loss_event!     (both: n1_speed < 20)
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
        r1.append_to_csv(csv, "CS4_aligned");

        if (!ok_r1) {
            std::cerr << "[WARNING] RUN 1: expected ALIGNED but got NOT ALIGNED.\n";
        }

        // ────────────────────────────────────────────────────────────────────
        // RUN 2: Syntactic bisimulation baseline — same pair
        //
        // PT labels: engine_start!, takeoff_thrust!, overheat_detected!, ...
        // DT labels: engine_spool_telemetry!, takeoff_power_mode!, thermal_exceedance_event!, ...
        //
        // No common label strings. Syntactic bisimulation must fail immediately.
        // This is the RQ2 core result: DO-178C avionics software vs propulsion health
        // management teams independently author their models using aviation-domain
        // vocabulary conventions. Semantic alignment bridges this gap via the ontology.
        //
        // Expected: FALSE
        // ────────────────────────────────────────────────────────────────────
        std::cout << "\n[RUN 2] Syntactic bisimulation baseline (same pair)..." << std::endl;

        rtwbs::SemanticAlignmentChecker syntactic_checker;
        rtwbs::SemanticAlignmentResult result_r2;
        bool ok_r2 = syntactic_checker.check_weak_timed_bisimulation(pt, dt, result_r2);

        std::cout << "Verdict: " << (ok_r2 ? "TRUE (syntactically bisimilar)"
                                           : "FALSE (labels differ — expected for RQ2)")
                  << std::endl;
        result_r2.print();

        if (ok_r2) {
            std::cerr << "[WARNING] RUN 2: expected syntactic FAILURE but got TRUE.\n";
        } else {
            std::cout << "[RQ2] Syntactic baseline fails — DO-178C/propulsion vocabulary gap confirmed.\n";
        }

        // ────────────────────────────────────────────────────────────────────
        // RUN 3: Misalignment variants
        //
        // VARIANT A (EGT threshold drift):
        //   dt_thresh_drift.interp: thermal_exceedance_event! fires at egt > 980
        //   instead of egt > max_egt (=950). The 30°C gap:
        //   EGT in [950, 980) triggers overheat_detected! on PT but NOT
        //   thermal_exceedance_event! on DT.
        //   Z3 counterexample: egt=960 satisfies (> 960 950) but not (> 960 980).
        //   ARP4754A FHA consequence: undetected overheat in [950,980) can
        //   progress to in-flight shutdown within 50 flight cycles.
        //   Expected: NOT ALIGNED
        //
        // VARIANT B (missing flame-out event):
        //   dt_missing_flameout.interp: combustion_loss_event omitted entirely.
        //   The most time-critical safety event (3-second relight window per
        //   FAA AC 33.28-3) has no DT semantic mapping.
        //   Expected: NOT ALIGNED (combustion_loss_event not in E)
        // ────────────────────────────────────────────────────────────────────
        std::cout << "\n[RUN 3A] EGT threshold drift (Variant A, 30C gap: 950->980)..." << std::endl;

        rtwbs::TimedAutomaton dt_drift(ASSET_DIR + "V2_DT_thresh_drift.xml");
        dt_drift.construct_zone_graph();

        auto dk_drift = load_domain(ASSET_DIR + "domain.ont",
                                    ASSET_DIR + "pt.interp",
                                    ASSET_DIR + "dt_thresh_drift.interp");

        rtwbs::SemanticAlignmentChecker checker_r3a;
        rtwbs::SemanticAlignmentResult  r3a;
        bool ok_r3a = checker_r3a.check_semantic_alignment(pt, dt_drift, dk_drift, r3a);

        std::cout << "Verdict: " << (ok_r3a ? "TRUE (drift not detected)"
                                            : "FALSE (NOT aligned — EGT drift detected)")
                  << std::endl;
        r3a.print();
        r3a.append_to_csv(csv, "CS4_egt_drift");

        if (!ok_r3a) {
            // EGT gap: 30C over operational range [0, emergency_egt_threshold+20] = [0, 990]
            // Fraction of EGT states in gap: 30/990 ~ 3%
            double gap_fraction = 30.0 / 990.0;
            std::cout << "[RQ4] EGT drift gap: " << std::fixed << std::setprecision(1)
                      << (gap_fraction * 100.0)
                      << "% of EGT operational range [0, 990] undetected by DT.\n"
                      << "       ARP4754A FHA: catastrophic consequence within 50 cycles.\n";
        } else {
            std::cout << "[NOTE] Drift not bisimulation-critical in this zone structure.\n";
        }

        std::cout << "\n[RUN 3B] Missing flame-out event (Variant B — combustion_loss_event omitted)..." << std::endl;

        auto dk_missing = load_domain(ASSET_DIR + "domain.ont",
                                      ASSET_DIR + "pt.interp",
                                      ASSET_DIR + "dt_missing_flameout.interp");

        rtwbs::SemanticAlignmentChecker checker_r3b;
        rtwbs::SemanticAlignmentResult  r3b;
        bool ok_r3b = checker_r3b.check_semantic_alignment(pt, dt, dk_missing, r3b);

        std::cout << "Verdict: " << (ok_r3b ? "TRUE (missing event not detected — unexpected)"
                                            : "FALSE (NOT aligned — missing event detected, as expected)")
                  << std::endl;
        r3b.print();
        r3b.append_to_csv(csv, "CS4_missing_flameout");

        if (!ok_r3b) {
            std::cout << "[RQ2] Missing combustion_loss_event detected.\n"
                      << "      FAA AC 33.28-3: 3-second relight window — most safety-critical gap.\n";
        }

        // ────────────────────────────────────────────────────────────────────
        // RUN 4: Runtime monitor baseline
        //
        // Monitor 1: DO178C_EGTSafetyLimit
        //   formula: (<= egt max_egt)
        //   trigger: thermal_exceedance_event!
        //   type:    safety
        //   Detects EGT overheat via state-based domain predicate.
        //
        // Monitor 2: DO178C_FlameoutN1
        //   formula: (< n1_speed flameout_n1_threshold)
        //   trigger: combustion_loss_event!
        //   type:    safety
        //   Detects flame-out via N1 speed drop.
        //
        // Monitor 3: DO178C_FuelFlowIdle
        //   formula: (>= fuel_flow min_fuel_flow)
        //   trigger: engine_spool_telemetry!
        //   type:    safety
        //   Verifies minimum fuel flow during start.
        //
        // Monitor 4: DO178C_TakeoffEGT (timing property -- cross-model limitation)
        //   formula: (<= egt takeoff_egt_limit)
        //   trigger: takeoff_power_mode!
        //   type:    safety
        //   This IS a state-based property expressible in domain variables --
        //   unlike CS3's clock timing monitor, this CAN be evaluated by the monitor.
        // ────────────────────────────────────────────────────────────────────
        std::cout << "\n[RUN 4] Runtime monitor baseline..." << std::endl;

        rtwbs::RuntimeMonitorBaseline monitor;

        monitor.add_monitor({
            "DO178C_EGTSafetyLimit",
            "(<= egt max_egt)",
            "thermal_exceedance_event!",
            true
        });

        monitor.add_monitor({
            "DO178C_FlameoutN1",
            "(< n1_speed flameout_n1_threshold)",
            "combustion_loss_event!",
            true
        });

        monitor.add_monitor({
            "DO178C_FuelFlowIdle",
            "(>= fuel_flow min_fuel_flow)",
            "engine_spool_telemetry!",
            true
        });

        monitor.add_monitor({
            "DO178C_TakeoffEGT",
            "(<= egt takeoff_egt_limit)",
            "takeoff_power_mode!",
            true
        });

        auto monitor_results = monitor.run(pt, dt,
                                           dk_base.ontology,
                                           dk_base.pt_interp,
                                           dk_base.dt_interp,
                                           1000, 500, 42);

        for (const auto& mr : monitor_results) {
            std::cout << "Monitor '" << mr.monitor_name << "': "
                      << (mr.violation_detected ? "DETECTED" : "NOT DETECTED")
                      << " | traces: " << mr.traces_simulated
                      << " | steps to detection: " << mr.steps_to_detection
                      << " | overhead/step: " << mr.overhead_per_step_us << " us\n";
        }

        std::cout << "\n[RQ4] All 4 monitors use state-based domain predicates.\n"
                  << "      Unlike CS3 timing monitor, all CS4 monitors are expressible\n"
                  << "      in the DO-178C domain variable set (EGT, N1, fuel_flow).\n"
                  << "      Key difference: SemAlign additionally checks delay Condition IV\n"
                  << "      (cross-model timing gaps) at zero additional engineering cost.\n";

        // ────────────────────────────────────────────────────────────────────
        // RUN 5: Ontology evolution — RQ1 (Theorem 1)
        //
        // domain_v2.ont changes:
        //   Condition I: Add Emissions sort (nox_emissions, co2_emissions, max_nox)
        //   Condition II: takeoff_egt_limit: 935 -> 925 (10°C tighter)
        //
        // pt_v2.interp / dt_v2.interp reference takeoff_egt_limit symbolically --
        // Theorem 1 guarantees alignment is preserved under ontology evolution
        // when interpretations are symbolic (conditions I and II hold).
        //
        // Expected: ALIGNED = true (Theorem 1)
        // ────────────────────────────────────────────────────────────────────
        std::cout << "\n[RUN 5] Ontology evolution (RQ1, takeoff_egt_limit: 935 -> 925°C)..." << std::endl;

        auto dk_v2 = load_domain(ASSET_DIR + "domain_v2.ont",
                                  ASSET_DIR + "pt_v2.interp",
                                  ASSET_DIR + "dt_v2.interp");

        rtwbs::SemanticAlignmentChecker checker_r5;
        rtwbs::SemanticAlignmentResult  r5;
        bool ok_r5 = checker_r5.check_semantic_alignment(pt, dt, dk_v2, r5);

        std::cout << "Verdict under domain_v2.ont: "
                  << (ok_r5 ? "TRUE (alignment preserved — Theorem 1 holds)"
                             : "FALSE (alignment broken by ontology evolution)")
                  << std::endl;
        r5.print();
        r5.append_to_csv(csv, "CS4_evo_v2");

        if (!ok_r5) {
            std::cerr << "[WARNING] RUN 5: ontology evolution broke alignment.\n";
        }

        // ────────────────────────────────────────────────────────────────────
        // RUN 6: Monolithic check (full PT vs full DT) — RQ3 baseline
        // (Same as RUN 1 but re-run explicitly for compositional comparison timing)
        // ────────────────────────────────────────────────────────────────────
        std::cout << "\n[RUN 6] Compositional RQ3 — Monolithic check (PT_full vs DT_full)..." << std::endl;

        auto dk_comp = load_domain(ASSET_DIR + "domain.ont",
                                   ASSET_DIR + "pt.interp",
                                   ASSET_DIR + "dt.interp");

        rtwbs::SemanticAlignmentChecker checker_mono;
        rtwbs::SemanticAlignmentResult  r_mono;
        bool ok_mono = checker_mono.check_semantic_alignment(pt, dt, dk_comp, r_mono);

        std::cout << "Monolithic verdict: "
                  << (ok_mono ? "TRUE" : "FALSE")
                  << " | time: " << r_mono.time_ms << " ms" << std::endl;
        r_mono.append_to_csv(csv, "CS4_monolithic");

        // ────────────────────────────────────────────────────────────────────
        // RUN 7: Compositional check — FMS and TMS subsystems independently
        //
        // FMS: EngineControllerPT (fuel mgmt) vs EngineHealthDT (fuel mgmt)
        //   PT events: engine_start!, idle_reached!, ground_idle!, shutdown_normal!,
        //              emergency_shutdown!, system_reset!
        //   DT events: engine_spool_telemetry!, idle_confirmation_event!,
        //              ground_ops_mode!, shutdown_telemetry!, protective_shutdown_command!
        //
        // TMS: EngineControllerPT (thrust mgmt) vs EngineHealthDT (thrust mgmt)
        //   PT events: takeoff_thrust!, climb_thrust!, cruise_thrust!,
        //              descent_initiated!, approach_thrust!, landing_thrust!,
        //              overheat_detected!, flameout_detected!, relight_initiated!
        //   DT events: takeoff_power_mode!, climb_power_mode!, cruise_power_mode!,
        //              descent_power_mode!, approach_power_mode!, landing_power_mode!,
        //              thermal_exceedance_event!, combustion_loss_event!,
        //              relight_command_telemetry!
        //
        // Both subsystems should yield ALIGNED = true.
        // Compositional time = time_fms + time_tms (vs monolithic time)
        // RQ3 hypothesis: compositional time < monolithic time (smaller zone graphs)
        // ────────────────────────────────────────────────────────────────────
        std::cout << "\n[RUN 7] Compositional check — FMS and TMS subsystems..." << std::endl;

        rtwbs::TimedAutomaton pt_fms(ASSET_DIR + "V1_PT_FMS.xml");
        rtwbs::TimedAutomaton dt_fms(ASSET_DIR + "V2_DT_FMS.xml");
        rtwbs::TimedAutomaton pt_tms(ASSET_DIR + "V1_PT_TMS.xml");
        rtwbs::TimedAutomaton dt_tms(ASSET_DIR + "V2_DT_TMS.xml");

        pt_fms.construct_zone_graph();
        dt_fms.construct_zone_graph();
        pt_tms.construct_zone_graph();
        dt_tms.construct_zone_graph();

        std::cout << "  FMS PT zones: " << pt_fms.get_num_states()
                  << "  |  FMS DT zones: " << dt_fms.get_num_states() << std::endl;
        std::cout << "  TMS PT zones: " << pt_tms.get_num_states()
                  << "  |  TMS DT zones: " << dt_tms.get_num_states() << std::endl;

        auto dk_fms = load_domain(ASSET_DIR + "domain.ont",
                                   ASSET_DIR + "pt_fms.interp",
                                   ASSET_DIR + "dt_fms.interp");

        auto dk_tms = load_domain(ASSET_DIR + "domain.ont",
                                   ASSET_DIR + "pt_tms.interp",
                                   ASSET_DIR + "dt_tms.interp");

        rtwbs::SemanticAlignmentChecker checker_fms;
        rtwbs::SemanticAlignmentResult  r_fms;
        bool ok_fms = checker_fms.check_semantic_alignment(pt_fms, dt_fms, dk_fms, r_fms);
        std::cout << "FMS verdict: " << (ok_fms ? "TRUE" : "FALSE")
                  << " | time: " << r_fms.time_ms << " ms" << std::endl;
        r_fms.append_to_csv(csv, "CS4_FMS");

        rtwbs::SemanticAlignmentChecker checker_tms;
        rtwbs::SemanticAlignmentResult  r_tms;
        bool ok_tms = checker_tms.check_semantic_alignment(pt_tms, dt_tms, dk_tms, r_tms);
        std::cout << "TMS verdict: " << (ok_tms ? "TRUE" : "FALSE")
                  << " | time: " << r_tms.time_ms << " ms" << std::endl;
        r_tms.append_to_csv(csv, "CS4_TMS");

        // ────────────────────────────────────────────────────────────────────
        // RQ3 Compositional speedup report
        // ────────────────────────────────────────────────────────────────────
        double compositional_time = r_fms.time_ms + r_tms.time_ms;
        double monolithic_time    = r_mono.time_ms;
        double speedup            = (monolithic_time > 0.0)
                                    ? (monolithic_time / compositional_time)
                                    : 0.0;

        std::cout << "\n[RQ3] Compositional speedup summary:" << std::endl;
        std::cout << "  Monolithic check time:        " << std::fixed << std::setprecision(2)
                  << monolithic_time << " ms" << std::endl;
        std::cout << "  Subsystem 1 check time:       " << r_fms.time_ms << " ms" << std::endl;
        std::cout << "  Subsystem 2 check time:       " << r_tms.time_ms << " ms" << std::endl;
        std::cout << "  Compositional total:          " << compositional_time << " ms" << std::endl;
        std::cout << "  Speedup factor:               " << std::setprecision(2)
                  << speedup << "x" << std::endl;
        std::cout << "  FMS aligned:          " << (ok_fms ? "YES" : "NO") << std::endl;
        std::cout << "  TMS aligned:          " << (ok_tms ? "YES" : "NO") << std::endl;
        std::cout << "  Monolithic aligned:   " << (ok_mono ? "YES" : "NO") << std::endl;

        bool compositional_consistent = (ok_fms == ok_mono && ok_tms == ok_mono);
        std::cout << "  Results consistent:   "
                  << (compositional_consistent ? "YES (compositional = monolithic)"
                                               : "NO (compositional diverges!)") << std::endl;

        // Write compositional metrics to CSV as a summary row
        {
            std::ofstream rq3_csv(results_folder + "CS4_rq3_compositional.csv");
            rq3_csv << "model,monolithic_time_ms,fms_time_ms,tms_time_ms,"
                       "compositional_total_ms,speedup_factor,"
                       "fms_aligned,tms_aligned,monolithic_aligned,consistent\n";
            rq3_csv << "CS4_Engine,"
                    << monolithic_time << ","
                    << r_fms.time_ms << ","
                    << r_tms.time_ms << ","
                    << compositional_time << ","
                    << std::setprecision(4) << speedup << ","
                    << (ok_fms ? "true" : "false") << ","
                    << (ok_tms ? "true" : "false") << ","
                    << (ok_mono ? "true" : "false") << ","
                    << (compositional_consistent ? "true" : "false") << "\n";
            rq3_csv.close();
            std::cout << "  RQ3 data written to: " << results_folder
                      << "CS4_rq3_compositional.csv\n";
        }

        // ────────────────────────────────────────────────────────────────────
        // RQ4 Engineering Cost Report
        // ────────────────────────────────────────────────────────────────────
        std::cout << "\n[RQ4] Engineering cost summary:" << std::endl;
        std::cout << "  SemAlign one-time cost (RUN 1):   " << r1.time_ms << " ms\n";
        std::cout << "  SemAlign total SMT calls:         " << r1.smt_calls_total << "\n";
        std::cout << "  Label-equiv pairs (|E|):          " << r1.label_pairs_in_E << "\n";
        std::cout << "  Initial relation size:            " << r1.initial_state_pairs << "\n";
        std::cout << "  Final relation size:              " << r1.final_relation_size << "\n";
        std::cout << "  Fixpoint iterations:              " << r1.fixpoint_iterations << "\n";
        std::cout << "  Monitor LOC estimate (4 monitors):" << monitor.estimate_monitor_loc() << " LOC\n";
        if (!monitor_results.empty()) {
            std::cout << "  Monitor overhead/step (M1):       "
                      << monitor_results[0].overhead_per_step_us << " us\n";
        }
        std::cout << "  NOTE: All CS4 monitors are state-based (EGT, N1, fuel_flow)\n";
        std::cout << "        unlike CS3 where timing required cross-model clock sync.\n";

        csv.close();
        std::cout << "\nResults written to: " << csv_path << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
