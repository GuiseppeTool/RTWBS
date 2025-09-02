#ifndef RTWBS_H
#define RTWBS_H

#include "timedautomaton.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <queue>
#include <functional>

namespace rtwbs {

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
private:
    // Cache for memoization (optimization from paper)
    std::unordered_map<std::string, bool> path_cache_;
    std::unordered_set<StateCorrespondence, StateCorrespondenceHash> correspondence_cache_;
    
    // Extract event transitions from a timed automaton
    std::vector<EventTransition> extract_event_transitions(const TimedAutomaton& automaton);
    
    // Check if a path exists in the abstract system that corresponds to the refined path
    bool find_equivalent_path(const std::vector<EventTransition>& refined_path,
                             const std::vector<EventTransition>& abstract_transitions);
    
    // Verify state correspondence under RWTBS rules
    bool verify_state_correspondence(int refined_state, int abstract_state,
                                   const TimedAutomaton& refined, const TimedAutomaton& abstract);
    
    // Check timing constraints: strict for sent events, relaxed for received events
    bool check_timing_constraints(const EventTransition& refined_trans, 
                                 const EventTransition& abstract_trans);
    
    // Generate hash key for path caching
    std::string generate_path_key(const std::vector<EventTransition>& path);

public:
    /**
     * Check if refined automaton is RTWBS-equivalent to abstract automaton
     * 
     * @param refined The refined/concrete timed automaton (e.g., DT)
     * @param abstract The abstract timed automaton (e.g., PT)
     * @return true if refined is a valid RTWBS refinement of abstract
     */
    bool check_rtwbs_equivalence(const TimedAutomaton& refined, const TimedAutomaton& abstract);
    
    /**
     * Get detailed refinement result with counterexample if check fails
     * 
     * @param refined The refined timed automaton
     * @param abstract The abstract timed automaton
     * @param counterexample Output parameter for counterexample trace if check fails
     * @return true if equivalent, false with counterexample if not
     */
    bool check_rtwbs_with_counterexample(const TimedAutomaton& refined, 
                                        const TimedAutomaton& abstract,
                                        std::vector<EventTransition>& counterexample);
    
    /**
     * Clear internal caches (useful for memory management)
     */
    void clear_caches();
    
    /**
     * Get statistics about the last refinement check
     */
    struct CheckStatistics {
        size_t paths_checked;
        size_t cache_hits;
        size_t correspondences_verified;
        double check_time_ms;
    };
    
    CheckStatistics get_last_check_statistics() const;

private:
    mutable CheckStatistics last_stats_;
};

} // namespace rtwbs

#endif // RTWBS_H
