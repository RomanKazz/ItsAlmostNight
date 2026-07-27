#pragma once

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string_view>

inline void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

inline void requireNear(double actual, double expected, double epsilon, std::string_view message) {
    require(std::abs(actual - expected) <= epsilon, message);
}
