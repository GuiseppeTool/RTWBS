#ifndef TIMED_AUTOMATON_H
#define TIMED_AUTOMATON_H

#include <iostream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <memory>
#include <functional>
#include <string>

#include "macros.h"
#include "context.h"

#include "dbm/dbm.h"
#include "dbm/fed.h"
#include "dbm/constraints.h"
#include "dbm/print.h"

#include "configs.h"

// Forward declarations for UTAP types
namespace UTAP {
    class Document;
    class Expression;
    class Template;
}

namespace rtwbs {

/**
 * Represents a discrete location in a timed automaton
 */
struct Location {
    int id;
    std::string name;
    std::vector<constraint_t> invariants;  // Clock constraints that must hold in this location
    
    Location(int id, const std::string& name) : id(id), name(name) {}
    
    bool operator==(const Location& other) const { return id == other.id; }
};

/**
 * Represents a transition in a timed automaton
 */
struct Transition {
    int from_location;
    int to_location;
    std::string action;
    std::vector<constraint_t> guards;   // Clock constraints that must hold to take this transition
    std::vector<cindex_t> resets;       // Clocks to reset to 0 when taking this transition
    
    // Synchronization support
    std::string channel;                // Channel name for synchronization (empty if no sync)
    bool is_sender;                     // true for TA_CONFIG.sender_suffix
    bool is_receiver;                   // true for TA_CONFIG.receiver_suffix
    bool has_synchronization() const { return !channel.empty(); }
    
    Transition(int from, int to, const std::string& action) 
        : from_location(from), to_location(to), action(action), is_sender(false), is_receiver(false) {

    }
    bool includes(const Transition* other) const{
        return other->is_included(this);
    }
    bool is_included(const Transition* other) const {
        if (!other) return false;
        
        // Check if actions are compatible
        if (action != other->action) return false;
        
        // Check if this transition's guards are included in other's guards
        // For each guard in this transition, check if it's satisfied by other's guards
        for (const auto& guard : guards) {
            bool found_compatible = false;
            for (const auto& other_guard : other->guards) {
                // Check if guards affect same clock variables
                if (guard.i == other_guard.i && guard.j == other_guard.j) {
                    // Check if this guard is weaker or equal to other guard
                    // For now, simple comparison - this guard should be <= other guard
                    if (guard.value <= other_guard.value) {
                        found_compatible = true;
                        break;
                    }
                }
            }
            if (!found_compatible) {
                return false;
            }
        }
        return true;
    }
};

/**
 * Represents a state in the zone graph (location + zone)
 */
struct ZoneState {
    int location_id;
    std::vector<raw_t> zone;  // DBM representing the zone
    cindex_t dimension;
    size_t hash_value;
    
    ZoneState(int loc_id, const std::vector<raw_t>& zone_dbm, cindex_t dim) 
        : location_id(loc_id), zone(zone_dbm), dimension(dim) {
        compute_hash();
    }
    
    bool operator==(const ZoneState& other) const {
        if (location_id != other.location_id || dimension != other.dimension) {
            return false;
        }
        return dbm_areEqual(zone.data(), other.zone.data(), dimension);
    }

    // checks if one zone is within the other. NO ID CHECK
    bool operator<=(const ZoneState& other) const {
        if ( dimension != other.dimension) {
            return false;
        }
        relation_t rel = dbm_relation(zone.data(), other.zone.data(), dimension);
        return (rel == base_SUBSET || rel == base_EQUAL);
    }

private:
    void compute_hash() {
        std::hash<int> int_hasher;
        std::hash<raw_t> raw_hasher;
        
        hash_value = int_hasher(location_id);
        for (const auto& elem : zone) {
            hash_value ^= raw_hasher(elem) + 0x9e3779b9 + (hash_value << 6) + (hash_value >> 2);
        }
    }
};

/**
 * Hash function for ZoneState to use in unordered containers
 */
struct ZoneStateHash {
    size_t operator()(const ZoneState& state) const {
        return state.hash_value;
    }
};

/**
 * Timed Automaton class for modeling timed automata and constructing their zone graphs
 */
class TimedAutomaton {
private:
    cindex_t dimension_;
    std::vector<Location> locations_;
    std::vector<Transition> transitions_;
    std::unordered_map<int, std::vector<int>> outgoing_transitions_;  // location_id -> transition indices
    
