#pragma once

#include <algorithm>
#include <cmath>

namespace ian {

struct OvertimeDifficulty {
    double health{1.0};
    double damage{1.0};
    double speed{1.0};
    double spawnRate{1.0};
    bool operator==(const OvertimeDifficulty&) const = default;
};

// Cumulative combat time, starting with wave eleven. Ordinary wave growth
// remains independent and multiplicative. Density/speed have gameplay caps;
// health/damage are bounded only to keep very long/debug runs numerically safe.
struct OvertimeConfig {
    int afterWave{10};
    double healthDoublingSeconds{60.0};
    double damageDoublingSeconds{120.0};
    double speedGainPerMinute{0.12};
    double maximumSpeed{1.75};
    double spawnGainPerMinute{0.20};
    double maximumSpawnRate{2.0};

    [[nodiscard]] bool valid() const {
        return afterWave >= 1 && afterWave <= 1000 &&
            std::isfinite(healthDoublingSeconds) && healthDoublingSeconds >= 1.0 &&
            std::isfinite(damageDoublingSeconds) && damageDoublingSeconds >= 1.0 &&
            std::isfinite(speedGainPerMinute) && speedGainPerMinute >= 0.0 &&
            std::isfinite(maximumSpeed) && maximumSpeed >= 1.0 && maximumSpeed <= 3.0 &&
            std::isfinite(spawnGainPerMinute) && spawnGainPerMinute >= 0.0 &&
            std::isfinite(maximumSpawnRate) && maximumSpawnRate >= 1.0 && maximumSpawnRate <= 4.0;
    }
};

[[nodiscard]] inline OvertimeDifficulty overtimeDifficulty(
    double seconds, const OvertimeConfig& config = {}) {
    // Avoid NaNs/overflow even when inspecting corrupt/debug values.
    seconds = std::isfinite(seconds) ? std::max(0.0, seconds) : 0.0;
    const OvertimeConfig safe = config.valid() ? config : OvertimeConfig{};
    const double minutes = seconds / 60.0;
    return {
        std::exp2(std::min(40.0, seconds / safe.healthDoublingSeconds)),
        std::exp2(std::min(40.0, seconds / safe.damageDoublingSeconds)),
        std::min(safe.maximumSpeed, 1.0 + safe.speedGainPerMinute * minutes),
        std::min(safe.maximumSpawnRate, 1.0 + safe.spawnGainPerMinute * minutes),
    };
}

} // namespace ian
