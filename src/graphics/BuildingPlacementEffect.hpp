#pragma once

#include "graphics/EnemyMagicEffect.hpp"

namespace ian::building_placement_effect {

// A quick pop, soft compression and small settle. Exactly unit scale at rest.
inline float scaleAt(float progress) {
    if (!std::isfinite(progress) || progress >= 1) return 1;
    const float p = std::max(0.0F, progress);
    constexpr std::array<float, 5> times{0, .22F, .46F, .70F, 1};
    constexpr std::array<float, 5> sizes{.08F, 1.13F, .96F, 1.015F, 1};
    for (std::size_t i = 1; i < times.size(); ++i) {
        if (p <= times[i]) {
            return sizes[i - 1] + (sizes[i] - sizes[i - 1]) *
                fire_wand_effect::ease((p - times[i - 1]) / (times[i] - times[i - 1]));
        }
    }
    return 1;
}

// Flat-colour geometry in the existing depth-tested effect batch: no textures,
// point lights, emitters or allocations on placement. Maximum 456 vertices.
inline void burst(Vector3 origin, Vector3 camera, float progress, float scale,
                  std::uint32_t seed, bool detailed, bool reduceFlashes) {
    if (!std::isfinite(progress) || progress <= .12F || progress >= 1 || scale <= 0) return;
    const float p = (progress - .12F) / .88F;
    const float spread = 1 - (1 - p) * (1 - p);
    const float fade = 1 - fire_wand_effect::ease((p - .12F) / .88F);
    const auto b = enemy_magic_effect::basis(origin, camera);
    rlNormal3f(b.front.x, b.front.y, b.front.z);
    const float seedAngle = static_cast<float>(seed % 37U) * .17F;
    const int puffs = detailed ? 6 : 3;
    for (int i = 0; i < puffs; ++i) {
        const float angle = seedAngle + static_cast<float>(i) * 2 * PI / static_cast<float>(puffs);
        const float distance = (.45F + spread * .75F) * scale;
        const float radius = (.10F + .17F * std::sin(PI * std::sqrt(p))) * scale;
        const Vector3 center{origin.x + std::cos(angle) * distance,
            origin.y + (.10F + .22F * p) * scale,
            origin.z + std::sin(angle) * distance};
        enemy_magic_effect::disc(center, b.right, b.up, radius,
            enemy_magic_effect::opacity({239, 217, 173, 190}, fade),
            enemy_magic_effect::opacity({217, 185, 137, 110}, fade), 10);
        const Vector3 lobe = fire_wand_effect::planePoint(
            Vector3Add(center, Vector3Scale(b.front, .005F)), b.right, b.up,
            radius * .38F, radius * .28F);
        enemy_magic_effect::disc(lobe, b.right, b.up, radius * .67F,
            enemy_magic_effect::opacity({255, 239, 203, 195}, fade),
            enemy_magic_effect::opacity({239, 217, 173, 140}, fade), 10);
    }
    if (reduceFlashes) return;
    const int stars = detailed ? 4 : 2;
    const float sparkle = std::sin(PI * std::min(1.0F, p / .82F));
    for (int i = 0; i < stars; ++i) {
        const float angle = seedAngle + static_cast<float>(i) * 2.39996F;
        const float distance = (.55F + spread * .46F) * scale;
        const Vector3 center{origin.x + std::cos(angle) * distance,
            origin.y + (.22F + std::sin(p * PI) * .52F) * scale,
            origin.z + std::sin(angle) * distance};
        enemy_magic_effect::star(center, b, .10F * scale * sparkle,
            angle + p * .6F, enemy_magic_effect::opacity({255, 218, 101, 240}, fade));
    }
}

} // namespace ian::building_placement_effect
