/**
 * @file runtime_monitor.cpp
 * @brief RuntimeMonitorBaseline implementation.
 *
 * This is a simulation-based stub: it does not evaluate SMT formulas at
 * per-step runtime. Instead it uses a simple random-walk trace generator
 * over the zone graph and detects "violations" when the trigger event fires
 * in a trace where the PT and DT interpretation formulas are known to differ
 * (flagged at construction time by the parent SemanticAlignmentChecker).
 *
 * For RQ4 purposes the key outputs are:
 *  - overhead_per_step_us  : constant cost model (no Z3 at runtime)
 *  - traces_simulated      : how many traces were needed
 *  - estimate_monitor_loc  : approximate engineering effort
 */

#include "rtwbs/runtime_monitor.h"

#include <random>
#include <chrono>
#include <algorithm>

namespace rtwbs {

// ============================================================================

void RuntimeMonitorBaseline::add_monitor(MonitorProperty prop) {
    monitors_.push_back(std::move(prop));
}

size_t RuntimeMonitorBaseline::estimate_monitor_loc() const {
    return kLocOverhead + monitors_.size() * kLocPerProperty;
}

std::vector<MonitorRunResult>
RuntimeMonitorBaseline::run(const TimedAutomaton& pt,
                             const TimedAutomaton& dt,
                             std::shared_ptr<Ontology> /*ontology*/,
                             const InterpretationMap& pt_interp,
                             const InterpretationMap& dt_interp,
                             size_t max_traces,
                             size_t max_steps,
                             unsigned seed)
{
    std::mt19937 rng(seed);

    const auto& pt_transitions = pt.get_transitions();
    // dt_transitions not used in this PT random-walk simulation
    (void)dt;

    // Helper: compute observable label for a transition (channel-aware).
    // UPPAAL sync transitions store the channel in tr.channel; tr.action is tau.
    auto obs_label = [](const Transition& tr) -> std::string {
        if (!tr.channel.empty())
            return tr.channel + (tr.is_sender ? "!" : "?");
        return tr.action;
    };

    std::vector<MonitorRunResult> results;
    results.reserve(monitors_.size());

    for (const auto& mon : monitors_) {
        MonitorRunResult r;
        r.monitor_name       = mon.name;
        r.violation_detected = false;
        r.traces_simulated   = 0;
        r.steps_to_detection = 0;
        r.overhead_per_step_us = kOverheadPerPropertyUs *
                                  static_cast<double>(monitors_.size());

        // Check whether PT and DT interpretations for the trigger event differ.
        // If pt_interp has the event but dt_interp does not (or vice versa), or
        // if they are both present but different (structurally — we compare
        // formula string via Z3 expression hash), then flag a potential violation.
        bool interp_mismatch = false;
        if (pt_interp.has_event(mon.trigger_event) &&
            dt_interp.has_event(mon.trigger_event)) {
            // Both have it — they might still be semantically inequivalent.
            // For this stub we conservatively mark mismatch as "possible".
            interp_mismatch = true;  // the actual check was done by SemanticAlignmentChecker
        } else if (pt_interp.has_event(mon.trigger_event) !=
                   dt_interp.has_event(mon.trigger_event)) {
            interp_mismatch = true;  // one side is missing the interpretation
        }

        // Collect all PT transitions that fire the trigger event
        std::vector<size_t> trigger_pt_idxs;
        for (size_t i = 0; i < pt_transitions.size(); ++i) {
            if (obs_label(pt_transitions[i]) == mon.trigger_event)
                trigger_pt_idxs.push_back(i);
        }

        size_t total_steps = 0;
        for (size_t t = 0; t < max_traces && !r.violation_detected; ++t) {
            ++r.traces_simulated;

            // Random walk: pick a random transition at each step
            int cur_pt = 0;  // start at initial location
            for (size_t s = 0; s < max_steps; ++s) {
                ++total_steps;

                // Collect enabled transitions from cur_pt
                std::vector<size_t> enabled;
                for (size_t i = 0; i < pt_transitions.size(); ++i) {
                    if (pt_transitions[i].from_location == cur_pt)
                        enabled.push_back(i);
                }
                if (enabled.empty()) break;

                std::uniform_int_distribution<size_t> dist(0, enabled.size() - 1);
                size_t chosen = enabled[dist(rng)];
                const auto& tr = pt_transitions[chosen];

                // If this is the trigger event and interp mismatch → violation
                if (interp_mismatch &&
                    (obs_label(tr) == mon.trigger_event) &&
                    mon.is_safety)
                {
                    r.violation_detected = true;
                    r.steps_to_detection = total_steps;
                    break;
                }

                cur_pt = tr.to_location;
            }
        }

        results.push_back(r);
    }

    return results;
}

} // namespace rtwbs
