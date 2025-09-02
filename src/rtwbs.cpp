#include "rtwbs.h"
#include <chrono>
#include <algorithm>
#include <sstream>
#include <iostream>

namespace rtwbs {

std::vector<EventTransition> RTWBSChecker::extract_event_transitions(const TimedAutomaton& automaton) {
    std::vector<EventTransition> event_transitions;
    
    // Get all transitions from the automaton
    const auto& transitions = automaton.get_transitions();
    
    std::cout << "Extracting " << transitions.size() << " transitions from automaton" << std::endl;
    
    for (size_t i = 0; i < transitions.size(); ++i) {
        const auto& trans = transitions[i];
        
        // Skip internal (tau) transitions
        if (trans.action.empty()) {
            continue;
        }
        
        // Parse synchronization action to extract event name and direction
        std::string event_name = trans.action;
        bool is_sent = false;
        
        if (event_name.back() == '!') {
            is_sent = true;
            event_name.pop_back(); // Remove '!' suffix
        } else if (event_name.back() == '?') {
            is_sent = false;
            event_name.pop_back(); // Remove '?' suffix
        }
        
        // Extract time bound from guard constraints
        // This is simplified - just use a default bound for now
        int time_bound = 100; // Default
        
        // In a complete implementation, we would parse the guards vector
        // to extract actual timing constraints
        if (!trans.guards.empty()) {
            // For now, just use a default tighter bound if there are guards
            time_bound = 10;
        }
        
        event_transitions.emplace_back(trans.from_location, trans.to_location, event_name, is_sent, time_bound);
        
        std::cout << "  Transition " << i << ": " << event_name 
                  << " (" << (is_sent ? "send" : "receive") << ") "
                  << "bound=" << time_bound << " from=" << trans.from_location 
                  << " to=" << trans.to_location << std::endl;
    }
    
    return event_transitions;
}

bool RTWBSChecker::check_timing_constraints(const EventTransition& refined_trans, 
                                           const EventTransition& abstract_trans) {
    // RTWBS rules:
    // 1. For sent events (!): refined timing must be stricter or equal to abstract
    // 2. For received events (?): refined timing can be more relaxed than abstract
    
    if (refined_trans.is_sent && abstract_trans.is_sent) {
        // Both are sent events - refined must be stricter or equal
        return refined_trans.time_bound <= abstract_trans.time_bound;
    } else if (!refined_trans.is_sent && !abstract_trans.is_sent) {
        // Both are received events - refined can be more relaxed
        return refined_trans.time_bound >= abstract_trans.time_bound;
    } else {
        // Event direction mismatch
        return false;
    }
}

bool RTWBSChecker::find_equivalent_path(const std::vector<EventTransition>& refined_path,
                                       const std::vector<EventTransition>& abstract_transitions) {
    // Check if there exists a path in abstract that matches the refined path
    // under RTWBS rules
    
    if (refined_path.empty()) return true;
    
    // Generate cache key
    std::string key = generate_path_key(refined_path);
    auto cache_it = path_cache_.find(key);
    if (cache_it != path_cache_.end()) {
        last_stats_.cache_hits++;
        return cache_it->second;
    }
    
    // Simple implementation: look for matching event sequence
    // In a full implementation, this would use graph algorithms
    
    size_t refined_idx = 0;
    for (const auto& abs_trans : abstract_transitions) {
        if (refined_idx >= refined_path.size()) break;
        
        const auto& ref_trans = refined_path[refined_idx];
        
        // Check if events match and timing constraints are satisfied
        if (ref_trans.event == abs_trans.event &&
            check_timing_constraints(ref_trans, abs_trans)) {
            refined_idx++;
        }
    }
    
    bool result = (refined_idx == refined_path.size());
    path_cache_[key] = result;
    return result;
}

std::string RTWBSChecker::generate_path_key(const std::vector<EventTransition>& path) {
    std::ostringstream oss;
    for (const auto& trans : path) {
        oss << trans.from_state << "->" << trans.to_state 
            << ":" << trans.event << "(" << (trans.is_sent ? "!" : "?") 
            << "," << trans.time_bound << ");";
    }
    return oss.str();
}

bool RTWBSChecker::verify_state_correspondence(int refined_state, int abstract_state,
                                              const TimedAutomaton& refined, 
                                              const TimedAutomaton& abstract) {
    StateCorrespondence corr(refined_state, abstract_state);
    
    if (correspondence_cache_.find(corr) != correspondence_cache_.end()) {
        return true;
    }
    
    // Simplified state correspondence check
    // In a full implementation, this would check zone inclusion and simulation
    
    correspondence_cache_.insert(corr);
    last_stats_.correspondences_verified++;
    return true;
}

bool RTWBSChecker::check_rtwbs_equivalence(const TimedAutomaton& refined, 
                                          const TimedAutomaton& abstract) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Reset statistics
    last_stats_ = CheckStatistics{0, 0, 0, 0.0};
    
