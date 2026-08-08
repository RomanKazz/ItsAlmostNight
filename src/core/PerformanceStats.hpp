#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>

namespace ian {

using PerformanceClock = std::chrono::steady_clock;

[[nodiscard]] inline double performanceMilliseconds(
    PerformanceClock::time_point start,
    PerformanceClock::time_point end = PerformanceClock::now()) noexcept {
    return std::chrono::duration<double, std::milli>(end - start).count();
}

struct PerformanceMetric {
    void sample(double milliseconds) noexcept {
        const double value = std::isfinite(milliseconds)
            ? std::max(0.0, milliseconds)
            : 0.0;
        lastMilliseconds = value;
        averageMilliseconds = sampleCount == 0U
            ? value
            : averageMilliseconds * 0.9 + value * 0.1;
        ++sampleCount;
    }

    double lastMilliseconds{};
    double averageMilliseconds{};
    std::uint64_t sampleCount{};
};

} // namespace ian
