#pragma once

#include "combat/FlamethrowerAttack.hpp"
#include "graphics/FireWandEffect.hpp"

namespace ian::flamethrower_effect {

inline constexpr int MaximumPuffsPerFrame = 384;

// Emission is delayed by travel time: ignition grows from the socket, and
// after shutdown the last emitted heads keep travelling instead of blinking.
[[nodiscard]] inline float emissionEnvelope(double ignitionAge, double travel,
    double stoppedAgo = -1) {
    const auto smooth = [](double value) {
        const double t = std::clamp(value, 0.0, 1.0);
        return t * t * (3.0 - 2.0 * t);
    };
    const double birth = ignitionAge - travel;
    const double ignition = smooth(birth / FlamethrowerFadeSeconds);
    const double shutdown = stoppedAgo < 0 ? 1 :
        smooth((travel - stoppedAgo + FlamethrowerFadeSeconds) / FlamethrowerFadeSeconds);
    return static_cast<float>(ignition * shutdown);
}

inline Vector3 point(Vec3 value) {
    return {static_cast<float>(value.x), static_cast<float>(value.y), static_cast<float>(value.z)};
}

// Rounded flame heads, with a curled tail. All particles share one triangle
// batch; no textures, shaders, random allocations or growing emitter lists.
inline void puff(Vector3 center, Vector3 side, Vector3 along, Vector3 front,
    float radius, float stretch, float curl, float alpha) {
    const float bend = curl * 0.3F;
    const std::array<Vector2, 19> curve{{
        {0, -.03F}, {-.7F, -.1F}, {-1, .02F}, {-1, .18F},
        {-1, .36F}, {-.7F, .48F}, {-.62F, .62F},
        {-.52F, .8F}, {.02F + bend, .91F}, {.24F + bend, 1},
        {.1F + bend, .8F}, {.48F, .69F}, {.55F, .52F},
        {.64F, .35F}, {1, .3F}, {.95F, .16F},
        {.9F, -.03F}, {.26F, -.06F}, {0, -.03F},
    }};
    constexpr std::array<float, 3> Scales{1.0F, 0.70F, 0.33F};
    constexpr std::array<Color, 3> Cores{{{255, 148, 16, 255}, {255, 223, 63, 255}, {255, 255, 203, 255}}};
    constexpr std::array<Color, 3> Rims{{{240, 64, 20, 255}, {255, 163, 22, 255}, {255, 243, 114, 255}}};
    for (std::size_t layer = 0; layer < Scales.size(); ++layer) {
        const Vector3 bias = Vector3Scale(front, 0.003F * static_cast<float>(layer));
        const float lift = -radius * stretch * 0.25F;
        const auto planePoint = [&](Vector2 p) {
            return Vector3Add(Vector3Add(center, bias), Vector3Add(
                Vector3Scale(side, radius * Scales[layer] * p.x),
                Vector3Scale(along, radius * Scales[layer] * p.y * stretch + lift)));
        };
        const Vector3 core = planePoint({0, .32F});
        Vector2 previous = curve.front();
        for (std::size_t segment = 0; segment < 6; ++segment) {
            const std::size_t first = segment * 3;
            for (int step = 1; step <= 3; ++step) {
                const float t = static_cast<float>(step) / 3.0F;
                const float u = 1.0F - t;
                const Vector2 next{
                    u*u*u*curve[first].x + 3*u*u*t*curve[first+1].x +
                        3*u*t*t*curve[first+2].x + t*t*t*curve[first+3].x,
                    u*u*u*curve[first].y + 3*u*u*t*curve[first+1].y +
                        3*u*t*t*curve[first+2].y + t*t*t*curve[first+3].y};
                fire_wand_effect::vertex(core, fire_wand_effect::opacity(Cores[layer], alpha));
                fire_wand_effect::vertex(planePoint(previous), fire_wand_effect::opacity(Rims[layer], alpha));
                fire_wand_effect::vertex(planePoint(next), fire_wand_effect::opacity(Rims[layer], alpha));
                previous = next;
            }
        }
    }
}

// Analytic particle ages mean the animation keeps moving even when the game
// uses a reused runtime slot. Simulation time controls pause/resume naturally.
inline void emitStream(Vector3 muzzle, Vector3 direction, Vector3 camera,
    float length, float size, float amount, double seconds, std::uint32_t seed,
    int particles, bool detailed, double ignitionAge = 1000000, double stoppedAgo = -1) {
    direction = fire_wand_effect::normalized(direction, {0, 0, -1});
    const Vector3 right = fire_wand_effect::normalized(
        Vector3CrossProduct(direction, {0, 1, 0}), {1, 0, 0});
    const Vector3 up = Vector3CrossProduct(right, direction);
    const float identity = static_cast<float>(seed % 997U) * 0.123F;
    const float speed = static_cast<float>(FlamethrowerJetSpeed);
    const float travelTime = std::max(0.1F, length / speed);
    for (int index = 0; index < particles; ++index) {
        const float lane = static_cast<float>(index % 3) - 1.0F;
        const float initial = static_cast<float>(index) / static_cast<float>(particles);
        const float age = static_cast<float>(std::fmod(seconds / travelTime + initial + identity, 1.0));
        const float distance = age * length;
        const float envelope = emissionEnvelope(ignitionAge, age * travelTime, stoppedAgo);
        if (envelope <= 0) continue;
        const float coneWidth = 0.06F + distance * static_cast<float>(std::tan(FlamethrowerHalfAngle));
        const float wobble = static_cast<float>(seconds * 12.0) + static_cast<float>(index) * 1.71F + identity;
        Vector3 center = Vector3Add(muzzle, Vector3Scale(direction, distance));
        center = Vector3Add(center, Vector3Scale(right,
            coneWidth * (lane * 0.42F + 0.12F * std::sin(wobble))));
        center = Vector3Add(center, Vector3Scale(up, coneWidth * 0.20F * std::cos(wobble * 0.8F)));
        const Vector3 front = fire_wand_effect::normalized(Vector3Subtract(camera, center), {0, 0, 1});
        const Vector3 along = fire_wand_effect::normalized(Vector3Subtract(direction,
            Vector3Scale(front, Vector3DotProduct(direction, front))), up);
        const Vector3 side = fire_wand_effect::normalized(Vector3CrossProduct(front, along), right);
        // Dense white/yellow nozzle becomes broad orange heads down the jet.
        const float radius = (0.10F * std::min(size, 2.5F) + coneWidth * 0.36F) *
            (0.9F + 0.1F * std::sin(wobble));
        const float fade = std::clamp((1.0F - age) * 5.0F, 0.0F, 1.0F) * amount * envelope;
        puff(center, side, along, front, radius * (0.4F + .6F * envelope), 1.6F, std::sin(wobble), fade);
    }
    // A compact hot pilot at the muzzle joins the moving heads. Sparks are
    // optional detail, while low particle quality retains the actual jet.
    const Vector3 front = fire_wand_effect::normalized(Vector3Subtract(camera, muzzle), {0, 0, 1});
    const Vector3 along = fire_wand_effect::normalized(Vector3Subtract(direction,
        Vector3Scale(front, Vector3DotProduct(direction, front))), up);
    const Vector3 side = fire_wand_effect::normalized(Vector3CrossProduct(front, along), right);
    puff(Vector3Add(muzzle, Vector3Scale(direction, 0.08F)), side, along, front,
        0.11F * std::min(size, 2.5F), 1.8F, std::sin(static_cast<float>(seconds * 18)),
        amount * emissionEnvelope(ignitionAge, 0, stoppedAgo));
    if (detailed) {
        for (int index = 0; index < 4; ++index) {
            const float age = static_cast<float>(std::fmod(seconds * 2.7 + index * 0.249 + identity, 1.0));
            Vector3 center = Vector3Add(muzzle, Vector3Scale(direction, age * length));
            center = Vector3Add(center, Vector3Scale(up, age * age * 0.45F));
            center = Vector3Add(center, Vector3Scale(right, std::sin(static_cast<float>(index) * 3.1F + identity) * age * 0.5F));
            const float radius = 0.018F + 0.012F * (1.0F - age);
            const Color color = fire_wand_effect::opacity({255, 240, 140, 255}, amount * (1.0F - age) *
                emissionEnvelope(ignitionAge, age * travelTime, stoppedAgo));
            fire_wand_effect::disc(center, side, along, radius, color, color, 6);
        }
    }
}

} // namespace ian::flamethrower_effect
