#pragma once

#include <limits>

namespace ian {

[[nodiscard]] constexpr int saturatingAdd(int left, int right) {
    if (right > 0 &&
        left > std::numeric_limits<int>::max() - right) {
        return std::numeric_limits<int>::max();
    }
    if (right < 0 &&
        left < std::numeric_limits<int>::min() - right) {
        return std::numeric_limits<int>::min();
    }
    return left + right;
}

[[nodiscard]] constexpr int saturatingMultiplyNonNegative(
    int left, int right) {
    if (left <= 0 || right <= 0) {
        return 0;
    }
    if (left > std::numeric_limits<int>::max() / right) {
        return std::numeric_limits<int>::max();
    }
    return left * right;
}

} // namespace ian
