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
    abstract.add_location(1, "Middle");
    abstract.add_location(2, "End");
    
    // Add a transition that receives an event
    abstract.add_transition(0, 1, "receive_data?");
    abstract.add_guard(0, 1, 0, 5, dbm_WEAK); // x1 <= 5
    // Set up synchronization for receive_data
    abstract.add_channel("receive_data");
    abstract.add_synchronization(0, "receive_data", false); // false = receiver
    
    // Add a transition that sends an event
    abstract.add_transition(1, 2, "send_ack!");
    abstract.add_guard(1, 1, 0, 10, dbm_WEAK); // x1 <= 10
    // Set up synchronization for send_ack
    abstract.add_channel("send_ack");
    abstract.add_synchronization(1, "send_ack", true); // true = sender
    
    // Initialize zone graph with proper initial zone (all clocks = 0)
    std::vector<raw_t> initial_zone(4); // 2x2 matrix for 2 clocks
    dbm_init(initial_zone.data(), 2); // Initialize as zero zone for 2 clocks
    abstract.construct_zone_graph(0, initial_zone, 1000, true); // Force construction
    
    std::cout << "Abstract automaton: receive_data? (x <= 5) -> send_ack! (x <= 10)" << std::endl;
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
    
    // Receive event with MORE RELAXED timing (valid for received events)
    refined.add_transition(0, 1, "receive_data?");
    refined.add_guard(0, 1, 0, 8, dbm_WEAK); // x1 <= 8 (more relaxed than abstract's 5)
    // Set up synchronization for receive_data
    refined.add_channel("receive_data");
    refined.add_synchronization(0, "receive_data", false); // false = receiver
    
    // Send event with STRICTER timing (valid for sent events)
    refined.add_transition(1, 2, "send_ack!");
    refined.add_guard(1, 1, 0, 7, dbm_WEAK); // x1 <= 7 (stricter than abstract's 10)
    // Set up synchronization for send_ack
    refined.add_channel("send_ack");
    refined.add_synchronization(1, "send_ack", true); // true = sender
    
    // Initialize zone graph with proper initial zone (all clocks = 0)
    std::vector<raw_t> initial_zone(4); // 2x2 matrix for 2 clocks
    dbm_init(initial_zone.data(), 2); // Initialize as zero zone for 2 clocks
    refined.construct_zone_graph(0, initial_zone, 1000, true); // Force construction
    
    std::cout << "Refined automaton: receive_data? (x <= 8) -> send_ack! (x <= 7)" << std::endl;
    return refined;
}

TimedAutomaton create_invalid_refinement() {
    std::cout << "Creating invalid refinement (violates RTWBS)..." << std::endl;
    
    // Create automaton with 2 clocks
    TimedAutomaton invalid(2);
    
    // Add locations
    invalid.add_location(0, "Start");
    invalid.add_location(1, "Middle");
    invalid.add_location(2, "End");
    
    // Received event with STRICTER timing (INVALID - should be more relaxed)
    invalid.add_transition(0, 1, "receive_data?");
    invalid.add_guard(0, 1, 0, 3, dbm_WEAK); // x1 <= 3 (stricter than abstract's 5 - INVALID!)
    // Set up synchronization for receive_data
    invalid.add_channel("receive_data");
    invalid.add_synchronization(0, "receive_data", false); // false = receiver
    
    // Sent event with MORE RELAXED timing (INVALID - should be stricter)
    invalid.add_transition(1, 2, "send_ack!");
    invalid.add_guard(1, 1, 0, 15, dbm_WEAK); // x1 <= 15 (more relaxed than abstract's 10 - INVALID!)
    // Set up synchronization for send_ack
    invalid.add_channel("send_ack");
    invalid.add_synchronization(1, "send_ack", true); // true = sender
    
    // Initialize zone graph with proper initial zone (all clocks = 0)
    std::vector<raw_t> initial_zone(4); // 2x2 matrix for 2 clocks
    dbm_init(initial_zone.data(), 2); // Initialize as zero zone for 2 clocks
    invalid.construct_zone_graph(0, initial_zone, 1000, true); // Force construction
    
    std::cout << "Invalid refinement: receive_data? (x <= 3) -> send_ack! (x <= 15) - violates RWTBS!" << std::endl;
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
        
        checker.print_statistics();    
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
        
        checker.print_statistics();
        
        std::cout << std::endl;
        
        // Test 3: Self-equivalence
        std::cout << "=== Test 3: Self-Equivalence ===" << std::endl;
        bool self_equiv = checker.check_rtwbs_equivalence(abstract, abstract);
        std::cout << "Abstract ≡ Abstract: " << (self_equiv ? "TRUE" : "FALSE") << std::endl;

        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
