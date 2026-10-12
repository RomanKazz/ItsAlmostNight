#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>

namespace ian {

// Shared by cannon/bomb explosions, spell impacts and ground slams. Visuals
// remain unrestricted; occupied audio voices must never be restarted.
class ExplosionSoundBudget {
  public:
    static constexpr std::size_t MaximumVoices = 3;
    static constexpr double MinimumInterval = 0.14;
    static constexpr float MaximumVolume = 0.40F;

    void tick(double seconds) {
        if (std::isfinite(seconds) && seconds > 0.0)
            remaining_ = std::max(0.0, remaining_ - seconds);
    }

    void reset() { remaining_ = 0.0; }

    [[nodiscard]] std::optional<float> reserve(std::size_t activeVoices) {
        if (remaining_ > 1e-9 || activeVoices >= MaximumVoices)
            return std::nullopt;
        remaining_ = MinimumInterval;
        return MaximumVolume / std::sqrt(static_cast<float>(activeVoices + 1U));
    }

  private:
    double remaining_{};
};

} // namespace ian
