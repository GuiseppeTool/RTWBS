#include <iostream>
#include "rtwbs.h"

using namespace rtwbs;

/**
 * System-Level RTWBS Example
 * 
 * This example demonstrates RTWBS equivalence checking between two systems,
 * where each system contains multiple timed automata templates.
 * 
 * We create two systems:
 * 1. A refined system (e.g., Distributed Time implementation)
 * 2. An abstract system (e.g., Physical Time specification)
 * 
 * Each system contains multiple automata representing different components
 * of a distributed system (e.g., Sensor, Controller, Actuator).
 */

System create_abstract_system() {
    std::cout << "Creating abstract system (Physical Time specification)..." << std::endl;
    
    System abstract_system;
    
    // === Template 1: Sensor ===
    auto sensor = std::make_unique<TimedAutomaton>(2); // 1 clock + reference
    
    // Sensor locations
    sensor->add_location(0, "Idle");
    sensor->add_location(1, "Sensing");
    sensor->add_location(2, "DataReady");
    
    // Sensor transitions
    sensor->add_transition(0, 1, "start_sense");
    sensor->add_guard(0, 1, 0, 2, dbm_WEAK); // x <= 2
    
    sensor->add_transition(1, 2, "data_out!");
    sensor->add_guard(1, 1, 0, 5, dbm_WEAK); // x <= 5
    sensor->add_synchronization(1, "data", true); // sender
    
    sensor->add_transition(2, 0, "reset");
    sensor->add_reset(2, 1); // reset clock x
    
    // Initialize zone graph
    std::vector<raw_t> initial_zone(4);
    dbm_init(initial_zone.data(), 2);
    sensor->construct_zone_graph(0, initial_zone, 1000, true);
    
    abstract_system.add_automaton(std::move(sensor), "Sensor");
    
    // === Template 2: Controller ===
    auto controller = std::make_unique<TimedAutomaton>(2); // 1 clock + reference
    
    // Controller locations
    controller->add_location(0, "Waiting");
    controller->add_location(1, "Processing");
    controller->add_location(2, "OutputReady");
    
    // Controller transitions
    controller->add_transition(0, 1, "data_in?");
    controller->add_guard(0, 1, 0, 10, dbm_WEAK); // x <= 10
    controller->add_synchronization(0, "data", false); // receiver
    
    controller->add_transition(1, 2, "process");
    controller->add_guard(1, 1, 0, 3, dbm_WEAK); // x <= 3
    
    controller->add_transition(2, 0, "control_out!");
    controller->add_guard(2, 1, 0, 8, dbm_WEAK); // x <= 8
    controller->add_synchronization(2, "control", true); // sender
    controller->add_reset(2, 1); // reset clock x
    
    // Initialize zone graph
    dbm_init(initial_zone.data(), 2);
    controller->construct_zone_graph(0, initial_zone, 1000, true);
    
    abstract_system.add_automaton(std::move(controller), "Controller");
    
    std::cout << "Abstract system created with " << abstract_system.size() << " templates" << std::endl;
    return abstract_system;
}

System create_refined_system() {
    std::cout << "Creating refined system (Distributed Time implementation)..." << std::endl;
    
    System refined_system;
    
    // === Template 1: Sensor (refined) ===
    auto sensor = std::make_unique<TimedAutomaton>(2); // 1 clock + reference
    
    // Sensor locations (same structure)
    sensor->add_location(0, "Idle");
    sensor->add_location(1, "Sensing");
    sensor->add_location(2, "DataReady");
    
    // Sensor transitions (with adjusted timing for distributed system)
    sensor->add_transition(0, 1, "start_sense");
    sensor->add_guard(0, 1, 0, 3, dbm_WEAK); // x <= 3 (slightly more relaxed)
    
    sensor->add_transition(1, 2, "data_out!");
    sensor->add_guard(1, 1, 0, 4, dbm_WEAK); // x <= 4 (stricter for sent event - VALID for RTWBS)
    sensor->add_synchronization(1, "data", true); // sender
    
    sensor->add_transition(2, 0, "reset");
    sensor->add_reset(2, 1); // reset clock x
    
    // Initialize zone graph
    std::vector<raw_t> initial_zone(4);
    dbm_init(initial_zone.data(), 2);
    sensor->construct_zone_graph(0, initial_zone, 1000, true);
    
    refined_system.add_automaton(std::move(sensor), "Sensor");
    
    // === Template 2: Controller (refined) ===
    auto controller = std::make_unique<TimedAutomaton>(2); // 1 clock + reference
    
    // Controller locations (same structure)
    controller->add_location(0, "Waiting");
    controller->add_location(1, "Processing");
    controller->add_location(2, "OutputReady");
    
    // Controller transitions (with adjusted timing for distributed system)
    controller->add_transition(0, 1, "data_in?");
    controller->add_guard(0, 1, 0, 15, dbm_WEAK); // x <= 15 (more relaxed for received event - VALID for RTWBS)
    controller->add_synchronization(0, "data", false); // receiver
    
    controller->add_transition(1, 2, "process");
    controller->add_guard(1, 1, 0, 3, dbm_WEAK); // x <= 3 (same internal timing)
    
    controller->add_transition(2, 0, "control_out!");
    controller->add_guard(2, 1, 0, 6, dbm_WEAK); // x <= 6 (stricter for sent event - VALID for RTWBS)
    controller->add_synchronization(2, "control", true); // sender
    controller->add_reset(2, 1); // reset clock x
    
    // Initialize zone graph
    dbm_init(initial_zone.data(), 2);
    controller->construct_zone_graph(0, initial_zone, 1000, true);
    
    refined_system.add_automaton(std::move(controller), "Controller");
    
    std::cout << "Refined system created with " << refined_system.size() << " templates" << std::endl;
    return refined_system;
}

