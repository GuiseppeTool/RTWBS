#include "system.h"
#include "configs.h"
#include "utap/utap.hpp"
#include <iostream>
#include <stdexcept>

#include "utils.h"

namespace rtwbs {

// =================================================================
// System Class Implementation
// =================================================================

System::System(const std::string& fileName) {
    try {
        UTAP::Document doc;
        // Note: The actual UTAP API may vary - this is a simplified version
        // In practice, you'd need to check the exact UTAP API for parsing files
        std::cout << "Loading system from file: " << fileName << std::endl;
        
        // For now, we'll throw an exception indicating this needs proper UTAP integration
        //throw std::runtime_error("File parsing not yet fully implemented - use manual construction");
        
        // Future implementation would be:
         int result = parse_XML_file(fileName, doc, true);
         if (result == 0) {
             build_from_utap_document(doc);
         } else {
             throw std::runtime_error("Failed to parse UPPAAL file: " + fileName);
         }
    } catch (const std::exception& e) {
        throw std::runtime_error("Error loading system from file '" + fileName + "': " + e.what());
    }
}


void System::add_automaton(std::unique_ptr<TimedAutomaton> automaton, const std::string& template_name) {
    if (!automaton) {
        throw std::invalid_argument("Cannot add null automaton to system");
    }
    
    if (template_name.empty()) {
        throw std::invalid_argument("Template name cannot be empty");
    }
    
    if (has_template(template_name)) {
        throw std::invalid_argument("Template '" + template_name + "' already exists in system");
    }
    
    name_to_index_[template_name] = automata_.size();
    template_names_.push_back(template_name);
    automata_.push_back(std::move(automaton));
    
    std::cout << "Added automaton '" << template_name << "' to system (index " << (automata_.size() - 1) << ")" << std::endl;
}

const TimedAutomaton& System::get_automaton(size_t index) const {
    validate_index(index);
    return *automata_[index];
}

TimedAutomaton& System::get_automaton(size_t index) {
    validate_index(index);
    return *automata_[index];
}

const TimedAutomaton& System::get_automaton(const std::string& template_name) const {
    auto it = name_to_index_.find(template_name);
    if (it == name_to_index_.end()) {
        throw std::invalid_argument("Template '" + template_name + "' not found in system");
    }
    return *automata_[it->second];
}

TimedAutomaton& System::get_automaton(const std::string& template_name) {
    auto it = name_to_index_.find(template_name);
    if (it == name_to_index_.end()) {
        throw std::invalid_argument("Template '" + template_name + "' not found in system");
    }
    return *automata_[it->second];
}

bool System::has_template(const std::string& template_name) const {
    return name_to_index_.find(template_name) != name_to_index_.end();
}

const std::string& System::get_template_name(size_t index) const {
    validate_index(index);
    return template_names_[index];
}

void System::construct_all_zone_graphs() {
    std::cout << "Constructing zone graphs for all " << automata_.size() << " automata..." << std::endl;
    
    for (size_t i = 0; i < automata_.size(); ++i) {
        std::cout << "  Constructing zone graph for '" << template_names_[i] << "'..." << std::endl;
        automata_[i]->construct_zone_graph();
    }
    
    std::cout << "Zone graph construction complete for all automata." << std::endl;
}

void System::print_all_statistics() const {
    std::cout << "=== System Statistics ===" << std::endl;
    std::cout << "Number of automata: " << automata_.size() << std::endl;
    std::cout << std::endl;
    
    for (size_t i = 0; i < automata_.size(); ++i) {
        std::cout << "--- Automaton " << i << ": " << template_names_[i] << " ---" << std::endl;
        automata_[i]->print_statistics();
        std::cout << std::endl;
    }
    std::cout << "=========================" << std::endl;
}

void System::print_system_overview() const {
    std::cout << "=== System Overview ===" << std::endl;
    std::cout << "Total automata: " << automata_.size() << std::endl;
    
    for (size_t i = 0; i < automata_.size(); ++i) {
        std::cout << "  [" << i << "] " << template_names_[i] 
                  << " (dimension: " << automata_[i]->get_dimension() 
                  << ", states: " << automata_[i]->get_num_states() << ")" << std::endl;
    }
    std::cout << "======================" << std::endl;
}

void System::remove_automaton(size_t index) {
    validate_index(index);
    
    std::string template_name = template_names_[index];
    
    // Remove from containers
    automata_.erase(automata_.begin() + index);
    template_names_.erase(template_names_.begin() + index);
    
    // Update name_to_index map
    name_to_index_.erase(template_name);
    for (auto& pair : name_to_index_) {
        if (pair.second > index) {
            pair.second--;
        }
    }
    
    std::cout << "Removed automaton '" << template_name << "' from system" << std::endl;
}

void System::remove_automaton(const std::string& template_name) {
    auto it = name_to_index_.find(template_name);
    if (it == name_to_index_.end()) {
        throw std::invalid_argument("Template '" + template_name + "' not found in system");
    }
    
    size_t index = it->second;
    remove_automaton(index);
}

void System::clear() {
    automata_.clear();
    template_names_.clear();
    name_to_index_.clear();
    
    std::cout << "Cleared all automata from system" << std::endl;
}

void System::build_from_utap_document(UTAP::Document& doc) {
    // This is where you would parse the UTAP document and create automata
    // For each template in the document, create a separate TimedAutomaton
    
    std::cout << "Building system from UTAP document..." << std::endl;
    
    // Simplified implementation - in practice this would:
    // 1. Iterate through all templates in the document
    // 2. For each template, create a TimedAutomaton
    // 3. Parse the template's locations, transitions, etc.
    // 4. Add the automaton to the system
    
    std::unordered_map<std::string, cindex_t> clock_map;
    std::unordered_map<std::string, int> constants;
    std::unordered_map<std::string, int> variables;  // Non-constant variables like 'id'
    
    // Extract clocks from global declarations
    auto& global_declarations = doc.get_globals();
    cindex_t clock_count = 0;

    
    DEV_PRINT("   Analyzing global declarations..." << std::endl);
    
    DEV_PRINT("   Global declarations has " << global_declarations.variables.size() << " variables:" << std::endl);
    
    for (const auto& variable : global_declarations.variables) {
        if (variable.uid.get_type().is_clock()) {
            clock_count++;
            clock_map[variable.uid.get_name()] = clock_count; // Start from 1, as 0 is the reference clock
            DEV_PRINT("   Found clock: " << variable.uid.get_name() << " -> index " << clock_count << std::endl);
        } else if (variable.uid.get_type().is_constant()) {
            std::string const_name = variable.uid.get_name();
            
            // Skip built-in constants (they start with standard prefixes)
            if (const_name.find("INT") == 0 || const_name.find("UINT") == 0 || 
                const_name.find("FLT") == 0 || const_name.find("DBL") == 0 || 
                const_name.find("M_") == 0) {
                // Skip built-in constants
                continue;
            }
            
            // Process user-defined constants
            int value;
            if (EVALUATE_EXPRESSION(variable.init, value, constants, variables)) {
                constants[const_name] = value;
                DEV_PRINT("   Found global constant: " << const_name << " = " << value << std::endl);
            } else {
                DEV_PRINT("   Failed to evaluate global constant: " << const_name << std::endl);
            }
        } else {
            // Save non-constant, non-clock variables (like 'id')
            variables[variable.uid.get_name()] = 0; // Initialize to 0
            DEV_PRINT("   Found global variable: " << variable.uid.get_name() << std::endl);
        }
    }
    
    // Set dimension = reference clock (index 0) + number of clocks

    //TimedAutomaton automaton(template_ref, clock_count+1, clock_map, constants, variables);
    // Future implementation outline:
    
    for (const auto& template_ref : doc.get_templates()) {

            
            // Create automaton from template
            auto automaton = 
                std::make_unique<TimedAutomaton>(template_ref, clock_count+1, clock_map, constants, variables); 
        // Add to system
        add_automaton(std::move(automaton), template_ref.uid.get_name());
    }
    
}



void System::validate_index(size_t index) const {
    if (index >= automata_.size()) {
        throw std::out_of_range("Automaton index " + std::to_string(index) + 
                               " out of range (system has " + std::to_string(automata_.size()) + " automata)");
    }
}

} // namespace rtwbs
