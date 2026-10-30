#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>

namespace ian {

enum class DefenseSoundGroup : std::size_t {
    Artillery, Rockets, Catapult, Lightning, Bow, Gun, Wind, Trap, Impact, Count,
};
inline constexpr std::size_t DefenseSoundGroupCount = static_cast<std::size_t>(DefenseSoundGroup::Count);

// A representative rhythm for the whole defense mix, independent of the
// number of projectiles, buildings and render frames. Player/UI voices bypass it.
class DefenseSoundBudget {
public:
    static constexpr std::size_t MaximumVoices = 5;
    static constexpr double StartsPerSecond = 14.0;
    static constexpr double BurstStarts = 2.0;

    void tick(double seconds) {
        if (!std::isfinite(seconds) || seconds <= 0.0) return;
        tokens_ = std::min(BurstStarts, tokens_ + seconds * StartsPerSecond);
        for (double& remaining : remaining_) remaining = std::max(0.0, remaining - seconds);
    }
    void reset() { tokens_ = BurstStarts; remaining_.fill(0.0); }

    [[nodiscard]] std::optional<float> reserve(DefenseSoundGroup group,
        std::size_t activeInGroup, std::size_t activeTotal) {
        const auto index = static_cast<std::size_t>(group);
        const std::size_t limit = group == DefenseSoundGroup::Gun ? 2U : 1U;
        if (index >= DefenseSoundGroupCount || tokens_ < 1.0 - 1e-9 ||
            remaining_[index] > 1e-9 || activeInGroup >= limit || activeTotal >= MaximumVoices)
            return std::nullopt;
        static constexpr std::array intervals{0.20, 0.18, 0.24, 0.16, 0.14, 0.10, 0.24, 0.20, 0.10};
        tokens_ = std::max(0.0, tokens_ - 1.0);
        remaining_[index] = intervals[index];
        return 1.0F / std::sqrt(static_cast<float>(activeTotal + 1U));
    }

private:
    double tokens_{BurstStarts};
    std::array<double, DefenseSoundGroupCount> remaining_{};
};

} // namespace ian
