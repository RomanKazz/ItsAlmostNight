#pragma once

#include <algorithm>
#include <array>

namespace ian::decoy_beacon_model {
// Authored socket positions in decoy_beacon_levels/manifest.json.
inline constexpr std::array<float, 8> HangingLampScales{1, 1, 1, 1, 1, 1.14F, 1.14F, 1.27F};
inline constexpr std::array<float, 8> CentralLampScales{1, 1, 1, 1.18F, 1.18F, 1.18F, 1.35F, 1.35F};

[[nodiscard]] inline float hangingLampScale(int level) {
    return HangingLampScales[static_cast<std::size_t>(std::clamp(level, 1, 8) - 1)];
}

[[nodiscard]] inline float hangingLampHeight(int level) {
    return .810F + (.6705F - .810F) * hangingLampScale(level);
}

[[nodiscard]] inline float centralLampScale(int level) {
    return CentralLampScales[static_cast<std::size_t>(std::clamp(level, 1, 8) - 1)];
}
} // namespace ian::decoy_beacon_model
