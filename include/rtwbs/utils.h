

#ifndef RTWBS_UTILS_H
#define RTWBS_UTILS_H


namespace rtwbs {
    class TimeoutException : public std::exception {
    private:
        std::string message_;
    public:
        explicit TimeoutException(const std::string& message = "Operation timed out")
            : message_(message) {}

        virtual const char* what() const noexcept override {
            return message_.c_str();
        }
    };

    enum class RunningMode {
        SERIAL,
        THREAD_POOL,
        OPENMP
    };

    /**
     * @brief Algorithm selection for RTWBS equivalence/simulation checks.
     *
     * GFP         – Global Greatest Fixed Point (existing eager algorithm).
     *               Builds the full zone graph upfront, seeds all candidate
     *               pairs, and eliminates invalid pairs via worklist.
     *
     * ON_THE_FLY  – Local Co-inductive Depth-First Search.
     *               Explores only reachable pairs on demand, using a
     *               co-inductive assumption (pairs on the recursion stack
     *               are assumed valid).  Supports Serial and OpenMP modes.
     */
    enum class AlgorithmMode {
        GFP,
        ON_THE_FLY
    };

} // namespace rtwbs

#endif // RTWBS_UTILS_H