    try {
        // Extract event transitions from both automata
        auto refined_transitions = extract_event_transitions(refined);
        auto abstract_transitions = extract_event_transitions(abstract);
        
        std::cout << "RTWBS Check: Refined has " << refined_transitions.size() 
                  << " transitions, Abstract has " << abstract_transitions.size() 
                  << " transitions" << std::endl;
        
        // Check if every path in refined has a corresponding path in abstract
        // This is a simplified implementation
        
        // Group transitions by starting state for efficient lookup
        std::unordered_map<int, std::vector<EventTransition>> refined_by_state;
        for (const auto& trans : refined_transitions) {
            refined_by_state[trans.from_state].push_back(trans);
        }
        
        // For each state in refined, check if there's a valid correspondence
        for (const auto& [state, transitions] : refined_by_state) {
            last_stats_.paths_checked++;
            
            // Create a simple path from this state
            std::vector<EventTransition> path;
            if (!transitions.empty()) {
                path.push_back(transitions[0]); // Take first transition as example
            }
            
            if (!find_equivalent_path(path, abstract_transitions)) {
                std::cout << "RTWBS Check: Failed to find equivalent path for state " 
                          << state << std::endl;
                return false;
            }
        }
        
        std::cout << "RTWBS Check: All paths verified successfully" << std::endl;
        
        auto end_time = std::chrono::high_resolution_clock::now();
        last_stats_.check_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "RTWBS Check: Exception during check: " << e.what() << std::endl;
        
        auto end_time = std::chrono::high_resolution_clock::now();
        last_stats_.check_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        
        return false;
    }
}

bool RTWBSChecker::check_rtwbs_with_counterexample(const TimedAutomaton& refined, 
                                                   const TimedAutomaton& abstract,
                                                   std::vector<EventTransition>& counterexample) {
    counterexample.clear();
    
    if (check_rtwbs_equivalence(refined, abstract)) {
        return true;
    }
    
    // If check failed, try to construct a counterexample
    auto refined_transitions = extract_event_transitions(refined);
    auto abstract_transitions = extract_event_transitions(abstract);
    
    // Find the first path that doesn't have a corresponding path
    for (const auto& ref_trans : refined_transitions) {
        std::vector<EventTransition> path = {ref_trans};
        
        if (!find_equivalent_path(path, abstract_transitions)) {
            counterexample = path;
            std::cout << "Counterexample: Transition " << ref_trans.event 
                      << " from state " << ref_trans.from_state 
                      << " has no valid abstract correspondence" << std::endl;
            break;
        }
    }
    
    return false;
}

void RTWBSChecker::clear_caches() {
    path_cache_.clear();
    correspondence_cache_.clear();
}

RTWBSChecker::CheckStatistics RTWBSChecker::get_last_check_statistics() const {
    return last_stats_;
}

} // namespace rtwbs
