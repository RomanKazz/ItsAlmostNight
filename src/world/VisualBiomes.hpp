#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace ian::visual_biomes {

// Visual composition only: terrain heights, collisions and resources stay independent.
inline constexpr std::size_t Count = 4;
using Rgb = std::array<float, 3>;
inline constexpr Rgb GreenGround{0.44F, 0.50F, 0.24F};
inline constexpr Rgb GreenLeaves{0.400F, 0.553F, 0.243F};

struct Layout {
    std::array<std::array<float, 2>, Count> centers{};
    std::array<Rgb, Count> ground{};
    std::array<Rgb, Count> leaves{};
    // World size, transition width and two static boundary-warp phases.
    std::array<float, 4> shape{};
};

inline float randomUnit(std::uint32_t seed, std::uint32_t salt) {
    auto value = seed ^ (salt * 0x9e3779b9U);
    value ^= value >> 16U;
    value *= 0x7feb352dU;
    value ^= value >> 15U;
    value *= 0x846ca68bU;
    value ^= value >> 16U;
    return static_cast<float>(value & 0xffffffU) / 16777216.0F;
}

inline Rgb shiftPalette(Rgb color, float hueShift, float saturationShift, float valueShift) {
    const float maximum = std::max({color[0], color[1], color[2]});
    const float minimum = std::min({color[0], color[1], color[2]});
    const float delta = maximum - minimum;
    float hue = 0;
    if (delta > 0.0001F) {
        if (maximum == color[0]) hue = (color[1] - color[2]) / delta;
        else if (maximum == color[1]) hue = 2 + (color[2] - color[0]) / delta;
        else hue = 4 + (color[0] - color[1]) / delta;
        hue /= 6;
    }
    hue += hueShift;
    hue -= std::floor(hue);
    const float saturation = std::clamp((maximum > 0 ? delta / maximum : 0) + saturationShift, 0.0F, 1.0F);
    const float value = std::clamp(maximum + valueShift, 0.0F, 1.0F);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        const float offset = channel == 0 ? 5.0F : channel == 1 ? 3.0F : 1.0F;
        const float k = std::fmod(hue * 6 + offset, 6.0F);
        color[channel] = value * (1 - saturation * std::clamp(std::min(k, 4 - k), 0.0F, 1.0F));
    }
    return color;
}

[[nodiscard]] inline Layout generate(std::uint32_t seed, double worldSize) {
    Layout layout;
    const float size = std::isfinite(worldSize) && worldSize > 0
        ? static_cast<float>(worldSize) : 384.0F;
    constexpr float Tau = 2 * std::numbers::pi_v<float>;
    const float rotation = randomUnit(seed, 1) * Tau;
    constexpr std::array<Rgb, Count> Ground{{GreenGround,
        {0.64F, 0.39F, 0.47F}, {0.60F, 0.43F, 0.23F}, {0.63F, 0.71F, 0.75F}}};
    constexpr std::array<Rgb, Count> Leaves{{GreenLeaves,
        {0.86F, 0.43F, 0.63F}, {0.82F, 0.39F, 0.13F}, {0.48F, 0.66F, 0.77F}}};
    for (std::size_t i = 0; i < Count; ++i) {
        const auto salt = static_cast<std::uint32_t>(10 + i * 8);
        const float angle = rotation + static_cast<float>(i) * Tau / 4 + (randomUnit(seed, salt) - .5F) * .20F;
        const float radius = size * (i == 0 ? .12F : .33F + (randomUnit(seed, salt + 1) - .5F) * .035F);
        layout.centers[i] = {std::cos(angle) * radius, std::sin(angle) * radius};
        const float hue = (randomUnit(seed, salt + 2) - .5F) * (i == 1 ? .070F : .030F);
        const float saturation = (randomUnit(seed, salt + 3) - .5F) * .065F;
        const float value = (randomUnit(seed, salt + 4) - .5F) * .045F;
        layout.ground[i] = shiftPalette(Ground[i], hue, saturation, value);
        layout.leaves[i] = shiftPalette(Leaves[i], hue, saturation, value);
    }
    layout.shape = {size, size * .035F, randomUnit(seed, 60) * Tau, randomUnit(seed, 61) * Tau};
    return layout;
}

// Keep this inexpensive analytic field identical to world.vs and grass_instanced.vs.
[[nodiscard]] inline std::array<float, Count> weights(const Layout& layout, float x, float z) {
    if (!std::isfinite(x) || !std::isfinite(z)) return {1, 0, 0, 0};
    const auto& shape = layout.shape;
    const float qx = x / shape[0], qz = z / shape[0];
    x += (std::sin(qz * 11 + shape[2]) + .45F * std::sin((qx + qz) * 19 + shape[3])) * shape[0] * .018F;
    z += (std::sin(qx * 12 + shape[3]) + .45F * std::sin((qz - qx) * 17 + shape[2])) * shape[0] * .018F;
    std::array<float, Count> result{};
    for (std::size_t i = 0; i < Count; ++i)
        result[i] = std::hypot(x - layout.centers[i][0], z - layout.centers[i][1]);
    const float closest = *std::min_element(result.begin(), result.end());
    float total = 0;
    for (auto& weight : result) {
        const float t = std::clamp((weight - closest) / shape[1], 0.0F, 1.0F);
        weight = 1 - t * t * (3 - 2 * t);
        total += weight;
    }
    for (auto& weight : result) weight /= total;
    return result;
}

[[nodiscard]] inline Rgb groundColor(const Layout& layout, float x, float z, Rgb source) {
    const auto blend = weights(layout, x, z);
    constexpr Rgb Luminance{.2126F, .7152F, .0722F};
    float value = 0, reference = 0;
    for (std::size_t c = 0; c < 3; ++c) {
        value += source[c] * Luminance[c];
        reference += GreenGround[c] * Luminance[c];
    }
    const float detail = std::clamp(value / reference, .35F, 1.55F);
    Rgb color{};
    for (std::size_t c = 0; c < 3; ++c) {
        color[c] = source[c] * layout.ground[0][c] / GreenGround[c] * blend[0];
        for (std::size_t i = 1; i < Count; ++i) color[c] += layout.ground[i][c] * blend[i] * detail;
    }
    return color;
}

} // namespace ian::visual_biomes
