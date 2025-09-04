#include "timedautomaton.h"

#include "utils.h"
#include "utap/utap.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <fstream>


namespace rtwbs {

TimedAutomaton::TimedAutomaton(const char* xml_content):constructed_(false) {
    DEV_PRINT("TimedAutomaton: Parsing XML content..." << std::endl);
    
    // Create a UTAP document and parse the XML
    UTAP::Document doc;
    int32_t result = parse_XML_buffer(xml_content, doc, true);
    
    if (result != 0) {
        throw std::runtime_error("Failed to parse XML with error code: " + std::to_string(result));
    }
    
    DEV_PRINT("   XML parsed successfully!" << std::endl);
    DEV_PRINT("   Document contains " << doc.get_templates().size() << " template(s)" << std::endl);
    
    if (doc.get_templates().empty()) {
        throw std::runtime_error("No templates found in document!");
    }
    
    // Build the automaton from the UTAP document
    build_from_utap_document(doc);
    
    DEV_PRINT("   TimedAutomaton construction complete!" << std::endl);
}

TimedAutomaton::TimedAutomaton(const std::string& fileName):constructed_(false)
{
    // Create a UTAP document and parse the XML
    UTAP::Document doc;
    int res = parse_XML_file(fileName, doc, true);
    if (res != 0) {
        throw std::runtime_error("Failed to parse XML file: " + fileName);
    }

    // Build the automaton from the UTAP document
    build_from_utap_document(doc);
}

///TO DO DEAL WITH GLOBAL VARIABLES

void TimedAutomaton::build_from_utap_document(UTAP::Document& doc) {
    DEV_PRINT("   Converting UTAP document to TimedAutomaton..." << std::endl);
    
    // Debug: Check document-level information
    DEV_PRINT("   Document has " << doc.get_templates().size() << " templates" << std::endl);
    
    // Check if there are processes (instantiated templates)
    //auto& processes = doc.get_processes();
    //std::cout << "   Document has " << processes.size() << " instantiated processes" << std::endl;
    //for (const auto& process : processes) {
    //    std::cout << "     Process: " << process.uid.get_name() << std::endl;
    //    std::cout << "       Process parameters frame size: " << process.parameters.get_size() << std::endl;
    //    std::cout << "       Process unbound parameters: " << process.unbound << std::endl;
    //    std::cout << "       Process arguments: " << process.arguments << std::endl;
    //    
    //    // Check parameter mapping
    //    std::cout << "       Process parameter mapping:" << std::endl;
    //    for (const auto& [symbol, expression] : process.mapping) {
    //        std::cout << "         " << symbol.get_name() << " -> " << expression.str() << std::endl;
    //    }
    //}
    
    // Get the first template
    auto& templates = doc.get_templates();
    auto& template_ref = templates.front();

    // Debug: More detailed template inspection
    //std::cout << "   Template name: " << template_ref.uid.get_name() << std::endl;
    //std::cout << "   Template unbound parameters: " << template_ref.unbound << std::endl;
    //std::cout << "   Template arguments: " << template_ref.arguments << std::endl;
    //std::cout << "   Template parameters frame size: " << template_ref.parameters.get_size() << std::endl;
    //
    //// Let's also check the instance mapping (parameters to arguments)
    //std::cout << "   Template parameter mapping size: " << template_ref.mapping.size() << std::endl;
    //for (const auto& [symbol, expression] : template_ref.mapping) {
    //    std::cout << "     Mapping: " << symbol.get_name() << " -> " << expression.str() << std::endl;
    //}


    // Create clock name to index mapping
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
            if (evaluate_expression(variable.init, value)) {
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
    
    build_from_template(template_ref, clock_count+1, clock_map, constants, variables);
}
void TimedAutomaton::build_from_template(const UTAP::Template& template_ref, int dimensions, 
    const std::unordered_map<std::string, cindex_t>& clock_map,
    const std::unordered_map<std::string, int>& constants,
    const std::unordered_map<std::string, int>& variables
){
    constants_ = constants;
    variables_ = variables;
    
    // Copy global clocks to member clock_map_
    clock_map_.clear();
    for (const auto& clock_pair : clock_map) {
        clock_map_[clock_pair.first] = clock_pair.second;
        DEV_PRINT("   Added global clock to clock_map_: " << clock_pair.first << " -> " << clock_pair.second << std::endl);
    }
    
    // Recalculate dimension: reference + global clocks + template clocks
    int template_clock_count = 0;
    for (const auto& variable : template_ref.variables) {
        if (variable.uid.get_type().is_clock()) {
            clock_map_[variable.uid.get_name()] = dimensions++;
            template_clock_count++;
            DEV_PRINT("   Found template clock: " << variable.uid.get_name() << std::endl);
        } else if (variable.uid.get_type().is_constant()) {
            int value;
            if (EVALUATE_EXPRESSION(variable.init, value, constants_, variables_)) {
                constants_[variable.uid.get_name()] = value;
                DEV_PRINT("   Found template constant: " << variable.uid.get_name() << " = " << value << std::endl);
            } else {
                DEV_PRINT("   Failed to evaluate template constant: " << variable.uid.get_name() << std::endl);
            }
        } else {
            // Save non-constant, non-clock template variables
            variables_[variable.uid.get_name()] = 0; // Initialize to 0
            DEV_PRINT("   Found template variable: " << variable.uid.get_name() << std::endl);
        }
    }

    // Process template parameters (they should be treated as variables)
    DEV_PRINT("\n   Template has " << template_ref.unbound << " unbound parameters" << std::endl);
    if (template_ref.parameters.get_size() > 0) {
        DEV_PRINT("   Template has " << template_ref.parameters.get_size() << " parameters:" << std::endl);

        for (size_t i = 0; i < template_ref.parameters.get_size(); ++i) {
            const auto& param = template_ref.parameters[i];
            DEV_PRINT("   Template Parameter: " << param.get_name() 
                      << ", type: " << param.get_type().str() << std::endl);

            // Parameters are treated as variables
            if (!param.get_type().is_clock() && !param.get_type().is_constant()) {
                variables_[param.get_name()] = 0; // Initialize to 0
                DEV_PRINT("   Found template parameter (treated as variable): " << param.get_name() << std::endl);
            } else if (param.get_type().is_constant()) {
                // Parameter constants - note: symbols don't have init expressions directly
                DEV_PRINT("   Found template parameter constant: " << param.get_name() << " (value to be determined)" << std::endl);
                // For now, we'll skip evaluating parameter constants since they don't have init expressions
                constants_[param.get_name()] = 0; // Default value
            } else if (param.get_type().is_clock()) {
                // Clock parameters
                clock_map_[param.get_name()] = dimensions++;
                template_clock_count++;
                DEV_PRINT("   Found template parameter clock: " << param.get_name() << std::endl);
            }
        }
    }

    int total_clocks = (dimensions - 1) + template_clock_count; // (dimensions-1) is global clocks from constructor
    dimension_ = dimensions + template_clock_count; // +1 for reference clock

    DEV_PRINT("   Creating automaton with dimension " << dimension_ << " (" << total_clocks << " clocks + reference)" << std::endl);
    DEV_PRINT("   Global clocks: " << (dimensions - 1) << ", Template clocks: " << template_clock_count << ", Total dimension: " << dimension_ << std::endl);


    DEV_PRINT("   Template has " << template_ref.variables.size() << " variables:" << std::endl);
    DEV_PRINT("   Template frame has " << template_ref.frame.get_size() << " symbols:" << std::endl);
    DEV_PRINT("   Template parameters frame has " << template_ref.parameters.get_size() << " symbols:" << std::endl);

    // Debug: List all frame symbols to see if parameters are mixed in
    DEV_PRINT("   Template frame symbols:" << std::endl);
    for (size_t i = 0; i < template_ref.frame.get_size(); ++i) {
        const auto& symbol = template_ref.frame[i];
        DEV_PRINT("     Frame Symbol " << i << ": " << symbol.get_name() 
                  << ", type: " << symbol.get_type().str() 
                  << ", is_clock: " << symbol.get_type().is_clock()
                  << ", is_constant: " << symbol.get_type().is_constant() << std::endl);
    }
    


    
    // Add locations
    std::unordered_map<std::string, int> location_map;

    for (size_t i = 0; i < template_ref.locations.size(); ++i) {
        std::string loc_id = template_ref.locations[i].uid.get_name();
        std::string loc_name = loc_id;
        
        int loc_int_id = static_cast<int>(i);
        location_map[loc_id] = loc_int_id;
        add_location(loc_int_id, loc_name);
        
        DEV_PRINT("   Added location: " << loc_name << " (ID: " << loc_id << " -> " << loc_int_id << ")" << std::endl);
        
        // Parse and add location invariants
        if (!template_ref.locations[i].invariant.empty()) {
            std::string invariant_str = template_ref.locations[i].invariant.str();
            DEV_PRINT("     Invariant: " << invariant_str << std::endl);
            
            std::string clock_name;
            std::string op;
            int value;

            if (parse_clock_constraint_from_expr(template_ref.locations[i].invariant, clock_name, op, value)) {
                DEV_PRINT("     Clock constraint parsed: " << clock_name << " " << op << " " << value << std::endl);
                DEV_PRINT("     Checking if clock '" << clock_name << "' is in global_clocks..." << std::endl);
                bool in_global =  clock_map.find(clock_name) != clock_map.end();
                DEV_PRINT("     In global_clocks: " << (in_global ? "YES" : "NO") << std::endl);

                DEV_PRINT("     Checking if clock '" << clock_name << "' is in private_clocks_..." << std::endl);
                bool in_private =  clock_map_.find(clock_name) != clock_map_.end();
                DEV_PRINT("     In private_clocks_: " << (in_private ? "YES" : "NO") << std::endl);


                if (in_global || in_private) {
                    if(in_global && !in_private){
                      clock_map_[clock_name] = clock_map.at(clock_name);  
                    }
                    DEV_PRINT("     Parsed invariant: " << clock_name << " " << op << " " << value << std::endl);
                    add_dbm_constraint(clock_name, op, value, clock_map_, loc_int_id, -1);
                    DEV_PRINT("     Added invariant constraint to location" << std::endl);
                } else {
                    DEV_PRINT("     Clock validation failed - not in both lists" << std::endl);
                }
            } else {
                DEV_PRINT("     Failed to parse invariant constraint: " << invariant_str << std::endl);
            }
        } else {
            DEV_PRINT("     No invariant on this location" << std::endl);
        }
    }
    
    // Add transitions
    for (size_t i = 0; i < template_ref.edges.size(); ++i) {
        auto& edge = template_ref.edges[i];
        
        // Get source and destination location names
        std::string source_id, target_id;
        if (edge.src != nullptr) {
            source_id = edge.src->uid.get_name();
        }
        if (edge.dst != nullptr) {
            target_id = edge.dst->uid.get_name();
        }
        
        if (source_id.empty() || target_id.empty()) {
            DEV_PRINT("   Skipping edge with missing source or destination" << std::endl);
            continue;
        }
        
        int source_int = location_map[source_id];
        int target_int = location_map[target_id];
        
        // Parse assignment for action name
        std::string action = TA_CONFIG.default_action_name;  // Default action name
        if (!edge.assign.empty()) {
            std::string assign_str = edge.assign.str();
            DEV_PRINT("     Assignment: " << assign_str << std::endl);
            
            // Try to parse as clock reset first
            std::string reset_clock_name;
            int reset_value;
            
            if (parse_clock_reset_from_expr(edge.assign, reset_clock_name, reset_value)) {
                DEV_PRINT("     Parsed reset: " << reset_clock_name << " := " << reset_value << std::endl);
                // Check if this is actually a clock
                if (clock_map_.find(reset_clock_name) != clock_map_.end()) {
                    cindex_t clock_idx = clock_map_[reset_clock_name];
                    // Only handle resets to 0 (standard timed automata behavior)
                    if (reset_value == 0) {
                        // Add transition first, then the reset
                        add_transition(source_int, target_int, action);
                        add_reset(i, clock_idx);
                        DEV_PRINT("     Added reset to transition: " << reset_clock_name << " -> 0" << std::endl);
                    } else {
                        DEV_PRINT("     Warning: Non-zero reset value " << reset_value << " not supported" << std::endl);
                        add_transition(source_int, target_int, action);
                    }
                } else {
                    // Check if it's a variable assignment
                    if (variables_.find(reset_clock_name) != variables_.end()) {
                        variables_[reset_clock_name] = reset_value;
                        DEV_PRINT("     Parsed variable assignment: " << reset_clock_name << " := " << reset_value << std::endl);
                        add_transition(source_int, target_int, action);
                    } else {
                        // Not a clock or known variable, use assignment as action name
                        action = assign_str;
                        add_transition(source_int, target_int, action);
                    }
                }
            } else {
                // Try to parse as variable assignment
                std::string var_name;
                int var_value;
                
                if (parse_variable_assignment_from_expr(edge.assign, var_name, var_value)) {
                    if (variables_.find(var_name) != variables_.end()) {
                        variables_[var_name] = var_value;
                        DEV_PRINT("     Parsed variable assignment: " << var_name << " := " << var_value << std::endl);
                        add_transition(source_int, target_int, action);
                    } else {
                        // Unknown variable, use assignment as action name
                        action = assign_str;
                        add_transition(source_int, target_int, action);
                    }
                } else {
                    // Use assignment as action name
                    action = assign_str;
                    add_transition(source_int, target_int, action);
                }
            }
        } else {
            add_transition(source_int, target_int, action);
        }
        
        DEV_PRINT("   Added transition: " << source_id << " -> " << target_id 
                  << " (" << source_int << " -> " << target_int << ")" << std::endl);
        
        // Parse and add guard constraints
        if (!edge.guard.empty()) {
            std::string guard_str = edge.guard.str();
            DEV_PRINT("     Guard: " << guard_str << std::endl);
            DEV_PRINT("     Guard expression kind: " << edge.guard.get_kind() << std::endl);
            DEV_PRINT("     Guard expression size: " << edge.guard.get_size() << std::endl);
            
            // Extract all constraints from the guard expression
            std::vector<Constraint> constraints;
            extract_all_constraints(edge.guard, constraints);
            
            bool has_clock_constraints = false;
            bool variable_constraints_satisfied = true;
            
            // Process each constraint
            for (const auto& constraint : constraints) {
                if (constraint.is_clock) {
                    // This is a clock constraint - add it to the DBM
                    if (clock_map_.find(constraint.name) != clock_map_.end()) {
                        add_dbm_constraint(constraint.name, constraint.op, constraint.value, clock_map_, -1, i);
                        has_clock_constraints = true;
                        DEV_PRINT("     Added clock constraint: " << constraint.name << " " 
                                 << constraint.op << " " << constraint.value << std::endl);
                    }
                } else {
                    // This is a variable constraint - evaluate it
                    if (!evaluate_variable_constraint(constraint.name, constraint.op, constraint.value)) {
                        variable_constraints_satisfied = false;
                        DEV_PRINT("     Variable constraint not satisfied: " << constraint.name << " " 
                                 << constraint.op << " " << constraint.value << std::endl);
                    } else {
                        DEV_PRINT("     Variable constraint satisfied: " << constraint.name << " " 
                                 << constraint.op << " " << constraint.value << std::endl);
                    }
                }
            }
            
            // If no constraints were found, try the old parsing methods as fallback
            if (constraints.empty()) {
                std::string clock_name, var_name, op;
                int value;
                
                // Try clock constraint parsing
                if (parse_clock_constraint_from_expr(edge.guard, clock_name, op, value) 
                    && clock_map_.find(clock_name) != clock_map_.end()) {
                    add_dbm_constraint(clock_name, op, value, clock_map_, -1, i);
                    DEV_PRINT("     Added fallback clock constraint: " << clock_name << " " << op << " " << value << std::endl);
                    has_clock_constraints = true;
                }
                // Try variable constraint parsing
                else if (parse_variable_constraint_from_expr(edge.guard, var_name, op, value)) {
                    if (!evaluate_variable_constraint(var_name, op, value)) {
                        variable_constraints_satisfied = false;
                        DEV_PRINT("     Fallback variable constraint not satisfied: " << var_name << " " 
                                 << op << " " << value << std::endl);
                    } else {
                        DEV_PRINT("     Fallback variable constraint satisfied: " << var_name << " " 
                                 << op << " " << value << std::endl);
                    }
                } else {
                    DEV_PRINT("     Failed to parse guard constraint: " << guard_str << std::endl);
                }
            }
            
            // Skip this transition if variable constraints are not satisfied
            if (!variable_constraints_satisfied) {
                DEV_PRINT("     Skipping transition due to unsatisfied variable constraints" << std::endl);
                continue;
            }
            
            if (has_clock_constraints || !constraints.empty()) {
                DEV_PRINT("     Added guard constraints to transition" << std::endl);
            }
        } else {
            DEV_PRINT("     No guard on this transition" << std::endl);
        }
        
        // Parse and add synchronization labels
        if (!edge.sync.empty()) {
            std::string sync_str = edge.sync.str();
            DEV_PRINT("     Synchronization: " << sync_str << std::endl);
            
            std::string channel;
            bool is_sender;
            
            if (parse_synchronization_from_expr(edge.sync, channel, is_sender)) {
                DEV_PRINT("     Parsed synchronization: " << channel << (is_sender ? "!" : "?") << std::endl);
                
                add_synchronization(i, channel, is_sender);
                add_channel(channel);
                DEV_PRINT("     Added synchronization to transition" << std::endl);
            } else {
                DEV_PRINT("     Failed to parse synchronization: " << sync_str << std::endl);
            }
        } else {
            DEV_PRINT("     No synchronization on this transition" << std::endl);
        }
    }
}

bool TimedAutomaton::evaluate_expression(const UTAP::Expression& expr, int& result) {
    if (expr.empty()) {
        return false;
    }
    
    auto kind = expr.get_kind();
    
    switch (kind) {
        case UTAP::Constants::CONSTANT:
            result = expr.get_value();
            return true;
            
        case UTAP::Constants::IDENTIFIER: {
            // For variables, we need to get their value from the symbol
            auto symbol = expr.get_symbol();
            if (symbol.get_type().is_integer()) {
                // Check if it's a known constant
                if (constants_.count(symbol.get_name())) {
                    result = constants_.at(symbol.get_name());
                    return true;
                }
                // Check if it's a known variable
                if (variables_.count(symbol.get_name())) {
                    result = variables_.at(symbol.get_name());
                    return true;
                }
                // For integer variables with initializers
                auto* var_data = static_cast<const UTAP::Variable*>(symbol.get_data());
                if (var_data && !var_data->init.empty()) {
                    return evaluate_expression(var_data->init, result);
                }
            }
            return false;
        }
        
        case UTAP::Constants::PLUS:
            if (expr.get_size() == 2) {
                int left, right;
                if (evaluate_expression(expr[0], left) && evaluate_expression(expr[1], right)) {
                    result = left + right;
                    return true;
                }
            }
            return false;
            
        case UTAP::Constants::MINUS:
            if (expr.get_size() == 2) {
                int left, right;
                if (evaluate_expression(expr[0], left) && evaluate_expression(expr[1], right)) {
                    result = left - right;
                    return true;
                }
            }
            return false;
            
        case UTAP::Constants::MULT:
            if (expr.get_size() == 2) {
                int left, right;
                if (evaluate_expression(expr[0], left) && evaluate_expression(expr[1], right)) {
                    result = left * right;
                    return true;
                }
            }
            return false;
            
        default:
            return false;
    }
}

bool TimedAutomaton::parse_clock_constraint_from_expr(const UTAP::Expression& expr, std::string& clock_name, 
                                                     std::string& op, int& value) {
    if (expr.empty()) {
        return false;
    }
    
    auto kind = expr.get_kind();
    
    // Handle logical operators recursively (AND, OR)
    if (kind == UTAP::Constants::AND || kind == UTAP::Constants::OR) {
        // Try to find any clock constraint in the expression tree
        for (size_t i = 0; i < expr.get_size(); ++i) {
            if (parse_clock_constraint_from_expr(expr[i], clock_name, op, value)) {
                return true; // Found a clock constraint
            }
        }
        return false; // No clock constraints found
    }
    
    // Handle potential comma operator
    if (kind == 12 || kind == 11 || kind == 13 || kind == 14 || kind == 15) {
        DEV_PRINT("     Found potential comma/sequence in clock constraint parsing (kind " << kind << ")" << std::endl);
        for (size_t i = 0; i < expr.get_size(); ++i) {
            if (parse_clock_constraint_from_expr(expr[i], clock_name, op, value)) {
                return true;
            }
        }
        return false;
    }
    
    // Handle NOT expressions
    if (kind == UTAP::Constants::NOT && expr.get_size() == 1) {
        // For NOT expressions, try to parse the inner expression
        // Note: This might need special handling depending on the logic
        return parse_clock_constraint_from_expr(expr[0], clock_name, op, value);
    }
    
    // Handle comparison operators
    if (expr.get_size() == 2) {
        // Map UTAP comparison operators to strings
        switch (kind) {
            case UTAP::Constants::GE:
                op = ">=";
                break;
            case UTAP::Constants::GT:
                op = ">";
                break;
            case UTAP::Constants::LE:
                op = "<=";
                break;
            case UTAP::Constants::LT:
                op = "<";
                break;
            case UTAP::Constants::EQ:
                op = "==";
                break;
            default:
                return false; // Not a comparison operator
        }
        
        // Get left side - try both orders in case of different expression structures
        const auto& left = expr[0];
        const auto& right = expr[1];
        
        // Case 1: left side is a clock identifier
        if (left.get_kind() == UTAP::Constants::IDENTIFIER) {
            std::string identifier_name = left.get_symbol().get_name();
            
            // Check if this identifier is a clock (we need to check both global and template clocks)
            if (clock_map_.find(identifier_name) != clock_map_.end()) {
                clock_name = identifier_name;
                
                // Evaluate right side
                if (evaluate_expression(right, value)) {
                    DEV_PRINT("     Found clock constraint: " << clock_name << " " << op << " " << value << std::endl);
                    return true;
                }
            }
        }
        
        // Case 2: right side is a clock identifier (for expressions like "5 < x")
        if (right.get_kind() == UTAP::Constants::IDENTIFIER) {
            std::string identifier_name = right.get_symbol().get_name();
            
            // Check if this identifier is a clock
            if (clock_map_.find(identifier_name) != clock_map_.end()) {
                clock_name = identifier_name;
                
                // Evaluate left side and flip the operator
                int left_value;
                if (evaluate_expression(left, left_value)) {
                    value = left_value;
                    
                    // Flip the operator for expressions like "5 < x" -> "x > 5"
                    if (op == ">=") op = "<=";
                    else if (op == ">") op = "<";
                    else if (op == "<=") op = ">=";
                    else if (op == "<") op = ">";
                    // == stays the same
                    
                    DEV_PRINT("     Found flipped clock constraint: " << clock_name << " " << op << " " << value << std::endl);
                    return true;
                }
            }
        }
    }
    
    return false;
}

bool TimedAutomaton::parse_clock_reset_from_expr(const UTAP::Expression& expr, std::string& clock_name, int& value) {
    if (expr.empty() || expr.get_size() != 2) {
        return false;
    }
    
    auto kind = expr.get_kind();
    
    // Check if this is an assignment
    if (kind != UTAP::Constants::ASSIGN) {
        return false;
    }
    
    // Get left side (should be a clock identifier)
    const auto& left = expr[0];
    if (left.get_kind() != UTAP::Constants::IDENTIFIER) {
        return false;
    }
    
    clock_name = left.get_symbol().get_name();
    
    // Get right side (should evaluate to an integer)
    const auto& right = expr[1];
    if (!evaluate_expression(right, value)) {
        return false;
    }
    
    return true;
}

bool TimedAutomaton::parse_variable_assignment_from_expr(const UTAP::Expression& expr, std::string& var_name, int& value) {
    if (expr.empty() || expr.get_size() != 2) {
        return false;
    }
    
    auto kind = expr.get_kind();
    
    // Check if this is an assignment
    if (kind != UTAP::Constants::ASSIGN) {
        return false;
    }
    
    // Get left side (should be a variable identifier)
    const auto& left = expr[0];
    if (left.get_kind() != UTAP::Constants::IDENTIFIER) {
        return false;
    }
    
    var_name = left.get_symbol().get_name();
    
    // Get right side (should evaluate to an integer)
    const auto& right = expr[1];
    if (!evaluate_expression(right, value)) {
        return false;
    }
    
    return true;
}

bool TimedAutomaton::parse_variable_constraint_from_expr(const UTAP::Expression& expr, std::string& var_name, 
                                                        std::string& op, int& value) {
    if (expr.empty()) {
        return false;
    }
    
    auto kind = expr.get_kind();
    
    // Handle logical operators recursively (AND, OR)
    if (kind == UTAP::Constants::AND || kind == UTAP::Constants::OR) {
        // Try to find any variable constraint in the expression tree
        for (size_t i = 0; i < expr.get_size(); ++i) {
            if (parse_variable_constraint_from_expr(expr[i], var_name, op, value)) {
                return true; // Found a variable constraint
            }
        }
        return false; // No variable constraints found
    }
    
    // Handle potential comma operator
    if (kind == 12 || kind == 11 || kind == 13 || kind == 14 || kind == 15) {
        DEV_PRINT("     Found potential comma/sequence in variable constraint parsing (kind " << kind << ")" << std::endl);
        for (size_t i = 0; i < expr.get_size(); ++i) {
            if (parse_variable_constraint_from_expr(expr[i], var_name, op, value)) {
                return true;
            }
        }
        return false;
    }
    
    // Handle NOT expressions
    if (kind == UTAP::Constants::NOT && expr.get_size() == 1) {
        // For NOT expressions, try to parse the inner expression
        // Note: This might need special handling depending on the logic
        return parse_variable_constraint_from_expr(expr[0], var_name, op, value);
    }
    
    // Handle comparison operators
    if (expr.get_size() == 2) {
        // Map UTAP comparison operators to strings
        switch (kind) {
            case UTAP::Constants::GE:
                op = ">=";
                break;
            case UTAP::Constants::GT:
                op = ">";
                break;
            case UTAP::Constants::LE:
                op = "<=";
                break;
            case UTAP::Constants::LT:
                op = "<";
                break;
            case UTAP::Constants::EQ:
                op = "==";
                break;
            case UTAP::Constants::NEQ:
                op = "!=";
                break;
            default:
                return false; // Not a comparison operator
        }
        
        // Get left and right sides
        const auto& left = expr[0];
        const auto& right = expr[1];
        
        // Case 1: left side is a variable identifier
        if (left.get_kind() == UTAP::Constants::IDENTIFIER) {
            std::string identifier_name = left.get_symbol().get_name();
            
            // Check if this identifier is a variable or constant (not a clock)
            if ((variables_.find(identifier_name) != variables_.end() || 
                 constants_.find(identifier_name) != constants_.end()) &&
                clock_map_.find(identifier_name) == clock_map_.end()) {
                
                var_name = identifier_name;
                
                // Evaluate right side
                if (evaluate_expression(right, value)) {
                    DEV_PRINT("     Found variable constraint: " << var_name << " " << op << " " << value << std::endl);
                    return true;
                }
            }
        }
        
        // Case 2: right side is a variable identifier (for expressions like "5 == id")
        if (right.get_kind() == UTAP::Constants::IDENTIFIER) {
            std::string identifier_name = right.get_symbol().get_name();
            
            // Check if this identifier is a variable or constant (not a clock)
            if ((variables_.find(identifier_name) != variables_.end() || 
                 constants_.find(identifier_name) != constants_.end()) &&
                clock_map_.find(identifier_name) == clock_map_.end()) {
                
                var_name = identifier_name;
                
                // Evaluate left side and flip the operator
                int left_value;
                if (evaluate_expression(left, left_value)) {
                    value = left_value;
                    
                    // Flip the operator for expressions like "5 == id" -> "id == 5"
                    if (op == ">=") op = "<=";
                    else if (op == ">") op = "<";
                    else if (op == "<=") op = ">=";
                    else if (op == "<") op = ">";
                    // == and != stay the same
                    
                    DEV_PRINT("     Found flipped variable constraint: " << var_name << " " << op << " " << value << std::endl);
                    return true;
                }
            }
        }
    }
    
    return false;
}

bool TimedAutomaton::evaluate_variable_constraint(const std::string& var_name, const std::string& op, int value) const {
    // Check if variable exists in our variable map
    if (variables_.find(var_name) == variables_.end() && constants_.find(var_name) == constants_.end()) {
        DEV_PRINT("     Variable " << var_name << " not found in variables or constants" << std::endl);
        return false;
    }
    
    int var_value;
    if (variables_.find(var_name) != variables_.end()) {
        var_value = variables_.at(var_name);
    } else {
        var_value = constants_.at(var_name);
    }
    
    DEV_PRINT("     Evaluating variable constraint: " << var_name << " (" << var_value << ") " << op << " " << value << std::endl);
    
    // Evaluate the constraint
    if (op == "==") {
        return var_value == value;
    } else if (op == "!=") {
        return var_value != value;
    } else if (op == "<") {
        return var_value < value;
    } else if (op == "<=") {
        return var_value <= value;
    } else if (op == ">") {
        return var_value > value;
    } else if (op == ">=") {
        return var_value >= value;
    }
    
    DEV_PRINT("     Unknown operator: " << op << std::endl);
    return false;
}

bool TimedAutomaton::extract_all_constraints(const UTAP::Expression& expr, std::vector<Constraint>& constraints) {
    if (expr.empty()) {
        return false;
    }
    
    auto kind = expr.get_kind();
    
    // Handle logical operators recursively
    if (kind == UTAP::Constants::AND || kind == UTAP::Constants::OR) {
        bool found_any = false;
        for (size_t i = 0; i < expr.get_size(); ++i) {
            if (extract_all_constraints(expr[i], constraints)) {
                found_any = true;
            }
        }
        return found_any;
    }
    
    // Handle potential comma operator - try different common representations
    // In some UTAP versions, comma might be represented differently
    if (kind == 12 || kind == 11 || kind == 13 || kind == 14 || kind == 15) { // Try various potential comma kinds
        DEV_PRINT("   Found potential comma/sequence operator (kind " << kind << ")" << std::endl);
        bool found_any = false;
        for (size_t i = 0; i < expr.get_size(); ++i) {
            if (extract_all_constraints(expr[i], constraints)) {
                found_any = true;
            }
        }
        return found_any;
    }
    
    // Handle comparison operators
    if (expr.get_size() == 2) {
        std::string op;
        switch (kind) {
            case UTAP::Constants::GE: op = ">="; break;
            case UTAP::Constants::GT: op = ">"; break;
            case UTAP::Constants::LE: op = "<="; break;
            case UTAP::Constants::LT: op = "<"; break;
            case UTAP::Constants::EQ: op = "=="; break;
            case UTAP::Constants::NEQ: op = "!="; break;
            default: return false;
        }
        
        const auto& left = expr[0];
        const auto& right = expr[1];
        
        // Check if left side is an identifier
        if (left.get_kind() == UTAP::Constants::IDENTIFIER) {
            std::string name = left.get_symbol().get_name();
            int value;
            
            if (evaluate_expression(right, value)) {
                Constraint constraint;
                constraint.name = name;
                constraint.op = op;
                constraint.value = value;
                // Check if this is a clock
                constraint.is_clock = (clock_map_.find(name) != clock_map_.end());
                
                constraints.push_back(constraint);
                DEV_PRINT("     Extracted constraint: " << name << " " << op << " " << value 
                         << " (is_clock: " << constraint.is_clock << ")" << std::endl);
                return true;
            }
        }
        
        // Check if right side is an identifier
        if (right.get_kind() == UTAP::Constants::IDENTIFIER) {
            std::string name = right.get_symbol().get_name();
            int value;
            
            if (evaluate_expression(left, value)) {
                // Flip the operator
                if (op == ">=") op = "<=";
                else if (op == ">") op = "<";
                else if (op == "<=") op = ">=";
                else if (op == "<") op = ">";
                
                Constraint constraint;
                constraint.name = name;
                constraint.op = op;
                constraint.value = value;
                // Check if this is a clock
                constraint.is_clock = (clock_map_.find(name) != clock_map_.end());
                
                constraints.push_back(constraint);
                DEV_PRINT("     Extracted flipped constraint: " << name << " " << op << " " << value 
                         << " (is_clock: " << constraint.is_clock << ")" << std::endl);
                return true;
            }
        }
    }
    
    return false;
}

bool TimedAutomaton::evaluate_complex_guard(const UTAP::Expression& expr) {
    if (expr.empty()) {
        return true; // Empty guard is always true
    }
    
    auto kind = expr.get_kind();
    
    // Handle logical operators
    if (kind == UTAP::Constants::AND) {
        // All operands must be true
        for (size_t i = 0; i < expr.get_size(); ++i) {
            if (!evaluate_complex_guard(expr[i])) {
                return false;
            }
        }
        return true;
    }
    
    if (kind == UTAP::Constants::OR) {
        // At least one operand must be true
        for (size_t i = 0; i < expr.get_size(); ++i) {
            if (evaluate_complex_guard(expr[i])) {
                return true;
            }
        }
        return false;
    }
    
    if (kind == UTAP::Constants::NOT && expr.get_size() == 1) {
        return !evaluate_complex_guard(expr[0]);
    }
    
    // Handle individual constraints
    if (expr.get_size() == 2) {
        std::string op;
        switch (kind) {
            case UTAP::Constants::GE: op = ">="; break;
            case UTAP::Constants::GT: op = ">"; break;
            case UTAP::Constants::LE: op = "<="; break;
            case UTAP::Constants::LT: op = "<"; break;
            case UTAP::Constants::EQ: op = "=="; break;
            case UTAP::Constants::NEQ: op = "!="; break;
            default: return true; // Unknown expressions are assumed true
        }
        
        const auto& left = expr[0];
        const auto& right = expr[1];
        
        // Try to evaluate as variable constraint
        if (left.get_kind() == UTAP::Constants::IDENTIFIER) {
            std::string name = left.get_symbol().get_name();
            
            // Check if it's a variable (not a clock)
            if ((variables_.find(name) != variables_.end() || constants_.find(name) != constants_.end()) &&
                clock_map_.find(name) == clock_map_.end()) {
                
                int value;
                if (evaluate_expression(right, value)) {
                    return evaluate_variable_constraint(name, op, value);
                }
            }
        }
        
        // Try right side as identifier too
        if (right.get_kind() == UTAP::Constants::IDENTIFIER) {
            std::string name = right.get_symbol().get_name();
            
            if ((variables_.find(name) != variables_.end() || constants_.find(name) != constants_.end()) &&
                clock_map_.find(name) == clock_map_.end()) {
                
                int value;
                if (evaluate_expression(left, value)) {
                    // Flip operator
                    if (op == ">=") op = "<=";
                    else if (op == ">") op = "<";
                    else if (op == "<=") op = ">=";
                    else if (op == "<") op = ">";
                    
                    return evaluate_variable_constraint(name, op, value);
                }
            }
        }
    }
    
    // For expressions we can't evaluate (like clock constraints), assume they're handled elsewhere
    return true;
}

bool TimedAutomaton::parse_synchronization_from_expr(const UTAP::Expression& expr, std::string& channel, bool& is_sender) {
    if (expr.empty()) {
        return false;
    }
    
    // Handle synchronization by parsing the string representation
    std::string sync_str = expr.str();
    if (!sync_str.empty()) {
        if (sync_str.back() == TA_CONFIG.sender_suffix) {
            channel = sync_str.substr(0, sync_str.length() - 1);
            is_sender = true;
            return true;
        } else if (sync_str.back() == TA_CONFIG.receiver_suffix) {
            channel = sync_str.substr(0, sync_str.length() - 1);
            is_sender = false;
            return true;
        }
    }
    
    return false;
}

void TimedAutomaton::add_dbm_constraint(const std::string& clock_name, const std::string& op, int value,
                                       std::unordered_map<std::string, cindex_t>& clock_map,
                                       int location_id, size_t transition_idx) {
    
    
    
    // Find or create clock index
    cindex_t clock_idx;
    if (clock_map.find(clock_name) == clock_map.end()) {
        // Assign next available index, ensuring we don't exceed dimension
        clock_idx = clock_map.size() + 1; // +1 because index 0 is reserved for reference clock
        
        // Validate that we don't exceed dimension
        if (clock_idx >= dimension_) {
            std::cerr << "ERROR: Clock index " << clock_idx << " would exceed dimension " << dimension_ 
                      << " for clock " << clock_name << std::endl;
            std::cerr << "Current clock_map size: " << clock_map.size() << std::endl;
            std::cerr << "This suggests too many clocks are being discovered during constraint parsing" << std::endl;
            return;
        }
        
        clock_map[clock_name] = clock_idx;
        DEV_PRINT("     Assigned new clock index " << clock_idx << " to " << clock_name << std::endl);
    } else {
        clock_idx = clock_map[clock_name];
    }
    
    // Convert operator and value to DBM constraint
    if (op == ">=") {
        // x >= c becomes 0 - x <= -c
        if (location_id >= 0) {
            add_invariant(location_id, 0, clock_idx, -value, dbm_WEAK);
        } else if (transition_idx != static_cast<size_t>(-1)) {
            add_guard(transition_idx, 0, clock_idx, -value, dbm_WEAK);
        }
    } else if (op == ">") {
        // x > c becomes 0 - x <= -c with strict bound
        if (location_id >= 0) {
            add_invariant(location_id, 0, clock_idx, -value, dbm_STRICT);
        } else if (transition_idx != static_cast<size_t>(-1)) {
            add_guard(transition_idx, 0, clock_idx, -value, dbm_STRICT);
        }
    } else if (op == "<=") {
        // x <= c becomes x - 0 <= c
        if (location_id >= 0) {
            add_invariant(location_id, clock_idx, 0, value, dbm_WEAK);
        } else if (transition_idx != static_cast<size_t>(-1)) {
            add_guard(transition_idx, clock_idx, 0, value, dbm_WEAK);
        }
    } else if (op == "<") {
        // x < c becomes x - 0 < c
        if (location_id >= 0) {
            add_invariant(location_id, clock_idx, 0, value, dbm_STRICT);
        } else if (transition_idx != static_cast<size_t>(-1)) {
            add_guard(transition_idx, clock_idx, 0, value, dbm_STRICT);
        }
    } else if (op == "==") {
        // x == c becomes x - 0 <= c AND 0 - x <= -c
        if (location_id >= 0) {
            add_invariant(location_id, clock_idx, 0, value, dbm_WEAK);
            add_invariant(location_id, 0, clock_idx, -value, dbm_WEAK);
        } else if (transition_idx != static_cast<size_t>(-1)) {
            add_guard(transition_idx, clock_idx, 0, value, dbm_WEAK);
            add_guard(transition_idx, 0, clock_idx, -value, dbm_WEAK);
        }
    }
}

// Function implementations moved from header

void TimedAutomaton::add_location(int id, const std::string& name) {
    locations_.emplace_back(id, name);
}

void TimedAutomaton::add_invariant(int location_id, cindex_t i, cindex_t j, int32_t bound, strictness_t strict) {
    for (auto& loc : locations_) {
        if (loc.id == location_id) {
            constraint_t inv = {i, j, dbm_bound2raw(bound, strict)};
            loc.invariants.push_back(inv);
            break;
        }
    }
}

void TimedAutomaton::add_transition(int from, int to, const std::string& action) {
    if (action == TA_CONFIG.default_action_name || action.empty()) 
            transitions_.emplace_back(from, to, TA_CONFIG.tau_action_name);
    else 
            transitions_.emplace_back(from, to, action);
    int transition_idx = transitions_.size() - 1;
    outgoing_transitions_[from].push_back(transition_idx);
}

void TimedAutomaton::add_guard(size_t transition_idx, cindex_t i, cindex_t j, int32_t bound, strictness_t strict) {
    if (transition_idx < transitions_.size()) {
        constraint_t guard = {i, j, dbm_bound2raw(bound, strict)};
        transitions_[transition_idx].guards.push_back(guard);
    }
}

void TimedAutomaton::add_reset(size_t transition_idx, cindex_t clock) {
    if (transition_idx < transitions_.size()) {
        transitions_[transition_idx].resets.push_back(clock);
    }
}

void TimedAutomaton::add_synchronization(size_t transition_idx, const std::string& channel, bool is_sender) {
    if (transition_idx < transitions_.size()) {
        transitions_[transition_idx].channel = channel;
        transitions_[transition_idx].is_sender = is_sender;
        transitions_[transition_idx].is_receiver = !is_sender; // Set the receiver flag
        
        // Add to appropriate synchronization map
        if (is_sender) {
            sender_transitions_[channel].push_back(transition_idx);
        } else {
            receiver_transitions_[channel].push_back(transition_idx);
        }
    }
}

void TimedAutomaton::add_channel(const std::string& channel_name) {
    channels_.insert(channel_name);
}

const std::unordered_set<std::string>& TimedAutomaton::get_channels() const {
    return channels_;
}

const std::vector<Transition>& TimedAutomaton::get_transitions() const {
    return transitions_;
}

std::vector<std::pair<int, int>> TimedAutomaton::find_synchronized_pairs(const std::string& channel) const {
    std::vector<std::pair<int, int>> pairs;
    
    auto sender_it = sender_transitions_.find(channel);
    auto receiver_it = receiver_transitions_.find(channel);
    
    if (sender_it != sender_transitions_.end() && receiver_it != receiver_transitions_.end()) {
        for (int sender_idx : sender_it->second) {
            for (int receiver_idx : receiver_it->second) {
                pairs.emplace_back(sender_idx, receiver_idx);
            }
        }
    }
    
    return pairs;
}

void TimedAutomaton::construct_zone_graph(int initial_location, const std::vector<raw_t>& initial_zone,const size_t MAX_STATES, bool force ) {
    if (constructed_ && !force) {
        DEV_PRINT("Zone graph already constructed. Use force=true to rebuild." << std::endl);
        return;
    }
    // Clear previous exploration state
    states_.clear();
    state_map_.clear();
    zone_transitions_.clear();
    waiting_list_ = std::queue<int>();
    
    // Create initial state
    add_state(initial_location, initial_zone);
    
    
    
    // Explore using BFS
    while (!waiting_list_.empty() && states_.size() < MAX_STATES) {
        int current_state_id = waiting_list_.front();
        waiting_list_.pop();
        
        explore_state(current_state_id);
        
        // Progress reporting
        if (states_.size() % 100 == 0) {
            DEV_PRINT("Explored " << states_.size() << " states, " 
                     << waiting_list_.size() << " states in queue" << std::endl);
        }
    }
    
    if (states_.size() >= MAX_STATES) {
        DEV_PRINT("Warning: Reached maximum state limit (" << MAX_STATES 
                 << "). Zone graph exploration stopped." << std::endl);
    }
    constructed_ = true;
}

std::vector<raw_t> TimedAutomaton::time_elapse(const std::vector<raw_t>& zone) const {
    // Validate zone size
    size_t expected_size = dimension_ * dimension_;
    if (zone.size() != expected_size) {
        std::cerr << "ERROR: Zone size mismatch in time_elapse! Expected " << expected_size 
                  << ", got " << zone.size() << std::endl;
        return {}; // Return empty zone
    }
    
    std::vector<raw_t> result = zone;
    dbm_up(result.data(), dimension_);
    
    // Apply proper UDBM k-extrapolation to prevent infinite zone refinement
    // Set up maximum constants for each clock
    std::vector<int32_t> max_constants(dimension_);
    max_constants[0] = 0; // Reference clock is always 0
    
    // Use a reasonable maximum constant for all clocks
    // In practice, this should be computed from the automaton's constraints
    const int32_t MAX_CONSTANT = 10;
    for (cindex_t i = 1; i < dimension_; ++i) {
        max_constants[i] = MAX_CONSTANT;
    }
    
    // Apply classical k-extrapolation (formerly k-normalization)
    dbm_extrapolateMaxBounds(result.data(), dimension_, max_constants.data());
    
    return result;
}

std::vector<raw_t> TimedAutomaton::apply_invariants(const std::vector<raw_t>& zone, int location_id) const {
    std::vector<raw_t> result = zone;
    
    for (const auto& loc : locations_) {
        if (loc.id == location_id) {
            for (const auto& inv : loc.invariants) {
                dbm_constrain1(result.data(), dimension_, inv.i, inv.j, inv.value);
            }
            break;
        }
    }
    
    // Close the DBM and check consistency
    if (!dbm_close(result.data(), dimension_)) {
        // Return empty zone if inconsistent
        result.clear();
    }
    
    return result;
}

bool TimedAutomaton::is_transition_enabled(const std::vector<raw_t>& zone, const Transition& transition) const {
    // Validate zone size
    size_t expected_size = dimension_ * dimension_;
    if (zone.size() != expected_size) {
        std::cerr << "ERROR: Zone size mismatch! Expected " << expected_size 
                  << ", got " << zone.size() << std::endl;
        return false;
    }
    
    std::vector<raw_t> test_zone = zone;
    
    for (const auto& guard : transition.guards) {
        // Validate guard indices
        if (guard.i >= dimension_ || guard.j >= dimension_) {
            std::cerr << "ERROR: Guard index out of bounds! i=" << guard.i 
                      << ", j=" << guard.j << ", dimension=" << dimension_ << std::endl;
            return false;
        }
        dbm_constrain1(test_zone.data(), dimension_, guard.i, guard.j, guard.value);
    }
    
    return dbm_close(test_zone.data(), dimension_) && !dbm_isEmpty(test_zone.data(), dimension_);
}

std::vector<raw_t> TimedAutomaton::apply_transition(const std::vector<raw_t>& zone, const Transition& transition) const {
    // Validate zone size
    size_t expected_size = dimension_ * dimension_;
    if (zone.size() != expected_size) {
        std::cerr << "ERROR: Zone size mismatch in apply_transition! Expected " << expected_size 
                  << ", got " << zone.size() << std::endl;
        return {}; // Return empty zone
    }
    
    std::vector<raw_t> result = zone;
    
    // Apply guards
    for (const auto& guard : transition.guards) {
        // Validate guard indices
        if (guard.i >= dimension_ || guard.j >= dimension_) {
            std::cerr << "ERROR: Guard index out of bounds in apply_transition! i=" << guard.i 
                      << ", j=" << guard.j << ", dimension=" << dimension_ << std::endl;
            return {}; // Return empty zone
        }
        dbm_constrain1(result.data(), dimension_, guard.i, guard.j, guard.value);
    }
    
    // Apply resets
    for (cindex_t reset_clock : transition.resets) {
        if (reset_clock >= dimension_) {
            std::cerr << "ERROR: Reset clock index out of bounds! clock=" << reset_clock 
                      << ", dimension=" << dimension_ << std::endl;
            return {}; // Return empty zone
        }
        dbm_updateValue(result.data(), dimension_, reset_clock, 0);
    }
    
    // Close the DBM
    if (!dbm_close(result.data(), dimension_)) {
        result.clear();  // Return empty zone if inconsistent
    }
    
    return result;
}

void TimedAutomaton::print_statistics() const {
    std::cout << "Zone Graph Statistics:\n";
    std::cout << "======================\n";
    std::cout << "Number of states: " << states_.size() << "\n";
    std::cout << "Number of locations: " << locations_.size() << "\n";
    std::cout << "Number of transitions: " << transitions_.size() << "\n";
    std::cout << "Dimension: " << dimension_ << "\n\n";
    
    // Count zone graph transitions
    int total_zone_transitions = 0;
    for (const auto& trans_list : zone_transitions_) {
        total_zone_transitions += trans_list.size();
    }
    std::cout << "Number of zone graph transitions: " << total_zone_transitions << "\n\n";
}

void TimedAutomaton::print_state(size_t state_id) const {
    if (state_id < states_.size()) {
        const auto& state = *states_[state_id];
        std::cout << "State " << state_id << " (Location " << state.location_id << "):\n";
        dbm_print(stdout, state.zone.data(), state.dimension);
        std::cout << "\n";
    }
}

void TimedAutomaton::print_all_states() const {
    for (size_t i = 0; i < states_.size(); ++i) {
        print_state(i);
    }
}

size_t TimedAutomaton::get_num_states() const { 
    return states_.size(); 
}

const std::vector<int>& TimedAutomaton::get_successors(size_t state_id) const {
    static const std::vector<int> empty;
    return (state_id < zone_transitions_.size()) ? zone_transitions_[state_id] : empty;
}

int TimedAutomaton::add_state(int location_id, const std::vector<raw_t>& zone) {
    // Validate zone size
    size_t expected_size = dimension_ * dimension_;
    if (zone.size() != expected_size) {
        std::cerr << "ERROR: Zone size mismatch in add_state! Expected " << expected_size 
                  << ", got " << zone.size() << std::endl;
        std::cerr << "Dimension: " << dimension_ << std::endl;
        return -1; // Invalid state
    }
    
    // Create the state
    auto new_state = std::make_unique<ZoneState>(location_id, zone, dimension_);
    
    // Check if state already exists
    auto it = state_map_.find(*new_state);
    if (it != state_map_.end()) {
        return it->second;  // Return existing state ID
    }
    
    // Add new state
    int state_id = states_.size();
    state_map_[*new_state] = state_id;
    states_.push_back(std::move(new_state));
    zone_transitions_.emplace_back();
    waiting_list_.push(state_id);  // Only add new states to waiting list
    
    return state_id;
}

void TimedAutomaton::explore_state(int state_id) {
    const auto& current_state = *states_[state_id];
    
    // Apply time elapse first
    auto elapsed_zone = time_elapse(current_state.zone);
    auto invariant_zone = apply_invariants(elapsed_zone, current_state.location_id);
    
    if (invariant_zone.empty()) {
        return;  // Time elapse leads to empty zone
    }
    
    // Try all outgoing transitions from current location
    auto transition_it = outgoing_transitions_.find(current_state.location_id);
    if (transition_it != outgoing_transitions_.end()) {
        for (int transition_idx : transition_it->second) {
            const auto& transition = transitions_[transition_idx];
            
            // Check if transition is enabled
            if (is_transition_enabled(invariant_zone, transition)) {
                // Apply transition
                auto successor_zone = apply_transition(invariant_zone, transition);
                
                if (!successor_zone.empty()) {
                    // Apply invariants in target location
                    auto final_zone = apply_invariants(successor_zone, transition.to_location);
                    
                    if (!final_zone.empty()) {
                        // Add successor state
                        int successor_id = add_state(transition.to_location, final_zone);
                        zone_transitions_[state_id].push_back(successor_id);
                    }
                }
            }
        }
    }
}

void TimedAutomaton::construct_zone_graph() const {
    // Cast away const since we need to modify internal state for lazy initialization
    auto* mutable_this = const_cast<TimedAutomaton*>(this);
    
    if (constructed_) {
        return;  // Already constructed
    }
    
    // Create initial zone (all clocks = 0) using proper UDBM initialization
    std::vector<raw_t> initial_zone(dimension_ * dimension_);
    dbm_init(initial_zone.data(), dimension_); // Proper zero zone initialization
    
    // Construct with default parameters (location 0, zero zone)
    mutable_this->construct_zone_graph(TA_CONFIG.default_initial_location, initial_zone, 1000, true); // Force construction
}

const ZoneState* TimedAutomaton::get_zone_state(size_t state_id) const {
    if (state_id >= states_.size()) {
        return nullptr;
    }
    return states_[state_id].get();
}


std::vector<const Transition*> TimedAutomaton::get_outgoing_transitions(int location_id) const {
    std::vector<const Transition*> outgoing;
    
    for (const auto& transition : transitions_) {
        if (transition.from_location == location_id) {
            outgoing.push_back(&transition);
        }
    }
    
    return outgoing;
}



void TimedAutomaton::print_all_transitions() const {
    std::cout << "Transitions:\n";
    for (const auto& transition : transitions_) {
        std::cout << "  " << transition.from_location << " --(" << transition.action << ")--> " << transition.to_location << "\n";
    }
}

} // namespace rtwbs
