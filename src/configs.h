#ifndef CONFIGS_H
#define CONFIGS_H

#include <string>
#include <cstddef>
#include <iostream>

namespace rtwbs {

/**
 * Configuration class for RTWBS and Timed Automata settings
 * 
 * This class provides a centralized configuration system that can be:
 * 1. Modified at runtime through code
 * 2. Thread-safe (using singleton pattern)
 * 3. Easily extended with new configuration options
 * 4. Type-safe and well-documented
 */
class Config {
public:
    // =================================================================
    // Timed Automaton Configuration
    // =================================================================
    
    struct TimedAutomatonConfig {
        // Action and transition settings
        std::string default_action_name = "";           // Default action name for unlabeled transitions
        std::string tau_action_name = "tau";               // Internal/silent action representation
        std::string empty_action_name = "";                // Alternative representation for internal actions
        
        // Zone graph construction limits
        size_t max_states_default = 1000;                  // Default maximum states in zone graph
        size_t max_states_limit = 100000;                  // Hard limit for safety
        int default_initial_location = 0;                  // Default initial location ID
        
        // Synchronization settings
        char sender_suffix = '!';                          // Character marking sender actions
        char receiver_suffix = '?';                        // Character marking receiver actions
        
        // Debug and output settings
        bool enable_debug_output = false;                  // Enable debug prints during construction
        bool enable_warnings = true;                       // Enable warning messages
        bool force_construction = false;                   // Force zone graph reconstruction
    };
    
    // =================================================================
    // RTWBS Configuration
    // =================================================================
    
    struct RTWBSConfig {

    };
    
    

private:
    TimedAutomatonConfig timed_automaton_config_;
    RTWBSConfig rtwbs_config_;
    
    
    // Singleton pattern
    Config() = default;
    
public:
    // Delete copy constructor and assignment operator
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    
    /**
     * Get the singleton instance
     */
    static Config& instance() {
        static Config instance;
        return instance;
    }
    
    // =================================================================
    // Accessors for configuration sections
    // =================================================================
    
    TimedAutomatonConfig& timed_automaton() { return timed_automaton_config_; }
    const TimedAutomatonConfig& timed_automaton() const { return timed_automaton_config_; }
    
    RTWBSConfig& rtwbs() { return rtwbs_config_; }
    const RTWBSConfig& rtwbs() const { return rtwbs_config_; }
    
    // =================================================================
    // Convenience methods for common configurations
    // =================================================================
    

    
    // =================================================================
    // Utility methods for checking τ-transitions
    // =================================================================
    
    // =================================================================
    // Configuration printing for debugging
    // =================================================================
    
    /**
     * Print current configuration to stdout
     */
    void print_configuration() const {
        std::cout << "=== RTWBS Configuration ===" << std::endl;
        std::cout << "Timed Automaton:" << std::endl;
        std::cout << "  Default Action: '" << timed_automaton_config_.default_action_name << "'" << std::endl;
        std::cout << "  Tau Action: '" << timed_automaton_config_.tau_action_name << "'" << std::endl;
        std::cout << "  Max States: " << timed_automaton_config_.max_states_default << std::endl;
        std::cout << "  Debug Output: " << (timed_automaton_config_.enable_debug_output ? "ON" : "OFF") << std::endl;

    }
};

// =================================================================
// Global convenience functions
// =================================================================

/**
 * Get reference to the global configuration instance
 */
inline Config& config() {
    return Config::instance();
}

/**
 * Convenience macro for accessing configuration values
 */
#define RTWBS_CONFIG rtwbs::config()

/**
 * Convenience macros for specific config sections
 */
#define TA_CONFIG RTWBS_CONFIG.timed_automaton()
#define RTWBS_ALGO_CONFIG RTWBS_CONFIG.rtwbs()

} // namespace rtwbs

#endif // CONFIGS_H
