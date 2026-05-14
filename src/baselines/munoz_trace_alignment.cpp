/**
 * @file munoz_trace_alignment.cpp
 * @brief Implementation of MunozTraceAlignmentMonitor.
 *
 * Source: Munoz et al., "Trace Alignment for Digital Twin Monitoring",
 *         MODELS 2024. DOI: 10.1145/3652620.3688267
 */

#include "rtwbs/baselines/munoz_trace_alignment.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>
#include <vector>
#include <numeric>

namespace rtwbs {
namespace baselines {

// ============================================================================
// Construction
// ============================================================================

MunozTraceAlignmentMonitor::MunozTraceAlignmentMonitor(
        size_t num_traces, size_t max_steps,
        size_t window_size, double threshold, uint64_t seed)
    : num_traces_(num_traces), max_steps_(max_steps),
      window_size_(window_size), threshold_(threshold), seed_(seed)
{}

// ============================================================================
// Trace simulation
// ============================================================================

std::vector<std::string>
MunozTraceAlignmentMonitor::simulate_trace(const TimedAutomaton& ta,
                                            size_t max_steps,
                                            uint64_t rng_state) const
{
    // Simple xorshift64 — lightweight, no std::mt19937 overhead per trace.
    auto xorshift = [](uint64_t& s) -> uint64_t {
        s ^= s << 13; s ^= s >> 7; s ^= s << 17;
        return s;
    };

    const auto& transitions = ta.get_transitions();
    std::vector<std::string> trace;
    trace.reserve(max_steps / 2);

    int cur_loc = 0;  // start at initial location (id 0)

    for (size_t step = 0; step < max_steps; ++step) {
        // Collect enabled transitions from cur_loc
        std::vector<size_t> enabled;
        for (size_t i = 0; i < transitions.size(); ++i) {
            if (transitions[i].from_location == cur_loc)
                enabled.push_back(i);
        }
        if (enabled.empty()) break;

        size_t idx = enabled[xorshift(rng_state) % enabled.size()];
        const Transition& tr = transitions[idx];
        cur_loc = tr.to_location;

        // Emit observable label: channel+suffix for syncs, action for internals.
        // Tau (internal) transitions do not produce a label in the trace.
        if (!tr.channel.empty()) {
            // Sync transition
            trace.push_back(tr.channel + (tr.is_sender ? "!" : "?"));
        } else if (!tr.action.empty() &&
                   tr.action != "tau" && tr.action != "_tau_") {
            // Non-tau internal action
            trace.push_back(tr.action);
        }
        // else: tau — silent, not emitted
    }
    return trace;
}

// ============================================================================
// Label matching
// ============================================================================

bool MunozTraceAlignmentMonitor::labels_match(
        const std::string& label_a,
        const std::string& label_b,
        const std::unordered_map<std::string,
                                  std::unordered_set<std::string>>& E)
{
    if (label_a == label_b) return true;
    // Check semantic equivalence via E: label_a is a PT label, label_b is a DT label.
    auto it = E.find(label_a);
    if (it != E.end() && it->second.count(label_b)) return true;
    // Also check the reverse direction (DT label maps to PT equiv set).
    for (const auto& [pt_lbl, equiv_set] : E) {
        if (pt_lbl == label_b && equiv_set.count(label_a)) return true;
    }
    return false;
}

// ============================================================================
// Needleman-Wunsch
// ============================================================================

double MunozTraceAlignmentMonitor::needleman_wunsch(
        const std::vector<std::string>& seq_a,
        const std::vector<std::string>& seq_b,
        const std::unordered_map<std::string,
                                  std::unordered_set<std::string>>& E) const
{
    const int GAP      = -1;
    const int MATCH    =  2;
    const int MISMATCH = -1;

    const size_t m = seq_a.size();
    const size_t n = seq_b.size();

    if (m == 0 && n == 0) return 1.0;
    if (m == 0 || n == 0) {
        // One sequence is empty — all gaps
        int raw = GAP * static_cast<int>(std::max(m, n));
        int max_score = MATCH * static_cast<int>(std::max(m, n));
        if (max_score <= 0) return 0.0;
        return std::max(0.0, static_cast<double>(raw) / static_cast<double>(max_score));
    }

    // DP table: (m+1) × (n+1), stored flat.
    std::vector<std::vector<int>> dp(m + 1, std::vector<int>(n + 1, 0));

    for (size_t i = 0; i <= m; ++i) dp[i][0] = GAP * static_cast<int>(i);
    for (size_t j = 0; j <= n; ++j) dp[0][j] = GAP * static_cast<int>(j);

    for (size_t i = 1; i <= m; ++i) {
        for (size_t j = 1; j <= n; ++j) {
            int diag = dp[i-1][j-1] +
                       (labels_match(seq_a[i-1], seq_b[j-1], E) ? MATCH : MISMATCH);
            int del  = dp[i-1][j] + GAP;
            int ins  = dp[i][j-1] + GAP;
            dp[i][j] = std::max({diag, del, ins});
        }
    }

    int raw       = dp[m][n];
    int max_score = MATCH * static_cast<int>(std::max(m, n));
    if (max_score <= 0) return 1.0;
    return std::max(0.0, static_cast<double>(raw) / static_cast<double>(max_score));
}

// ============================================================================
// run()
// ============================================================================

MunozTraceResult MunozTraceAlignmentMonitor::run(
        const TimedAutomaton& pt,
        const TimedAutomaton& dt,
        const std::unordered_map<std::string,
                                  std::unordered_set<std::string>>& label_equiv_E) const
{
    MunozTraceResult result;
    result.monitor_source = kMunozMonitorSource;
    result.num_traces     = num_traces_;

    const auto t_start = std::chrono::steady_clock::now();

    std::vector<double> scores;
    scores.reserve(num_traces_);

    uint64_t rng_pt = seed_ * 0x9E3779B97F4A7C15ULL + 1;
    uint64_t rng_dt = seed_ * 0x6C62272E07BB0142ULL + 1;

    size_t total_pt_steps = 0;

    for (size_t t = 0; t < num_traces_; ++t) {
        // Use distinct RNG streams for PT and DT
        rng_pt += 0xA24BAED4963EE407ULL;
        rng_dt += 0x9FB21C651E98DF25ULL;

        auto pt_trace = simulate_trace(pt, max_steps_, rng_pt);
        auto dt_trace = simulate_trace(dt, max_steps_, rng_dt);
        total_pt_steps += pt_trace.size();
        result.trace_length = std::max(result.trace_length, pt_trace.size());

        double score = needleman_wunsch(pt_trace, dt_trace, label_equiv_E);
        scores.push_back(score);

        // Sliding-window anomaly detection
        if (scores.size() >= window_size_) {
            size_t win_start = scores.size() - window_size_;
            double win_sum = 0.0;
            for (size_t w = win_start; w < scores.size(); ++w)
                win_sum += scores[w];
            double win_avg = win_sum / static_cast<double>(window_size_);

            if (win_avg < threshold_) {
                result.anomaly_detected = true;
                ++result.traces_to_detect;
            }
        }
    }

    const auto t_end = std::chrono::steady_clock::now();
    result.total_time_ms = std::chrono::duration<double, std::milli>(
                               t_end - t_start).count();

    // Average scores
    if (!scores.empty()) {
        result.avg_score_aligned = std::accumulate(scores.begin(), scores.end(), 0.0)
                                   / static_cast<double>(scores.size());
        // "misaligned" score = complement of aligned
        result.avg_score_misaligned = 1.0 - result.avg_score_aligned;
    }

    // Per-step overhead: total_time / total_pt_steps
    if (total_pt_steps > 0) {
        result.overhead_per_step_us = (result.total_time_ms * 1000.0) /
                                       static_cast<double>(total_pt_steps);
    }

    return result;
}

} // namespace baselines
} // namespace rtwbs
