#include "timedautomaton.h"
#include "utap/utap.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <fstream>

// DEV_PRINT macro for conditional debug output
#ifdef DEV_MODE
#define DEV_PRINT(msg) std::cout << msg
#else
#define DEV_PRINT(msg)
#endif

namespace rtwbs {

TimedAutomaton::TimedAutomaton(const char* xml_content) {
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

TimedAutomaton::TimedAutomaton(const std::string& fileName)
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

void TimedAutomaton::build_from_utap_document(UTAP::Document& doc) {
    DEV_PRINT("   Converting UTAP document to TimedAutomaton..." << std::endl);
    
    // Get the first template
    auto& templates = doc.get_templates();
    auto& template_ref = templates.front();
    
    // Create clock name to index mapping
    std::unordered_map<std::string, cindex_t> clock_map;
    
    // Extract clocks from global declarations
    auto& global_declarations = doc.get_globals();
    cindex_t clock_count = 0;
    
    DEV_PRINT("   Analyzing global declarations..." << std::endl);
    for (const auto& variable : global_declarations.variables) {
        if (variable.uid.get_type().is_clock()) {
            clock_count++;
            clock_map[variable.uid.get_name()] = clock_count;
            DEV_PRINT("   Found clock: " << variable.uid.get_name() << " -> index " << clock_count << std::endl);
        }
    }
    
    // Set dimension = reference clock (index 0) + number of clocks
    dimension_ = clock_count + 1;
    
    DEV_PRINT("   Creating automaton with dimension " << dimension_ << " (" << clock_count << " clocks + reference)" << std::endl);
    
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
                DEV_PRINT("     Parsed invariant: " << clock_name << " " << op << " " << value << std::endl);
                add_dbm_constraint(clock_name, op, value, clock_map, loc_int_id, -1);
                DEV_PRINT("     Added invariant constraint to location" << std::endl);
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
        std::string action = "tau";  // Default action name
        if (!edge.assign.empty()) {
            std::string assign_str = edge.assign.str();
            DEV_PRINT("     Assignment: " << assign_str << std::endl);
            
            // Try to parse as clock reset
            std::string reset_clock_name;
            int reset_value;
            
            if (parse_clock_reset_from_expr(edge.assign, reset_clock_name, reset_value)) {
                DEV_PRINT("     Parsed reset: " << reset_clock_name << " := " << reset_value << std::endl);
                
                // Check if this is actually a clock
                if (clock_map.find(reset_clock_name) != clock_map.end()) {
                    cindex_t clock_idx = clock_map[reset_clock_name];
                    
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
                    // Not a clock reset, use assignment as action name
                    action = assign_str;
                    add_transition(source_int, target_int, action);
                }
            } else {
                // Use assignment as action name
                action = assign_str;
                add_transition(source_int, target_int, action);
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
            
            std::string clock_name;
            std::string op;
            int value;
            
            if (parse_clock_constraint_from_expr(edge.guard, clock_name, op, value)) {
                DEV_PRINT("     Parsed constraint: " << clock_name << " " << op << " " << value << std::endl);
                add_dbm_constraint(clock_name, op, value, clock_map, -1, i);
                DEV_PRINT("     Added guard constraint to transition" << std::endl);
            } else {
                DEV_PRINT("     Failed to parse guard constraint: " << guard_str << std::endl);
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
    
    // Handle AND expressions (like "1 && x <= 5")
    if (kind == UTAP::Constants::AND && expr.get_size() == 2) {
        // Try to parse each operand
        std::string left_clock, left_op, right_clock, right_op;
        int left_value, right_value;
        
        bool left_parsed = parse_clock_constraint_from_expr(expr[0], left_clock, left_op, left_value);
        bool right_parsed = parse_clock_constraint_from_expr(expr[1], right_clock, right_op, right_value);
        
        // If right operand is a clock constraint, use it
        if (right_parsed) {
            clock_name = right_clock;
            op = right_op;
            value = right_value;
            return true;
        }
        // If left operand is a clock constraint, use it
        if (left_parsed) {
            clock_name = left_clock;
            op = left_op;
            value = left_value;
            return true;
        }
        
        return false;
    }
    
    // Handle simple binary constraints
    if (expr.get_size() != 2) {
        return false;
    }
    
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

bool TimedAutomaton::parse_synchronization_from_expr(const UTAP::Expression& expr, std::string& channel, bool& is_sender) {
    if (expr.empty()) {
        return false;
    }
    
    // Handle synchronization by parsing the string representation
    std::string sync_str = expr.str();
    if (!sync_str.empty()) {
        if (sync_str.back() == '!') {
            channel = sync_str.substr(0, sync_str.length() - 1);
            is_sender = true;
            return true;
        } else if (sync_str.back() == '?') {
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
        clock_idx = clock_map.size() + 1; // +1 because index 0 is reserved for reference clock
        clock_map[clock_name] = clock_idx;
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

void TimedAutomaton::construct_zone_graph(int initial_location, const std::vector<raw_t>& initial_zone,const size_t MAX_STATES ) {
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
            std::cout << "Explored " << states_.size() << " states, " 
                     << waiting_list_.size() << " states in queue" << std::endl;
        }
    }
    
    if (states_.size() >= MAX_STATES) {
        std::cout << "Warning: Reached maximum state limit (" << MAX_STATES 
                 << "). Zone graph exploration stopped." << std::endl;
    }
}

std::vector<raw_t> TimedAutomaton::time_elapse(const std::vector<raw_t>& zone) const {
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
    std::vector<raw_t> test_zone = zone;
    
    for (const auto& guard : transition.guards) {
        dbm_constrain1(test_zone.data(), dimension_, guard.i, guard.j, guard.value);
    }
    
    return dbm_close(test_zone.data(), dimension_) && !dbm_isEmpty(test_zone.data(), dimension_);
}

std::vector<raw_t> TimedAutomaton::apply_transition(const std::vector<raw_t>& zone, const Transition& transition) const {
    std::vector<raw_t> result = zone;
    
    // Apply guards
    for (const auto& guard : transition.guards) {
        dbm_constrain1(result.data(), dimension_, guard.i, guard.j, guard.value);
    }
    
    // Apply resets
    for (cindex_t reset_clock : transition.resets) {
        dbm_updateValue(result.data(), dimension_, reset_clock, 0);
    }
    
    // Close the DBM
    if (!dbm_close(result.data(), dimension_)) {
        result.clear();  // Return empty zone if inconsistent
    }
    
    return result;
}

void TimedAutomaton::print_statistics() const {
    DEV_PRINT("Zone Graph Statistics:\n");
    DEV_PRINT("======================\n");
    DEV_PRINT("Number of states: " << states_.size() << "\n");
    DEV_PRINT("Number of locations: " << locations_.size() << "\n");
    DEV_PRINT("Number of transitions: " << transitions_.size() << "\n");
    DEV_PRINT("Dimension: " << dimension_ << "\n\n");
    
    // Count zone graph transitions
    int total_zone_transitions = 0;
    for (const auto& trans_list : zone_transitions_) {
        total_zone_transitions += trans_list.size();
    }
    DEV_PRINT("Number of zone graph transitions: " << total_zone_transitions << "\n\n");
}

void TimedAutomaton::print_state(size_t state_id) const {
    if (state_id < states_.size()) {
        const auto& state = *states_[state_id];
        DEV_PRINT("State " << state_id << " (Location " << state.location_id << "):\n");
        dbm_print(stdout, state.zone.data(), state.dimension);
        DEV_PRINT("\n");
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

}  // namespace rtwbs