System create_invalid_system() {
    std::cout << "Creating invalid system (violates RTWBS rules)..." << std::endl;
    
    System invalid_system;
    
    // === Template 1: Sensor (invalid) ===
    auto sensor = std::make_unique<TimedAutomaton>(2);
    
    sensor->add_location(0, "Idle");
    sensor->add_location(1, "Sensing");
    sensor->add_location(2, "DataReady");
    
    sensor->add_transition(0, 1, "start_sense");
    sensor->add_guard(0, 1, 0, 2, dbm_WEAK); // x <= 2
    
    sensor->add_transition(1, 2, "data_out!");
    sensor->add_guard(1, 1, 0, 10, dbm_WEAK); // x <= 10 (MORE RELAXED for sent event - INVALID!)
    sensor->add_synchronization(1, "data", true); // sender
    
    sensor->add_transition(2, 0, "reset");
    sensor->add_reset(2, 1);
    
    std::vector<raw_t> initial_zone(4);
    dbm_init(initial_zone.data(), 2);
    sensor->construct_zone_graph(0, initial_zone, 1000, true);
    
    invalid_system.add_automaton(std::move(sensor), "Sensor");
    
    // === Template 2: Controller (invalid) ===
    auto controller = std::make_unique<TimedAutomaton>(2);
    
    controller->add_location(0, "Waiting");
    controller->add_location(1, "Processing");
    controller->add_location(2, "OutputReady");
    
    controller->add_transition(0, 1, "data_in?");
    controller->add_guard(0, 1, 0, 5, dbm_WEAK); // x <= 5 (STRICTER for received event - INVALID!)
    controller->add_synchronization(0, "data", false); // receiver
    
    controller->add_transition(1, 2, "process");
    controller->add_guard(1, 1, 0, 3, dbm_WEAK);
    
    controller->add_transition(2, 0, "control_out!");
    controller->add_guard(2, 1, 0, 8, dbm_WEAK);
    controller->add_synchronization(2, "control", true);
    controller->add_reset(2, 1);
    
    dbm_init(initial_zone.data(), 2);
    controller->construct_zone_graph(0, initial_zone, 1000, true);
    
    invalid_system.add_automaton(std::move(controller), "Controller");
    
    std::cout << "Invalid system created with " << invalid_system.size() << " templates" << std::endl;
    return invalid_system;
}

int main() {
    std::cout << "=== System-Level RTWBS Equivalence Checking Example ===" << std::endl;
    std::cout << "Demonstrates RTWBS checking between systems with multiple automata" << std::endl;
    std::cout << std::endl;
    
    try {
        // Create the systems
        auto abstract_system = create_abstract_system();
        auto refined_system = create_refined_system();
        auto invalid_system = create_invalid_system();
        
        std::cout << std::endl;
        
        // Create the RTWBS checker
        RTWBSChecker checker;
        
        // Test 1: Valid system refinement
        std::cout << "=== Test 1: Valid System Refinement ===" << std::endl;
        bool is_valid = checker.check_rtwbs_equivalence(refined_system, abstract_system);
        std::cout << "Overall Result: " << (is_valid ? "VALID" : "INVALID") << " system refinement" << std::endl;
        std::cout << std::endl;
        
        // Test 2: Invalid system refinement with detailed results
        std::cout << "=== Test 2: Invalid System Refinement (Detailed) ===" << std::endl;
        std::vector<RTWBSChecker::SystemCheckResult> detailed_results;
        bool is_invalid_valid = checker.check_rtwbs_equivalence_detailed(
            invalid_system, abstract_system, detailed_results);
        
        std::cout << "Overall Result: " << (is_invalid_valid ? "VALID" : "INVALID") << " system refinement" << std::endl;
        
        // Print detailed results
        std::cout << "--- Detailed Results ---" << std::endl;
        for (const auto& result : detailed_results) {
            std::cout << "Automaton " << result.automaton_index 
                      << " (" << result.template_name_refined << " ≼ " << result.template_name_abstract << "): "
                      << (result.is_equivalent ? "EQUIVALENT" : "NOT EQUIVALENT") << std::endl;
            
            // Counterexample reporting removed in new simplified API
            
            std::cout << "  Statistics: " << result.statistics.refined_states 
                      << " refined states, " << result.statistics.abstract_states 
                      << " abstract states, " << result.statistics.simulation_pairs 
                      << " final pairs" << std::endl;
        }
        std::cout << std::endl;
        
        // Test 3: System self-equivalence
        std::cout << "=== Test 3: System Self-Equivalence ===" << std::endl;
        bool self_equiv = checker.check_rtwbs_equivalence(abstract_system, abstract_system);
        std::cout << "Self-equivalence Result: " << (self_equiv ? "EQUIVALENT" : "NOT EQUIVALENT") << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << std::endl;
    std::cout << "=== System-Level RTWBS Explanation ===" << std::endl;
    std::cout << "• Systems contain multiple automata templates" << std::endl;
    std::cout << "• Each automaton pair is checked independently" << std::endl;
    std::cout << "• RTWBS rules apply to each individual automaton pair:" << std::endl;
    std::cout << "  - Sent events (!): refined timing ≤ abstract timing" << std::endl;
    std::cout << "  - Received events (?): refined timing ≥ abstract timing" << std::endl;
    std::cout << "• System is valid if ALL automaton pairs are valid" << std::endl;
    
    return 0;
}
