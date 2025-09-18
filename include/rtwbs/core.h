#ifndef RTWBS_CORE_H
#define RTWBS_CORE_H

#include "timedautomaton.h"
#include "configs.h"
#include "system.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <queue>
#include <functional>
#include <fstream>

namespace rtwbs {


struct PairHash {
    std::size_t operator()(const std::pair<const ZoneState*, const ZoneState*>& p) const {
        auto h1 = std::hash<const ZoneState*>{}(p.first);
        auto h2 = std::hash<const ZoneState*>{}(p.second);
        return h1 ^ (h2 << 1);
    }
};

    /**
     * Get statistics about the last refinement check
     */
    struct CheckStatistics {
    size_t refined_states;
    size_t abstract_states;
    size_t simulation_pairs;
    double check_time_ms;
    size_t memory_usage_bytes;

    // Overload + operator to combine statistics
    CheckStatistics operator+(const CheckStatistics& other) const {
        return CheckStatistics{
            refined_states + other.refined_states,
            abstract_states + other.abstract_states,
            simulation_pairs + other.simulation_pairs,
            check_time_ms + other.check_time_ms,
            memory_usage_bytes + other.memory_usage_bytes
        };
    }

    // Overload += operator for convenience
    CheckStatistics& operator+=(const CheckStatistics& other) {
        refined_states += other.refined_states;
        abstract_states += other.abstract_states;
        simulation_pairs += other.simulation_pairs;
        check_time_ms += other.check_time_ms;
        memory_usage_bytes += other.memory_usage_bytes;
        return *this;
    }
    void print() const {
        std::cout << "RTWBS Check Statistics:" << std::endl;
        std::cout << "  Refined States: " << refined_states << std::endl;
        std::cout << "  Abstract States: " << abstract_states << std::endl;
        std::cout << "  Simulation Pairs: " << simulation_pairs << std::endl;
        std::cout << "  Check Time: " << check_time_ms << " ms" << std::endl;
        std::cout << "  Memory Usage: " << memory_usage_bytes / 1024 << " KB" << std::endl;
    }
    
    // Write CSV header
    static void write_csv_header(std::ofstream& file) {
        file << "model_name,refined_states,abstract_states,simulation_pairs,check_time_ms,memory_usage_bytes,memory_usage_kb" << std::endl;
    }
    
    // Append statistics to CSV file
    void append_to_csv(std::ofstream& file, const std::string& model_name) const {
        file << model_name << ","
             << refined_states << ","
             << abstract_states << ","
             << simulation_pairs << ","
             << check_time_ms << ","
             << memory_usage_bytes << ","
             << (memory_usage_bytes / 1024.0) << std::endl;
    }
    //overload << operator for easy printing
    friend std::ostream& operator<<(std::ostream& os, const CheckStatistics& stats) {
        os << "RTWBS Check Statistics:" << std::endl;
        os << "  Refined States: " << stats.refined_states << std::endl;
        os << "  Abstract States: " << stats.abstract_states << std::endl;
        os << "  Simulation Pairs: " << stats.simulation_pairs << std::endl ;
        os << "  Check Time: " << stats.check_time_ms << " ms" <<   std::endl;  
        os << "  Memory Usage: " << stats.memory_usage_bytes / 1024 << " KB" << std::endl;
        return os;   
    } 
};


/**
 * RTWBS (Relaxed Weak Timed Bisimulation) Equivalence Checker
 * 
 * Based on the ICSE_DT paper, this implements the refinement check for
 * Relaxed Weak Timed Bisimulation which allows:
 * - Relaxed timing constraints on received events (can be delayed)
 * - Strict timing constraints on sent events (must respect original bounds)
 * - Weak bisimulation for event transitions (abstracts tau-transitions)
 */

struct EventTransition {
    int from_state;
    int to_state;
    std::string event;
    bool is_sent;  // true for sent events, false for received events
    int time_bound; // maximum time bound for this event
    
