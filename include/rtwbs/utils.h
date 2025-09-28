

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

} // namespace rtwbs

#endif // RTWBS_UTILS_H