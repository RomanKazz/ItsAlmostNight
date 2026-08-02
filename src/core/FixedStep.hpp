#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace ian {

class FixedStep {
  public:
    static constexpr double TickSeconds = 1.0 / 60.0;
    static constexpr double MaxFrameSeconds = 0.25;

    template <typename TickFunction>
    std::size_t advance(double frameSeconds, TickFunction&& tick) {
        if (!std::isfinite(frameSeconds)) {
            frameSeconds = 0.0;
        }
        accumulator_ += std::clamp(frameSeconds, 0.0, MaxFrameSeconds);

        std::size_t tickCount = 0;
        while (accumulator_ >= TickSeconds) {
            tick(TickSeconds);
            accumulator_ -= TickSeconds;
            ++tickCount;
        }
        return tickCount;
    }

    [[nodiscard]] double interpolationAlpha() const {
        return accumulator_ / TickSeconds;
    }

    void reset() { accumulator_ = 0.0; }

  private:
    double accumulator_{};
};

} // namespace ian
