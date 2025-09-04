

#include "utap/utap.hpp"

//this class is a singleton
namespace rtwbs{
class ExpressionEvaluator {
public: 
    #define EVALUATE_EXPRESSION(expr, result, constants_, variables_) \
        ExpressionEvaluator::get_instance().evaluate(expr, result, constants_, variables_)
    static ExpressionEvaluator& get_instance() {
        static ExpressionEvaluator instance;
        return instance;
    }

    bool evaluate(const UTAP::Expression& expr, int& result, 
                             const std::unordered_map<std::string, int>& constants_,
                                                 const std::unordered_map<std::string, int>& variables_) {
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
                        return evaluate(var_data->init, result, constants_, variables_);
                    }
                }
                return false;
            }
            
            case UTAP::Constants::PLUS:
                if (expr.get_size() == 2) {
                    int left, right;
                    if (evaluate(expr[0], left, constants_, variables_) && evaluate(expr[1], right, constants_, variables_)) {
                        result = left + right;
                        return true;
                    }
                }
                return false;
                
            case UTAP::Constants::MINUS:
                if (expr.get_size() == 2) {
                    int left, right;
                    if (evaluate(expr[0], left, constants_, variables_) && evaluate(expr[1], right, constants_, variables_)) {
                        result = left - right;
                        return true;
                    }
                }
                return false;
                
            case UTAP::Constants::MULT:
                if (expr.get_size() == 2) {
                    int left, right;
                    if (evaluate(expr[0], left, constants_, variables_) && evaluate(expr[1], right, constants_, variables_)) {
                        result = left * right;
                        return true;
                    }
                }
                return false;
                
            default:
                return false;
        }
    }
private:
    ExpressionEvaluator() = default;
    ~ExpressionEvaluator() = default;
    ExpressionEvaluator(const ExpressionEvaluator&) = delete;
    ExpressionEvaluator& operator=(const ExpressionEvaluator&) = delete;
};

} // namespace rtwbs