    // Synchronization support
    std::unordered_set<std::string> channels_;                        // Set of declared channels
    std::unordered_map<std::string, std::vector<int>> sender_transitions_;    // channel -> transitions with TA_CONFIG.sender_suffix
    std::unordered_map<std::string, std::vector<int>> receiver_transitions_;  // channel -> transitions with TA_CONFIG.receiver_suffix
    
    // Zone graph exploration state
    std::vector<std::unique_ptr<ZoneState>> states_;
    std::unordered_map<ZoneState, int, ZoneStateHash> state_map_;
    std::vector<std::vector<int>> zone_transitions_;  // adjacency list: state_id -> list of successor state_ids
    std::queue<int> waiting_list_;
    bool constructed_;
    //std::vector<std::string> clocks_;
    //std::unordered_map<std::string, cindex_t> clock_map_;
    //std::unordered_map<std::string, double> constants_;
    //std::unordered_map<std::string, double> variables_;  // Non-constant variables like 'id'


    Context context_;
public:

    const std::unordered_map<std::string, double>& get_variables() const { return context_.variables_; }
    const std::unordered_map<std::string, cindex_t>& get_clock_map() const { return context_.clocks_; }
    const std::unordered_map<std::string, double>& get_constants() const { return context_.constants_; }

    void set_constant(const std::string& name, int value) {
        context_.constants_[name] = value;
    }
    explicit TimedAutomaton() : dimension_(0) {}
    explicit TimedAutomaton(const UTAP::Template& template_ref, int dimensions, 
         Context context):constructed_(false),context_(context) {
        build_from_template(template_ref, dimensions);
    }
    /**
     * Constructor
     * @param dim: Number of clocks + 1 (for reference clock)
     */
    explicit TimedAutomaton(cindex_t dim) : dimension_(dim) {}
    /**
     * Constructor that parses UPPAAL XML file and builds the automaton
     * @param fileName: UPPAAL file name
     */
    explicit TimedAutomaton(const std::string& fileName);

    /**
     * Constructor that parses UPPAAL XML and builds the automaton
     * @param xml_content: UPPAAL XML content as string
     */
    explicit TimedAutomaton(const char* xml_content);
    
    /**
     * Get the dimension (number of clocks + 1)
     */
    cindex_t get_dimension() const { return dimension_; }


    std::vector<const Transition*> get_outgoing_transitions(int location_id) const;

    
    
private:
    // Helper methods for XML parsing
    bool evaluate_expression(const UTAP::Expression& expr, int& result);
    bool parse_clock_constraint_from_expr(const UTAP::Expression& expr, std::string& clock_name, 
                                         std::string& op, int& value);
    bool parse_variable_constraint_from_expr(const UTAP::Expression& expr, std::string& var_name, 
                                           std::string& op, int& value);
    bool evaluate_variable_constraint(const std::string& var_name, const std::string& op, int value) const;
    
    // New functions for handling complex expressions
    struct Constraint {
        std::string name;
        std::string op;
        int value;
        bool is_clock;
    };
    
    bool extract_all_constraints(const UTAP::Expression& expr, std::vector<Constraint>& constraints);
    bool evaluate_complex_guard(const UTAP::Expression& expr);
    
