#pragma once

#include <algorithm>
#include <array>
#include <cmath>
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

// One internal damage unit is the level-one gun turret's shot (10 on the
// HUD). Logarithmic growth keeps ordinary upgrades visible without letting
// late-run damage cover the screen. Evaluate once when the shot is created.
[[nodiscard]] inline BulletAppearance bulletAppearance(double payloadDamage) {
    if (!std::isfinite(payloadDamage) || payloadDamage <= 0.0) {
        return {};
    }
    const double power = std::log2(std::clamp(payloadDamage, 0.25, 16.0));
    constexpr std::array<BulletColor, 5> Palette{{
        {255, 183, 72},
        {255, 133, 55},
        {255, 76, 84},
        {204, 102, 255},
        {132, 205, 255},
    }};
    const double colorPower = std::clamp(power, 0.0, 4.0);
    const auto index = static_cast<std::size_t>(colorPower);
    const BulletColor from = Palette[index];
    const BulletColor to = Palette[std::min(index + 1, Palette.size() - 1)];
    const double blend = colorPower - static_cast<double>(index);
    const auto channel = [blend](std::uint8_t a, std::uint8_t b) {
        return static_cast<std::uint8_t>(std::lround(
            static_cast<double>(a) + (static_cast<double>(b) - a) * blend));
    };
    return {
        .scale = static_cast<float>(std::clamp(1.0 + power * 0.35, 0.72, 2.4)),
        .color = {channel(from.r, to.r), channel(from.g, to.g), channel(from.b, to.b)},
    };
}

} // namespace ian::presentation
