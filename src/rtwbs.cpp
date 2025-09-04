#include "rtwbs.h"

#include <chrono>
#include <algorithm>
#include <sstream>
#include <iostream>

namespace rtwbs {

// Helper function to compute weak transitions: τ* → action → τ*
std::vector<std::pair<const ZoneState*, const Transition*>> compute_weak_successors(
    const TimedAutomaton& automaton, 
    const ZoneState* current_zone, 
    const std::string& action) {
    
    std::vector<std::pair<const ZoneState*, const Transition*>> weak_successors;
    std::queue<const ZoneState*> to_explore;
    std::unordered_set<const ZoneState*> visited;
    
    to_explore.push(current_zone);
    visited.insert(current_zone);
    
    // First, follow all τ-transitions (internal transitions)
    std::vector<const ZoneState*> tau_reachable;
    while (!to_explore.empty()) {
        const ZoneState* zone = to_explore.front();
        to_explore.pop();
        tau_reachable.push_back(zone);
        
        auto outgoing_transitions = automaton.get_outgoing_transitions(zone->location_id);
        for (const auto& transition : outgoing_transitions) {
            if (transition->action == TA_CONFIG.tau_action_name || (!transition->has_synchronization() && transition->action.empty())) {
                // This is a τ-transition, compute successor
                auto zone_after_time = automaton.time_elapse(zone->zone);
                if (automaton.is_transition_enabled(zone_after_time, *transition)) {
                    auto successor_zone = automaton.apply_transition(zone_after_time, *transition);
                    successor_zone = automaton.apply_invariants(successor_zone, transition->to_location);
                    
                    // Find this successor in the zone graph (simplified - should be optimized)
                    ZoneState temp_successor(transition->to_location, successor_zone, automaton.get_dimension());
                    for (const auto& existing_zone : automaton.get_all_zone_states()) {
                        if (temp_successor == *existing_zone && visited.find(existing_zone.get()) == visited.end()) {
                            to_explore.push(existing_zone.get());
                            visited.insert(existing_zone.get());
                            break;
                        }
                    }
                }
            }
        }
    }
    
    // Now from each τ-reachable zone, try to take the desired action
    for (const ZoneState* tau_zone : tau_reachable) {
        auto outgoing_transitions = automaton.get_outgoing_transitions(tau_zone->location_id);
        for (const auto& transition : outgoing_transitions) {
            if (transition->action == action) {
                // Found the action transition, now compute successor and follow τ-transitions
                auto zone_after_time = automaton.time_elapse(tau_zone->zone);
                if (automaton.is_transition_enabled(zone_after_time, *transition)) {
                    auto successor_zone = automaton.apply_transition(zone_after_time, *transition);
                    successor_zone = automaton.apply_invariants(successor_zone, transition->to_location);
                    
                    // Create temporary zone state
                    ZoneState temp_successor(transition->to_location, successor_zone, automaton.get_dimension());
                    
                    // Follow τ-transitions after the action
                    std::queue<std::pair<const ZoneState*, const Transition*>> action_successors;
                    std::unordered_set<const ZoneState*> action_visited;
                    
                    // Find this successor in the zone graph and add it
                    for (const auto& existing_zone : automaton.get_all_zone_states()) {
                        if (temp_successor == *existing_zone) {
                            action_successors.push({existing_zone.get(), transition});
                            action_visited.insert(existing_zone.get());
                            break;
                        }
                    }
                    
                    // Now follow τ-transitions from action successors
                    while (!action_successors.empty()) {
                        auto [current_action_zone, original_transition] = action_successors.front();
                        action_successors.pop();
                        weak_successors.push_back({current_action_zone, original_transition});
                        
                        auto action_outgoing = automaton.get_outgoing_transitions(current_action_zone->location_id);
                        for (const auto& tau_trans : action_outgoing) {
                            if (tau_trans->action == TA_CONFIG.tau_action_name || (!tau_trans->has_synchronization() && tau_trans->action.empty())) {
                                auto tau_zone_after_time = automaton.time_elapse(current_action_zone->zone);
                                if (automaton.is_transition_enabled(tau_zone_after_time, *tau_trans)) {
                                    auto tau_successor_zone = automaton.apply_transition(tau_zone_after_time, *tau_trans);
                                    tau_successor_zone = automaton.apply_invariants(tau_successor_zone, tau_trans->to_location);
                                    
                                    ZoneState temp_tau_successor(tau_trans->to_location, tau_successor_zone, automaton.get_dimension());
                                    for (const auto& existing_zone : automaton.get_all_zone_states()) {
                                        if (temp_tau_successor == *existing_zone && action_visited.find(existing_zone.get()) == action_visited.end()) {
                                            action_successors.push({existing_zone.get(), original_transition});
                                            action_visited.insert(existing_zone.get());
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    return weak_successors;
}



bool RTWBSChecker::check_transition_simulation(std::vector<const Transition*> refined, std::vector<const Transition*> abstract){

        for (auto retined_transition : refined){
            bool found = false;
            for (auto abstract_transition : abstract){
               // Add logic here to check if abstract_transition can simulate retined_transition
               if (retined_transition->is_included(abstract_transition)) {
                   found = true;
                   break;
               }
            }
            if (!found){
                std::cout << "No matching abstract transition for refined transition " << retined_transition->action << std::endl;
                return false;
            }
        }
        
        return true; // All refined transitions have matching abstract transitions
}

//                    //Condition 2: RTWBS
//                    if (refined_transition->has_synchronization()){
//                        //2.1 i
//                        if (!abstract_transition->has_synchronization())
//                            continue;
//                        // Condition 2: If its a sent event (ending with !), then the time remain strict, lower equal
//                        if (refined_transition->is_sender && !refined_transition->is_included(abstract_transition)){ 
//                            continue;
//
//                        }
//                        // Condition 3: If its a received event (ending with ?) then the time must be a super set
//                        if (refined_transition->is_receiver && !refined_transition->includes(abstract_transition)){ 
//                            continue;
//
//                        }
//
//                    }
                    
bool RTWBSChecker::check_rtwbs_equivalence(const TimedAutomaton& refined, 
                                          const TimedAutomaton& abstract) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Reset statistics
    last_stats_ = CheckStatistics{0, 0, 0, 0.0, 0};
    refined.construct_zone_graph();
    abstract.construct_zone_graph();
    
    const auto& refined_zones = refined.get_all_zone_states();
    const auto& abstract_zones = abstract.get_all_zone_states();

    // Store pairs of compatible zones: refined -> abstract
    //std::unordered_map<size_t, std::vector<const ZoneState*>> compatible_zones;
    std::unordered_map<size_t, bool> compatible_zones;
    std::unordered_set<std::pair<const ZoneState*, const ZoneState*>,PairHash> R;
    int i=0;
    for (const auto& ref_zone : refined_zones) {
        compatible_zones[ref_zone->hash_value] = false;
        for (const auto& abs_zone : abstract_zones) {
            // For RTWBS, zones from same location are always potentially compatible
            // The actual RTWBS constraints will be checked during transition simulation
            if (ref_zone->location_id == abs_zone->location_id) {
                compatible_zones[ref_zone->hash_value] = true;
                R.insert({ref_zone.get(), abs_zone.get()});
            }
        }
        i++;
    }
    
    // Now check if all refined zones have at least one compatible abstract zone
    for (const auto& [hash, is_compatible] : compatible_zones) {
        if (!is_compatible) {
            std::cout << "Not all refined zones have compatible abstract zones" << std::endl;
            return false;
        }
    }

    std::cout << "Proceeding with the simulation check ^\n";

    
    // Iteratively refine the simulation relation R until fixed point
    while (true) {
        std::unordered_set<std::pair<const ZoneState*, const ZoneState*>, PairHash> R_prime;
        
        // For each pair currently in the relation
        for (const auto& [q_refined, q_abstract] : R) {
            bool is_pair_good = true;
            
            // Get outgoing transitions for both zones
            auto moves_refined = refined.get_outgoing_transitions(q_refined->location_id);
            auto moves_abstract = abstract.get_outgoing_transitions(q_abstract->location_id);
            
            // For EVERY move the refined automaton (spoiler) makes
            for (const auto& refined_transition : moves_refined) {
                bool found_matching_move = false;
                
                // Skip internal τ-transitions - they will be handled by weak transition relation
                if (refined_transition->action == TA_CONFIG.tau_action_name || (!refined_transition->has_synchronization() && refined_transition->action.empty())) {
                    continue;
                }
                
                // Compute weak successors for this action from current refined zone
                auto weak_refined_successors = compute_weak_successors(refined, q_refined, refined_transition->action);
                
                // The abstract automaton (duplicator) must have AT LEAST ONE valid response
                for (const auto& abstract_transition : moves_abstract) {
                    
                    // Condition 1: Action compatibility
                    if (refined_transition->action != abstract_transition->action) {
                        continue; // This move doesn't work, try the next one
                    }
                    
                    // Skip internal τ-transitions for abstract as well
                    if (abstract_transition->action == TA_CONFIG.tau_action_name || (!abstract_transition->has_synchronization() && abstract_transition->action.empty())) {
                        continue;
                    }
                    
                    // Compute weak successors for this action from current abstract zone  
                    auto weak_abstract_successors = compute_weak_successors(abstract, q_abstract, abstract_transition->action);
                    // RTWBS timing constraints check
                    bool timing_compatible = false;
                    
                    if (refined_transition->has_synchronization() && abstract_transition->has_synchronization()) {
                        // Both are synchronization transitions
                        if (refined_transition->channel == abstract_transition->channel) {
                            if (refined_transition->is_sender && abstract_transition->is_sender) {
                                // Sent events: refined timing must be ≤ abstract timing (strict)
                                timing_compatible = refined_transition->is_included(abstract_transition);
                            } else if (refined_transition->is_receiver && abstract_transition->is_receiver) {
                                // Received events: refined timing must be ≥ abstract timing (relaxed)
                                timing_compatible = refined_transition->includes(abstract_transition);
                            }
                        }
                    } else if (!refined_transition->has_synchronization() && !abstract_transition->has_synchronization()) {
                        // Both are internal transitions - use standard inclusion
                        timing_compatible = refined_transition->is_included(abstract_transition);
                    }
                    
                    if (!timing_compatible) {
                        continue; // This abstract transition doesn't satisfy RTWBS constraints
                    }
                    
                    // Check if weak successors are compatible
                    bool weak_successors_compatible = false;
                    
                    // For each weak refined successor, check if there exists a corresponding weak abstract successor
                    for (const auto& [weak_ref_zone, weak_ref_transition] : weak_refined_successors) {
                        for (const auto& [weak_abs_zone, weak_abs_transition] : weak_abstract_successors) {
                            // Check if this weak successor pair is in current relation R
                            if (R.find({weak_ref_zone, weak_abs_zone}) != R.end()) {
                                weak_successors_compatible = true;
                                break;
                            }
                        }
                        if (weak_successors_compatible) break;
                    }
                    
                    if (weak_successors_compatible) {
                        found_matching_move = true;
                        break; // Found a valid abstract response
                    }
                }
                
                if (!found_matching_move) {
                    // If abstract has no response to this refined move,
                    // the pair is not in the simulation
                    is_pair_good = false;
                    break; // No need to check other refined moves
                }
            }
            
            // If abstract could counter all refined moves, keep the pair
            if (is_pair_good) {
                R_prime.insert({q_refined, q_abstract});
            }
        }
        
        std::cout << "Refined relation R' has " << R_prime.size() << " pairs" << std::endl;
        
        // If relation did not shrink, we have reached the greatest fixed point
        if (R == R_prime) {
            break;
        }
        
        // Otherwise, update R and continue refining
        R = R_prime;
    }
    
    // Check if all initial states of refined are simulated by some initial state of abstract
    // For now, assuming single initial state at location 0
    bool simulation_holds = false;
    for (const auto& [ref_zone, abs_zone] : R) {
        if (ref_zone->location_id == 0 && abs_zone->location_id == 0) {
            simulation_holds = true;
            break;
        }
    }
    
    std::cout << "Final simulation relation has " << R.size() << " pairs" << std::endl;
    std::cout << "RTWBS simulation " << (simulation_holds ? "HOLDS" : "DOES NOT HOLD") << std::endl;
    
    // Update statistics
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    last_stats_.refined_states = refined_zones.size();
    last_stats_.abstract_states = abstract_zones.size();
    last_stats_.simulation_pairs = R.size();
    last_stats_.check_time_ms = duration.count();
    last_stats_.memory_usage_bytes = R.size() * sizeof(std::pair<const ZoneState*, const ZoneState*>) +
                                   compatible_zones.size() * (sizeof(size_t) + sizeof(bool));
    
    return simulation_holds;
}

bool RTWBSChecker::check_rtwbs_with_counterexample(const TimedAutomaton& refined, 
                                                   const TimedAutomaton& abstract,
                                                   std::vector<EventTransition>& counterexample) {
    counterexample.clear();
    
    if (check_rtwbs_equivalence(refined, abstract)) {
        return true;
    }
    
    /*// If check failed, try to construct a counterexample
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
    }*/
    
    return false;
}

void RTWBSChecker::clear_caches() {
    path_cache_.clear();
    correspondence_cache_.clear();
}

RTWBSChecker::CheckStatistics RTWBSChecker::get_last_check_statistics() const {
    return last_stats_;
}

bool RTWBSChecker::check_rtwbs_equivalence(const System& system_refined, const System& system_abstract) {
    // Systems must have the same number of automata
    if (system_refined.size() != system_abstract.size()) {
        std::cout << "Systems have different number of automata: " 
                  << system_refined.size() << " vs " << system_abstract.size() << std::endl;
        return false;
    }
    
    if (system_refined.size() == 0) {
        std::cout << "Both systems are empty - considered equivalent" << std::endl;
        return true;
    }
    
    std::cout << "=== System-Level RTWBS Checking ===" << std::endl;
    std::cout << "Checking " << system_refined.size() << " automaton pairs..." << std::endl;
    
    // Check each corresponding automaton pair
    bool all_equivalent = true;
    for (size_t i = 0; i < system_refined.size(); ++i) {
        const auto& refined_automaton = system_refined.get_automaton(i);
        const auto& abstract_automaton = system_abstract.get_automaton(i);
        
        const auto& refined_names = system_refined.get_template_names();
        const auto& abstract_names = system_abstract.get_template_names();
        
        std::cout << "--- Checking Pair " << i << ": " 
                  << refined_names[i] << " ≼ " << abstract_names[i] << " ---" << std::endl;
        
        bool pair_equivalent = check_rtwbs_equivalence(refined_automaton, abstract_automaton);
        
        std::cout << "Result: " << (pair_equivalent ? "EQUIVALENT" : "NOT EQUIVALENT") << std::endl;
        
        if (!pair_equivalent) {
            all_equivalent = false;
            // For now, continue checking all pairs instead of early termination
            // if (enable_early_termination) {
            //     std::cout << "Early termination enabled - stopping on first counterexample" << std::endl;
            //     break;
            // }
        }
        std::cout << std::endl;
    }
    
    std::cout << "=== System-Level Result: " << (all_equivalent ? "EQUIVALENT" : "NOT EQUIVALENT") << " ===" << std::endl;
    return all_equivalent;
}

bool RTWBSChecker::check_rtwbs_equivalence_detailed(const System& system_refined, 
                                                   const System& system_abstract,
                                                   std::vector<SystemCheckResult>& results) {
    results.clear();
    
    // Systems must have the same number of automata
    if (system_refined.size() != system_abstract.size()) {
        std::cout << "Systems have different number of automata: " 
                  << system_refined.size() << " vs " << system_abstract.size() << std::endl;
        return false;
    }
    
    if (system_refined.size() == 0) {
        std::cout << "Both systems are empty - considered equivalent" << std::endl;
        return true;
    }
    
    std::cout << "=== Detailed System-Level RTWBS Checking ===" << std::endl;
    std::cout << "Checking " << system_refined.size() << " automaton pairs..." << std::endl;
    
    bool all_equivalent = true;
    const auto& refined_names = system_refined.get_template_names();
    const auto& abstract_names = system_abstract.get_template_names();
    
    for (size_t i = 0; i < system_refined.size(); ++i) {
        SystemCheckResult result;
        result.automaton_index = i;
        result.template_name_refined = refined_names[i];
        result.template_name_abstract = abstract_names[i];
        
        const auto& refined_automaton = system_refined.get_automaton(i);
        const auto& abstract_automaton = system_abstract.get_automaton(i);
        
        std::cout << "--- Checking Pair " << i << ": " 
                  << result.template_name_refined << " ≼ " << result.template_name_abstract << " ---" << std::endl;
        
        // Check with counterexample collection
        result.is_equivalent = check_rtwbs_with_counterexample(
            refined_automaton, abstract_automaton, result.counterexample);
        
        result.statistics = get_last_check_statistics();
        
        std::cout << "Result: " << (result.is_equivalent ? "EQUIVALENT" : "NOT EQUIVALENT") << std::endl;
        
        if (!result.is_equivalent) {
            all_equivalent = false;
            if (!result.counterexample.empty()) {
                std::cout << "Counterexample found for this pair." << std::endl;
            }
        }
        
        results.push_back(result);
        std::cout << std::endl;
        
        // For now, continue checking all pairs instead of early termination
        // if (!result.is_equivalent && enable_early_termination) {
        //     std::cout << "Early termination enabled - stopping on first counterexample" << std::endl;
        //     break;
        // }
    }
    
    std::cout << "=== Detailed System-Level Result: " << (all_equivalent ? "EQUIVALENT" : "NOT EQUIVALENT") << " ===" << std::endl;
    return all_equivalent;
}

} // namespace rtwbs