    EventTransition(int from, int to, const std::string& evt, bool sent, int bound)
        : from_state(from), to_state(to), event(evt), is_sent(sent), time_bound(bound) {}
};

struct StateCorrespondence {
    int refined_state;
    int abstract_state;
    
    StateCorrespondence(int ref, int abs) : refined_state(ref), abstract_state(abs) {}
    
    bool operator==(const StateCorrespondence& other) const {
        return refined_state == other.refined_state && abstract_state == other.abstract_state;
    }
};

// Hash function for StateCorrespondence
struct StateCorrespondenceHash {
    std::size_t operator()(const StateCorrespondence& sc) const {
        return std::hash<int>()(sc.refined_state) ^ (std::hash<int>()(sc.abstract_state) << 1);
    }
};

class RTWBSChecker {
public:
    RTWBSChecker() : last_stats_{0,0,0,0.0,0} {}

    // Core equivalence check (game-based relaxed weak timed bisimulation)
    bool check_rtwbs_equivalence(const TimedAutomaton& refined, const TimedAutomaton& abstract);

    // System-level convenience (pairwise index matching)
    bool check_rtwbs_equivalence(const System& system_refined, const System& system_abstract);

    struct SystemCheckResult {
        size_t automaton_index;
        std::string template_name_refined;
        std::string template_name_abstract;
        bool is_equivalent;
        CheckStatistics statistics;
    };
    bool check_rtwbs_equivalence_detailed(const System& system_refined,
                                          const System& system_abstract,
                                          std::vector<SystemCheckResult>& results);

    CheckStatistics get_last_check_statistics() const { return last_stats_; }
    void print_statistics() const { last_stats_.print(); }
    void reset() { last_stats_ = CheckStatistics{0,0,0,0.0,0}; }

private:
    mutable CheckStatistics last_stats_;

    // ===== Optimisation Data Structures =====
    // Cache of tau-closures: ZoneState* -> vector of reachable ZoneState* using only tau/internal transitions
    std::unordered_map<const ZoneState*, std::vector<const ZoneState*>> tau_closure_cache_;

    // Key for weak successor cache (zone state + action label)
    struct WeakKey {
        const ZoneState* z;
        std::string action;
        bool operator==(const WeakKey& o) const noexcept { return z==o.z && action==o.action; }
    };
    struct WeakKeyHash {
        size_t operator()(const WeakKey& k) const noexcept {
            return std::hash<const ZoneState*>{}(k.z) ^ (std::hash<std::string>{}(k.action) << 1);
        }
    };
    // Cache: weak successors under pattern tau* a tau*
    std::unordered_map<WeakKey, std::vector<const ZoneState*>, WeakKeyHash> weak_succ_cache_;

    // Pair key used for relation + reverse dependency graph
    struct PairKey {
        const ZoneState* r; // refined
        const ZoneState* a; // abstract
        bool operator==(const PairKey& o) const noexcept { return r==o.r && a==o.a; }
    };
    struct PairKeyHash {
        size_t operator()(const PairKey& p) const noexcept {
            return std::hash<const ZoneState*>{}(p.r) ^ (std::hash<const ZoneState*>{}(p.a) << 1);
        }
    };

    // Reverse dependency graph: child -> parents that relied on (child) to justify a match
    std::unordered_map<PairKey, std::vector<PairKey>, PairKeyHash> reverse_deps_;

    // Relation set (active candidate pairs)
    std::unordered_set<PairKey, PairKeyHash> relation_;

    // Worklist for localised re-validation
    std::queue<PairKey> worklist_;

    // ---- Cached semantic helpers ----
    const std::vector<const ZoneState*>& tau_closure_cached(const TimedAutomaton& ta, const ZoneState* start);
    const std::vector<const ZoneState*>& weak_observable_successors_cached(const TimedAutomaton& ta, const ZoneState* start, const std::string& action);

    // Reset caches & structures between top-level checks
    void clear_optimisation_state();
};

} // namespace rtwbs

#endif // RTWBS_CORE_H
