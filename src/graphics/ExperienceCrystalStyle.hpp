#pragma once

namespace ian {

enum class ExperienceCrystalTier { Turquoise, Blue, Gold };

struct ExperienceCrystalStyle {
    ExperienceCrystalTier tier;
    float scale;
};

// Use the actual stored reward, including split drops and merged overflow.
// Keep most drops turquoise; higher-value rewards remain distinct by size
// as well as colour without changing the number of pickups or their value.
[[nodiscard]] constexpr ExperienceCrystalStyle experienceCrystalStyle(int value) {
    if (value >= 8) return {ExperienceCrystalTier::Gold, 1.35F};
    if (value >= 4) return {ExperienceCrystalTier::Blue, 1.18F};
    return {ExperienceCrystalTier::Turquoise, 1.0F};
}

} // namespace ian
