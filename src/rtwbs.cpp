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
        int32_t time_bound = dbm_INFINITY; // Default bound if no constraints
        
        // Parse actual guard constraints to extract timing bounds
        for (const auto& guard : trans.guards) {
            // Convert raw constraint back to bound value
            int32_t bound_value = dbm_raw2bound(guard.value);
            
            // For RTWBS, we're interested in upper bounds (constraints like x <= c)
            // Guard i=0, j=clock means "0 - x_clock <= bound" i.e., "x_clock >= -bound"
            // Guard i=clock, j=0 means "x_clock - 0 <= bound" i.e., "x_clock <= bound"
            if (guard.i != 0 && guard.j == 0) {
                // This is an upper bound constraint x_i <= bound_value
                time_bound = std::min(time_bound, bound_value);
            }
            // We could also handle lower bounds (guard.i == 0, guard.j != 0) if needed
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
    // Implement the RWTBS algorithm from the ICSE_DT paper
    // 
    // The algorithm checks if for every transition in the refined path,
    // there exists a corresponding weak transition in the abstract system
    // that satisfies RWTBS timing constraints.
    
    if (refined_path.empty()) return true;
    
    // Generate cache key
    std::string key = generate_path_key(refined_path);
    auto cache_it = path_cache_.find(key);
    if (cache_it != path_cache_.end()) {
        last_stats_.cache_hits++;
        return cache_it->second;
    }
    
    // RWTBS Algorithm Implementation:
    // 
    // Note: The systematic state space exploration is implemented in 
    // TimedAutomaton::construct_zone_graph(), which uses BFS to build 
    // the reachable zone graph. This RWTBS checker operates on the 
    // transitions extracted from that zone graph.
    //
    // For each transition t_r in refined_path:
    //   Find a corresponding transition t_a in abstract_transitions where:
    //   1. Event names match
    //   2. RWTBS timing constraints are satisfied:
    //      - For received events (?): I_C(μ) ⊆ I_C'(μ) (refined can be more relaxed)
    //      - For sent events (!): I_C(μ) = I_C'(μ) (refined must be same or stricter)
    
    std::vector<bool> refined_matched(refined_path.size(), false);
    
    // For each refined transition, try to find a matching abstract transition
    for (size_t r_idx = 0; r_idx < refined_path.size(); ++r_idx) {
        const auto& ref_trans = refined_path[r_idx];
        
        // Look for corresponding abstract transition
        for (const auto& abs_trans : abstract_transitions) {
            // Check event name match
            if (ref_trans.event != abs_trans.event) continue;
            
            // Check RWTBS timing constraints
            bool timing_valid = false;
            
            if (ref_trans.is_sent && abs_trans.is_sent) {
                // Both sent events: refined must be stricter or equal (I_C(μ) = I_C'(μ))
                // This means time_bound(refined) <= time_bound(abstract)
                timing_valid = (ref_trans.time_bound <= abs_trans.time_bound);
            } else if (!ref_trans.is_sent && !abs_trans.is_sent) {
                // Both received events: refined can be more relaxed (I_C(μ) ⊆ I_C'(μ))
                // This means time_bound(refined) >= time_bound(abstract)
                timing_valid = (ref_trans.time_bound >= abs_trans.time_bound);
            } else {
                // Direction mismatch - not valid under RWTBS
                timing_valid = false;
            }
            
            if (timing_valid) {
                refined_matched[r_idx] = true;
                break; // Found a match for this refined transition
            }
        }
    }
    
    // Check if all refined transitions found valid abstract counterparts
    bool result = std::all_of(refined_matched.begin(), refined_matched.end(), 
                             [](bool matched) { return matched; });
    
    // Debug output for failed matches
    if (!result) {
        std::cout << "RWTBS: Failed to match refined transitions:" << std::endl;
        for (size_t i = 0; i < refined_matched.size(); ++i) {
            if (!refined_matched[i]) {
                const auto& trans = refined_path[i];
                std::cout << "  Unmatched: " << trans.event 
                          << " (" << (trans.is_sent ? "sent" : "received") 
                          << ") bound=" << trans.time_bound << std::endl;
            }
        }
    }
    
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



bool RTWBSChecker::check_zone_inclusion_rtwbs(size_t refined_state_id, size_t abstract_state_id,
                                               const TimedAutomaton& refined_automaton,const TimedAutomaton& abstract_automaton) const {
    // Get zone states
    const ZoneState* refined_state = refined_automaton.get_zone_state(refined_state_id);
    const ZoneState* abstract_state = abstract_automaton.get_zone_state(abstract_state_id);
    
    if (!refined_state || !abstract_state) {
        return false;  // Invalid state IDs
    }
    
    // For RTWBS, we need to check that the refined zone can simulate the abstract zone
    // This means: every timing behavior in the refined zone should have a corresponding
    // behavior in the abstract zone that satisfies RTWBS constraints
    
    // Check location correspondence first
    //if (refined_state->location_id != abstract_state->location_id) {
    //    // Different locations may still correspond in some cases, but for simplicity
    //    // we require same symbolic locations
    //    return false;
    //}
    
    // Check zone inclusion using UDBM
    // For RTWBS: refined zones can be more relaxed than abstract zones
    // This reflects the relaxed timing constraints allowed in RTWBS:
    // - Received events can have more relaxed timing (superset zones)
    // - The key is that behaviors are preserved under timing relaxation
    relation_t relation = dbm_relation(refined_state->zone.data(), 
                                     abstract_state->zone.data(), 
                                     refined_state->dimension);
    
    // Accept multiple zone relations for RTWBS:
    // - EQUAL: exact match (always valid)
    // - SUBSET: refined is stricter (always valid) 
    // - SUPERSET: refined is more relaxed (valid for RTWBS due to timing relaxation)
    // Only reject DIFFERENT (incomparable zones)
    bool zones_compatible = (relation != base_DIFFERENT);
    
    return zones_compatible;
}


bool RTWBSChecker::verify_state_correspondence(int refined_state, int abstract_state,
                                              const TimedAutomaton& refined, 
                                              const TimedAutomaton& abstract) {
    StateCorrespondence corr(refined_state, abstract_state);
    
    if (correspondence_cache_.find(corr) != correspondence_cache_.end()) {
        return true;
    }
    
    // Full state correspondence check for RWTBS
    // 
    // For RWTBS, we need to verify that the refined state can simulate the abstract state
    // while respecting the relaxed timing constraints for received events and 
    // strict timing constraints for sent events.
    
    try {
        // Ensure zone graphs are constructed for both automata
        refined.construct_zone_graph();
        abstract.construct_zone_graph();

        // Full state correspondence check using actual zone states
        // 
        // For the current transition-based analysis, we primarily need to ensure
        // that the symbolic locations correspond and the timing constraints are compatible.
        
        // Step 1: Check if we have corresponding zone states

        const ZoneState* refined_zone_state = refined.get_zone_state(refined_state);
        const ZoneState* abstract_zone_state = abstract.get_zone_state(abstract_state);
        
        bool zones_available = (refined_zone_state && abstract_zone_state);
        bool zone_compatible = true; // Default to compatible if no zones
        
        if (!zones_available) {
            // Zone states are required for proper RTWBS correspondence checking
            // Without zones, we cannot verify the timing simulation relation
            std::cout << "State correspondence failed: zone states not available ("
                      << "refined=" << (refined_zone_state ? "found" : "missing") 
                      << ", abstract=" << (abstract_zone_state ? "found" : "missing") << ")" << std::endl;
            std::cout << "  Cannot perform proper RTWBS check without zone information" << std::endl;
            return false;  // Fail conservatively - no unsafe assumptions
                      
        } else {
            // We have actual zone states - perform zone inclusion check
            zone_compatible = (refined_zone_state->location_id == abstract_zone_state->location_id);
            
            if (zone_compatible && refined_zone_state->dimension == abstract_zone_state->dimension) {
                // Additional zone inclusion check using UDBM
                relation_t relation = dbm_relation(refined_zone_state->zone.data(), 
                                                 abstract_zone_state->zone.data(), 
                                                 refined_zone_state->dimension);
                
                // For RTWBS: accept EQUAL, SUBSET, and SUPERSET relations
                // Only reject DIFFERENT (incomparable zones)
                zone_compatible = (relation != base_DIFFERENT);
                
                std::cout << "Zone correspondence: refined_zone simulates abstract_zone = " 
                          << zone_compatible << " (relation=" << relation << ")" << std::endl;
            }
            
            if (!zone_compatible) {
                std::cout << "State correspondence failed: zones not compatible under RTWBS" << std::endl;
                return false;
            }
        }
        
        // Step 2: Verify outgoing transition compatibility
        // 
        // Check that outgoing transitions from the refined state satisfy RWTBS constraints
        // with respect to abstract transitions. We use the actual location IDs for this.
        
        int refined_location = zones_available ? refined_zone_state->location_id : refined_state;
        int abstract_location = zones_available ? abstract_zone_state->location_id : abstract_state;
        
        // Get outgoing transitions from both states
        const auto& refined_transitions = refined.get_transitions();
        const auto& abstract_transitions = abstract.get_transitions();
        
        // Find transitions originating from these locations
        std::vector<EventTransition> refined_outgoing;
        std::vector<EventTransition> abstract_outgoing;
        
        for (size_t i = 0; i < refined_transitions.size(); ++i) {
            const auto& trans = refined_transitions[i];
            if (trans.from_location == refined_state && !trans.action.empty()) {
                // Extract event transition info
                std::string event_name = trans.action;
                bool is_sent = false;
                
                if (event_name.back() == '!') {
                    is_sent = true;
                    event_name.pop_back();
                } else if (event_name.back() == '?') {
                    is_sent = false;
                    event_name.pop_back();
                }
                
                // Extract timing bounds
                int32_t time_bound = dbm_INFINITY;
                for (const auto& guard : trans.guards) {
                    int32_t bound_value = dbm_raw2bound(guard.value);
                    if (guard.i != 0 && guard.j == 0) {
                        time_bound = std::min(time_bound, bound_value);
                    }
                }
                
                refined_outgoing.emplace_back(trans.from_location, trans.to_location, 
                                            event_name, is_sent, time_bound);
            }
        }
        
        for (size_t i = 0; i < abstract_transitions.size(); ++i) {
            const auto& trans = abstract_transitions[i];
            if (trans.from_location == abstract_state && !trans.action.empty()) {
                // Extract event transition info
                std::string event_name = trans.action;
                bool is_sent = false;
                
                if (event_name.back() == '!') {
                    is_sent = true;
                    event_name.pop_back();
                } else if (event_name.back() == '?') {
                    is_sent = false;
                    event_name.pop_back();
                }
                
                // Extract timing bounds
                int32_t time_bound = dbm_INFINITY;
                for (const auto& guard : trans.guards) {
                    int32_t bound_value = dbm_raw2bound(guard.value);
                    if (guard.i != 0 && guard.j == 0) {
                        time_bound = std::min(time_bound, bound_value);
                    }
                }
                
                abstract_outgoing.emplace_back(trans.from_location, trans.to_location, 
                                             event_name, is_sent, time_bound);
            }
        }
        
        // Step 4: Verify RWTBS simulation relation for outgoing transitions
        // 
        // For each refined outgoing transition, there must exist a corresponding
        // abstract transition that satisfies RWTBS timing constraints
        
        for (const auto& ref_trans : refined_outgoing) {
            bool found_corresponding_abstract = false;
            
            for (const auto& abs_trans : abstract_outgoing) {
                // Check event name match
                if (ref_trans.event != abs_trans.event) continue;
                
                // Check direction match
                if (ref_trans.is_sent != abs_trans.is_sent) continue;
                
                // Check RWTBS timing constraints
                bool timing_valid = false;
                
                if (ref_trans.is_sent) {
                    // Sent events: refined must be same or stricter
                    timing_valid = (ref_trans.time_bound <= abs_trans.time_bound);
                } else {
                    // Received events: refined can be more relaxed
                    timing_valid = (ref_trans.time_bound >= abs_trans.time_bound);
                }
                
                if (timing_valid) {
                    found_corresponding_abstract = true;
                    break;
                }
            }
            
            if (!found_corresponding_abstract) {
                std::cout << "State correspondence failed: refined transition '" 
                          << ref_trans.event << "' (" 
                          << (ref_trans.is_sent ? "sent" : "received") 
                          << ") has no valid abstract counterpart" << std::endl;
                return false;
            }
        }
        
        // Step 5: Ensure no new events are introduced
        std::unordered_set<std::string> abstract_events;
        for (const auto& trans : abstract_outgoing) {
            abstract_events.insert(trans.event);
        }
        
        for (const auto& trans : refined_outgoing) {
            if (abstract_events.find(trans.event) == abstract_events.end()) {
                std::cout << "State correspondence failed: refined introduces new event '" 
                          << trans.event << "'" << std::endl;
                return false;
            }
        }
        
        // If we reach here, the state correspondence is valid
        correspondence_cache_.insert(corr);
        last_stats_.correspondences_verified++;
        
        std::cout << "State correspondence verified: refined_state=" << refined_state 
                  << " simulates abstract_state=" << abstract_state 
                  << " (zones compatible, transitions valid)" << std::endl;
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error in state correspondence check: " << e.what() << std::endl;
        return false;
    }
}

bool RTWBSChecker::check_rtwbs_equivalence(const TimedAutomaton& refined, 
                                          const TimedAutomaton& abstract) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Reset statistics
    last_stats_ = CheckStatistics{0, 0, 0, 0.0};
    refined.construct_zone_graph();
    abstract.construct_zone_graph();
    try {
        // Extract event transitions from both automata
        auto refined_transitions = extract_event_transitions(refined);
        auto abstract_transitions = extract_event_transitions(abstract);
        
        std::cout << "RTWBS Check: Refined has " << refined_transitions.size() 
                  << " transitions, Abstract has " << abstract_transitions.size() 
                  << " transitions" << std::endl;
        
        // RWTBS Refinement Check Algorithm (based on ICSE_DT paper):
        // 
        // The exploration strategy mentioned in the paper is implemented in 
        // TimedAutomaton::construct_zone_graph(), which uses BFS (breadth-first)
        // to systematically explore the reachable state space and build zone graphs 
        // for both the refined and abstract automata.
        //
        // For C' ⊑_RWTBS C (refined refines abstract), we must verify:
        // 1. ∀ (s' →_a s₁') ∈ C', ∃ (s ⇒_a s₁) ∈ C such that s₁' ~_RWTBS s₁
        // 2. Timing constraints: 
        //    - Received events: I_C(μ) ⊆ I_C'(μ) 
        //    - Sent events: I_C(μ) = I_C'(μ)
        //
        // This implementation operates on the transitions extracted from the
        // zone graphs constructed via BFS exploration.
        
        // Group transitions by event for efficient lookup
        std::unordered_map<std::string, std::vector<EventTransition>> abstract_by_event;
        for (const auto& trans : abstract_transitions) {
            abstract_by_event[trans.event].push_back(trans);
        }
        
        // Check each refined transition against abstract transitions
        bool all_transitions_valid = true;
        
        // Step 1: Event-level verification
        for (const auto& ref_trans : refined_transitions) {
            last_stats_.paths_checked++;
            
            // Find abstract transitions with same event
            auto it = abstract_by_event.find(ref_trans.event); // @todo applying abstraction function
            if (it == abstract_by_event.end()) {
                std::cout << "RWTBS Check: Event '" << ref_trans.event 
                          << "' not found in abstract automaton" << std::endl;
                all_transitions_valid = false;
                continue;
            }
            
            // Check if this refined transition satisfies RWTBS constraints
            bool found_valid_abstract = false;
            
            for (const auto& abs_trans : it->second) {
                // Check timing constraints according to RWTBS rules
                bool timing_valid = false;
                
                if (ref_trans.is_sent && abs_trans.is_sent) {
                    // Sent events: I_C(μ) = I_C'(μ) - refined must be same or stricter
                    timing_valid = (ref_trans.time_bound <= abs_trans.time_bound);
                    
                    std::cout << "  Sent event '" << ref_trans.event 
                              << "': refined_bound=" << ref_trans.time_bound 
                              << " vs abstract_bound=" << abs_trans.time_bound
                              << " -> " << (timing_valid ? "VALID" : "INVALID") << std::endl;
                              
                } else if (!ref_trans.is_sent && !abs_trans.is_sent) {
                    // Received events: I_C(μ) ⊆ I_C'(μ) - refined can be more relaxed
                    timing_valid = (ref_trans.time_bound >= abs_trans.time_bound);
                    
                    std::cout << "  Received event '" << ref_trans.event 
                              << "': refined_bound=" << ref_trans.time_bound 
                              << " vs abstract_bound=" << abs_trans.time_bound
                              << " -> " << (timing_valid ? "VALID" : "INVALID") << std::endl;
                              
                } else {
                    // Direction mismatch between refined and abstract
                    timing_valid = false;
                    std::cout << "  Event '" << ref_trans.event 
                              << "': direction mismatch (refined=" 
                              << (ref_trans.is_sent ? "sent" : "received")
                              << ", abstract=" 
                              << (abs_trans.is_sent ? "sent" : "received") 
                              << ")" << std::endl;
                }
                
                if (timing_valid) {
                    found_valid_abstract = true;
                    
                    // Step 2: Verify state correspondence for the target states
                    if (!verify_state_correspondence(ref_trans.to_state, abs_trans.to_state, 
                                                   refined, abstract)) {
                        std::cout << "  State correspondence failed for target states ("
                                  << ref_trans.to_state << " -> " << abs_trans.to_state << ")" << std::endl;
                        timing_valid = false;
                        found_valid_abstract = false;
                    }
                    
                    if (timing_valid) break; // Found a valid abstract counterpart
                }
            }
            
            if (!found_valid_abstract) {
                std::cout << "RWTBS Check: No valid abstract transition for refined event '" 
                          << ref_trans.event << "'" << std::endl;
                all_transitions_valid = false;
            }
        }
        
        // Additional check: Ensure no new events are introduced in refined
        std::unordered_set<std::string> abstract_events;
        for (const auto& trans : abstract_transitions) {
            abstract_events.insert(trans.event);
        }
        
        for (const auto& ref_trans : refined_transitions) {
            if (abstract_events.find(ref_trans.event) == abstract_events.end()) {
                std::cout << "RWTBS Check: Refined introduces new event '" 
                          << ref_trans.event << "' not in abstract" << std::endl;
                all_transitions_valid = false;
            }
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        last_stats_.check_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        
        if (all_transitions_valid) {
            std::cout << "RTWBS Check: All transitions satisfy RWTBS constraints" << std::endl;
        } else {
            std::cout << "RTWBS Check: RWTBS constraints violated" << std::endl;
        }
        
        return all_transitions_valid;
        
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