    void add_dbm_constraint(const std::string& clock_name, const std::string& op, int value,
                           std::unordered_map<std::string, cindex_t>& clock_map,
                           int location_id = -1, size_t transition_idx = -1);
    bool parse_clock_reset_from_expr(const UTAP::Expression& expr, std::string& clock_name, int& value);
    bool parse_variable_assignment_from_expr(const UTAP::Expression& expr, std::string& var_name, int& value);
    bool parse_synchronization_from_expr(const UTAP::Expression& expr, std::string& channel, bool& is_sender);
    bool detect_and_expand_function_calls(const UTAP::Expression& expr, std::string& expanded_expr);
    //bool is_valid_clock(const std::string& clock_name) const;
    void build_from_utap_document(UTAP::Document& doc);
    
public:
    void build_from_template(const UTAP::Template& template_ref, int dimensions);
    /**
     * Add a location to the automaton
     */
    void add_location(int id, const std::string& name);
    
    /**
     * Add an invariant constraint to a location
     */
    void add_invariant(int location_id, cindex_t i, cindex_t j, int32_t bound, strictness_t strict);
    
    /**
     * Add a transition to the automaton
     */
    void add_transition(int from, int to, const std::string& action);
    
    /**
     * Add synchronization to a transition
     */
    void add_synchronization(size_t transition_idx, const std::string& channel, bool is_sender);
    
    /**
     * Add a channel declaration
     */
    void add_channel(const std::string& channel_name);
    
    /**
     * Get all declared channels
     */
    const std::unordered_set<std::string>& get_channels() const;
    
    /**
     * Get all transitions (for RTWBS analysis)
     */
    const std::vector<Transition>& get_transitions() const;
    
    /**
     * Find synchronized transition pairs for a given channel
     */
    std::vector<std::pair<int, int>> find_synchronized_pairs(const std::string& channel) const;
    
    /**
     * Add a guard constraint to a transition
     */
    void add_guard(size_t transition_idx, cindex_t i, cindex_t j, int32_t bound, strictness_t strict);
    
    /**
     * Add a clock reset to a transition
     */
    void add_reset(size_t transition_idx, cindex_t clock);
    
    /**
     * Construct the zone graph starting from initial location and zone
     */
    void construct_zone_graph(int initial_location, const std::vector<raw_t>& initial_zone, const size_t MAX_STATES = TA_CONFIG.max_states_limit, bool force = TA_CONFIG.force_construction);
    
    /**
     * Construct the zone graph with default initial state (location 0, zero zone) - used by RTWBS
     */
    void construct_zone_graph() const;

    /**
     * Apply time elapse to a zone (up operation)
     */
    std::vector<raw_t> time_elapse(const std::vector<raw_t>& zone) const;
    
    /**
     * Apply invariant constraints to a zone
     */
    std::vector<raw_t> apply_invariants(const std::vector<raw_t>& zone, int location_id) const;
    
    /**
     * Apply guard constraints and check if transition is enabled
     */
    bool is_transition_enabled(const std::vector<raw_t>& zone, const Transition& transition) const;
    
    /**
     * Apply transition: guards + resets
     */
    std::vector<raw_t> apply_transition(const std::vector<raw_t>& zone, const Transition& transition) const;
    
    /**
     * Print the zone graph statistics
     */
    void print_statistics() const;
    
    /**
     * Print a specific state
     */
    void print_state(size_t state_id) const;
    
    /**
     * Print all states
     */
    void print_all_states() const;


    void print_all_transitions() const;
    
    /**
     * Get the number of states in the zone graph
     */
    size_t get_num_states() const;
    
    /**
     * Get successors of a state
     */
    const std::vector<int>& get_successors(size_t state_id) const;
    
    /**
     * Get zone state by ID (needed for RTWBS state correspondence)
     */
    const ZoneState* get_zone_state(size_t state_id) const;
    
    const std::vector<std::unique_ptr<ZoneState>>& get_all_zone_states() const { return states_; }

private:
    /**
     * Add a new state to the zone graph
     */
    int add_state(int location_id, const std::vector<raw_t>& zone);
    
    /**
     * Explore all successors of a state
     */
    void explore_state(int state_id);
};

} // namespace rtwbs

#endif // TIMED_AUTOMATON_H
