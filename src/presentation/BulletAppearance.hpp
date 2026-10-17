#pragma once

#include <cstdint>

namespace ian::presentation {

struct BulletColor {
    std::uint8_t r{255};
    std::uint8_t g{183};
    std::uint8_t b{72};
};

struct BulletAppearance {
    float scale{1.0F};
    BulletColor color{};
};

// Damage and flight speed never alter a projectile's authored style.
// Keep the legacy damage argument for event/presentation callers.
[[nodiscard]] inline constexpr BulletAppearance bulletAppearance(double) {
    return {};
}

// Cannon, catapult and mortar shells retain a neutral iron/stone palette.
inline constexpr BulletAppearance CannonballAppearance{1.0F, {96, 113, 126}};

} // namespace ian::presentation
