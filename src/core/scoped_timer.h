#include <chrono>
#include <iostream>
#include <string>

class ScopedTimer {
public:
    ScopedTimer(const std::string& label)
        : label_(label), start_(std::chrono::steady_clock::now()) {}

    ~ScopedTimer() {
        auto end = std::chrono::steady_clock::now();
        auto duration = end - start_;
        double seconds = std::chrono::duration<double>(duration).count();
        std::cout << label_ << " took " << seconds << " seconds.\n";
    }

private:
    std::string label_;
    std::chrono::steady_clock::time_point start_;
};
