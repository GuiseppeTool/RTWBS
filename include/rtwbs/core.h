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
private:
    // Cache for memoization (optimization from paper)
    std::unordered_map<std::string, bool> path_cache_;
    std::unordered_set<StateCorrespondence, StateCorrespondenceHash> correspondence_cache_;
    
   /* // Extract event transitions from a timed automaton
    std::vector<EventTransition> extract_event_transitions(const TimedAutomaton& automaton);
    
    // Check if a path exists in the abstract system that corresponds to the refined path
    bool find_equivalent_path(const std::vector<EventTransition>& refined_path,
                             const std::vector<EventTransition>& abstract_transitions){};
    
    // Verify state correspondence under RWTBS rules
    bool verify_state_correspondence(int refined_state, int abstract_state,
                                   const TimedAutomaton& refined, const TimedAutomaton& abstract){};
    
    // Check timing constraints: strict for sent events, relaxed for received events
    bool check_timing_constraints(const EventTransition& refined_trans, 
                                 const EventTransition& abstract_trans){};
    
    // Generate hash key for path caching
    std::string generate_path_key(const std::vector<EventTransition>& path){};

     // Check if two zone states satisfy RTWBS inclusion constraints
     // @param refined_state_id: State ID in refined automaton
     // @param abstract_state_id: State ID in abstract automaton  
     // @param abstract_automaton: The abstract automaton to compare against
     // @return: true if refined zone can simulate abstract zone under RTWBS
        bool check_zone_inclusion_rtwbs(size_t refined_state_id, size_t abstract_state_id, 
                                   const TimedAutomaton& refined_automaton,const TimedAutomaton& abstract_automaton) const{};
*/
public:
    RTWBSChecker()
    {
        path_cache_.clear();
        correspondence_cache_.clear();
        last_stats_=CheckStatistics{0, 0, 0, 0.0, 0};
    }
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
    



    bool check_transition_simulation(std::vector<const Transition*> refined, std::vector<const Transition*> abstract);
    
    CheckStatistics get_last_check_statistics() const;
    void print_statistics() const
    {
        last_stats_.print();
    }

    /**
     * Check RTWBS equivalence between two systems
     * 
     * This checks if each automaton in system_refined is RTWBS-equivalent 
     * to the corresponding automaton in system_abstract (matched by index).
     * 
     * @param system_refined The refined/concrete system
     * @param system_abstract The abstract system
     * @return true if all corresponding automata pairs are RTWBS-equivalent
     */
    bool check_rtwbs_equivalence(const System& system_refined, const System& system_abstract);
    
    /**
     * Check RTWBS equivalence between two systems with detailed results
     * 
     * @param system_refined The refined/concrete system
     * @param system_abstract The abstract system
     * @param results Output vector containing results for each automaton pair
     * @return true if all corresponding automata pairs are RTWBS-equivalent
     */
    struct SystemCheckResult {
        size_t automaton_index;
        std::string template_name_refined;
        std::string template_name_abstract;
        bool is_equivalent;
        CheckStatistics statistics;
        std::vector<EventTransition> counterexample;
    };
    
    bool check_rtwbs_equivalence_detailed(const System& system_refined, 
                                         const System& system_abstract,
                                         std::vector<SystemCheckResult>& results);
    void reset()
    {
        last_stats_=CheckStatistics{0, 0, 0, 0.0, 0};
         path_cache_.clear();
        correspondence_cache_.clear();
        
    }

private:
    mutable CheckStatistics last_stats_;
};

} // namespace rtwbs

#endif // RTWBS_CORE_H
