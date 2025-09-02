#include <iostream>
#include "../src/timedautomaton.h"
#include "../src/rtwbs.h"

using namespace rtwbs; // Use the rtwbs namespace

/**
 * RTWBS Equivalence Example
 * 
 * This example demonstrates RTWBS (Relaxed Weak Timed Bisimulation) equivalence
 * checking between two timed automata. Based on the ICSE_DT paper, RTWBS allows:
 * 
 * - Relaxed timing on received events (can be delayed)
 * - Strict timing on sent events (must respect original bounds)
 * 
 * We create two automata:
 * 1. A "refined" automaton (e.g., Distributed Time - DT)
 * 2. An "abstract" automaton (e.g., Physical Time - PT)
 * 
 * The checker verifies if the refined automaton is a valid RTWBS refinement
 * of the abstract one.
 */

TimedAutomaton create_abstract_automaton() {
    std::cout << "Creating abstract automaton (PT - Physical Time)..." << std::endl;
    
    // Create automaton with 2 clocks (reference + 1 real clock)
    TimedAutomaton abstract(2);
    
    // Add locations
    abstract.add_location(0, "Start");
    abstract.add_location(1, "End");
    
    // Add a transition that sends an event
    abstract.add_transition(0, 1, "send_data!");
    
    std::cout << "Abstract automaton: send_data! from Start to End" << std::endl;
    return abstract;
}

TimedAutomaton create_refined_automaton() {
    std::cout << "Creating refined automaton (DT - Distributed Time)..." << std::endl;
    
    // Create automaton with 2 clocks (reference + 1 real clock)
    TimedAutomaton refined(2);
    
    // Add locations
    refined.add_location(0, "Start");
    refined.add_location(1, "Middle");
    refined.add_location(2, "End");
    
    // First transition: receive a trigger
    refined.add_transition(0, 1, "trigger?");
    
    // Second transition: send the data
    refined.add_transition(1, 2, "send_data!");
    
    std::cout << "Refined automaton: trigger? -> send_data!" << std::endl;
    return refined;
}

TimedAutomaton create_invalid_refinement() {
    std::cout << "Creating invalid refinement (violates RTWBS)..." << std::endl;
    
    // Create automaton with 2 clocks
    TimedAutomaton invalid(2);
    
    // Add locations
    invalid.add_location(0, "Start");
    invalid.add_location(1, "End");
    
    // This transition violates RTWBS: sent event with relaxed timing would be invalid
    // But since we can't add specific timing constraints easily, we'll use different action
    invalid.add_transition(0, 1, "invalid_send!");
    
    std::cout << "Invalid refinement: different event name (not matching abstract)" << std::endl;
    return invalid;
}

int main() {
    std::cout << "=== RTWBS Equivalence Checking Example ===" << std::endl;
    std::cout << "Based on the ICSE_DT paper on Relaxed Weak Timed Bisimulation" << std::endl;
    std::cout << std::endl;
    
    try {
        // Create the automata
        auto abstract = create_abstract_automaton();
        auto refined = create_refined_automaton();
        auto invalid = create_invalid_refinement();
        
        std::cout << std::endl;
        
        // Create the RTWBS checker
        rtwbs::RTWBSChecker checker;
        
        // Test 1: Valid refinement
        std::cout << "=== Test 1: Valid RTWBS Refinement ===" << std::endl;
        bool is_valid = checker.check_rtwbs_equivalence(refined, abstract);
        
        std::cout << "Result: " << (is_valid ? "VALID" : "INVALID") << " refinement" << std::endl;
        
        auto stats = checker.get_last_check_statistics();
        std::cout << "Statistics: " << stats.paths_checked << " paths checked, "
                  << stats.cache_hits << " cache hits, "
                  << stats.correspondences_verified << " correspondences verified" << std::endl;
        std::cout << "Check time: " << stats.check_time_ms << " ms" << std::endl;
        
        std::cout << std::endl;
        
        // Test 2: Invalid refinement
        std::cout << "=== Test 2: Invalid RTWBS Refinement ===" << std::endl;
        
        std::vector<rtwbs::EventTransition> counterexample;
        bool is_invalid_valid = checker.check_rtwbs_with_counterexample(invalid, abstract, counterexample);
        
        std::cout << "Result: " << (is_invalid_valid ? "VALID" : "INVALID") << " refinement" << std::endl;
        
        if (!counterexample.empty()) {
            std::cout << "Counterexample found:" << std::endl;
            for (const auto& trans : counterexample) {
                std::cout << "  Event: " << trans.event 
                          << " (" << (trans.is_sent ? "sent" : "received") << ")"
                          << " with bound " << trans.time_bound << std::endl;
            }
        }
        
        auto stats2 = checker.get_last_check_statistics();
        std::cout << "Statistics: " << stats2.paths_checked << " paths checked, "
                  << stats2.cache_hits << " cache hits" << std::endl;
        std::cout << "Check time: " << stats2.check_time_ms << " ms" << std::endl;
        
        std::cout << std::endl;
        
        // Test 3: Self-equivalence
        std::cout << "=== Test 3: Self-Equivalence ===" << std::endl;
        bool self_equiv = checker.check_rtwbs_equivalence(abstract, abstract);
        std::cout << "Abstract ≡ Abstract: " << (self_equiv ? "TRUE" : "FALSE") << std::endl;
        
        std::cout << std::endl;
        std::cout << "=== RTWBS Explanation ===" << std::endl;
        std::cout << "RTWBS (Relaxed Weak Timed Bisimulation) allows:" << std::endl;
        std::cout << "• Received events (?): can have MORE relaxed timing in refinement" << std::endl;
        std::cout << "• Sent events (!): must have SAME or STRICTER timing in refinement" << std::endl;
        std::cout << "• This models distributed systems where message delays are acceptable" << std::endl;
        std::cout << "  but sending timing must be respected for correctness" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
