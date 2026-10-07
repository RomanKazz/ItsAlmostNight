#pragma once

#include "world/VisualBiomes.hpp"

#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace ian::atmosphere {

inline constexpr std::size_t LeafCount = 24;
inline constexpr std::size_t DustCount = 48;
inline constexpr float LeafRange = 22;
inline constexpr float DustRange = 17;

struct Particle {
    Vector3 position{}; // Y is an offset above terrain until culled and grounded.
    float opacity{};
    float size{};
    float yaw{};
    float tilt{};
    float shade{1};
};
struct Vertex { Vector3 position; Color color; };

inline float smooth(float from, float to, float value) {
    const float t = std::clamp((value - from) / (to - from), 0.0F, 1.0F);
    return t * t * (3 - 2 * t);
}
inline float wrap(float value, float center, float range) {
    float offset = std::fmod(value - center + range, 2 * range);
    if (offset < 0) offset += 2 * range;
    return center + offset - range;
}
inline Color tint(visual_biomes::Rgb rgb, float shade, float opacity) {
    const auto channel = [shade](float value) {
        return static_cast<unsigned char>(std::lround(std::clamp(value * shade, 0.0F, 1.0F) * 255));
    };
    return {channel(rgb[0]), channel(rgb[1]), channel(rgb[2]),
        static_cast<unsigned char>(std::lround(std::clamp(opacity, 0.0F, 1.0F) * 255))};
}

// A bounded analytic field: no entities, per-frame allocation or lifetime lists.
// Seeds and palette transforms are prepared only when the world changes.
class Field {
    struct Seed {
        float x{}, z{}, speed{}, cycle{}, cycleSpeed{}, phase{}, size{}, spin{}, shade{};
    };
    std::array<Seed, LeafCount> leaves_{};
    std::array<Seed, DustCount> dust_{};
    visual_biomes::Layout biomes_{};
    std::array<visual_biomes::Rgb, visual_biomes::Count> leafColors_{};
    std::array<visual_biomes::Rgb, visual_biomes::Count> dustColors_{};
    std::uint32_t seed_{};
    double worldSize_{};
    bool ready_{};

    template<std::size_t N>
    static void prepare(std::array<Seed, N>& seeds, std::uint32_t worldSeed, float range, bool leaf) {
        for (std::size_t i = 0; i < N; ++i) {
            const auto salt = static_cast<std::uint32_t>(i * 17 + (leaf ? 300 : 1300));
            const auto random = [=](std::uint32_t offset) { return visual_biomes::randomUnit(worldSeed, salt + offset); };
            seeds[i] = {
                .x = (random(1) * 2 - 1) * range, .z = (random(2) * 2 - 1) * range,
                .speed = leaf ? 0.75F + random(3) * 0.90F : 0.12F + random(3) * 0.26F,
                .cycle = random(4), .cycleSpeed = leaf ? 0.07F + random(5) * 0.04F : 0.025F + random(5) * 0.02F,
                .phase = random(6) * 2 * PI,
                .size = leaf ? 0.23F + random(7) * 0.18F : 0.017F + random(7) * 0.021F,
                .spin = (random(8) < 0.5F ? -1.0F : 1.0F) * (0.45F + random(9) * 0.5F),
                .shade = 0.86F + random(10) * 0.22F,
            };
        }
    }
    static Particle sample(const Seed& s, float time, Vector3 center, float range, bool leaf) {
        const float phase = time * (leaf ? 1.6F : 0.65F) + s.phase;
        const float x = wrap(s.x + time * s.speed + std::sin(phase) * (leaf ? 0.55F : 0.24F), center.x, range);
        const float z = wrap(s.z + time * s.speed * 0.28F + std::cos(phase * 0.83F) * 0.28F, center.z, range);
        const float dx = x - center.x, dz = z - center.z;
        const float distanceSquared = dx * dx + dz * dz;
        // Fade the wrap boundary before querying terrain or generating geometry.
        const float edge = 1 - smooth(range * range * 0.55F, range * range, distanceSquared);
        const float near = smooth(0.30F, 2.0F, distanceSquared);
        const float cycle = s.cycle + time * s.cycleSpeed;
        const float progress = cycle - std::floor(cycle);
        const float life = smooth(0, 0.10F, progress) * (1 - smooth(0.85F, 1, progress));
        return {
            .position = {x, leaf ? 0.4F + (1 - progress) * 4.6F + std::sin(phase) * 0.12F
                                : 0.35F + progress * 3.8F + std::sin(phase) * 0.10F, z},
            .opacity = edge * near * life * (leaf ? 0.82F : 0.34F),
            .size = s.size, .yaw = s.phase + time * s.spin,
            .tilt = s.phase * 0.7F + std::sin(phase) * 0.8F,
            .shade = s.shade,
        };
    }
    std::size_t biome(Vector3 position) const {
        const auto weights = visual_biomes::weights(biomes_, position.x, position.z);
        return static_cast<std::size_t>(std::max_element(weights.begin(), weights.end()) - weights.begin());
    }
  public:
    void configure(std::uint32_t seed, double worldSize) {
        if (ready_ && seed == seed_ && worldSize == worldSize_) return;
        seed_ = seed; worldSize_ = worldSize; ready_ = true;
        biomes_ = visual_biomes::generate(seed, worldSize);
        prepare(leaves_, seed, LeafRange, true);
        prepare(dust_, seed, DustRange, false);
        for (std::size_t i = 0; i < visual_biomes::Count; ++i) {
            leafColors_[i] = visual_biomes::shiftPalette(biomes_.leaves[i], 0, 0.05F, 0.10F);
            for (std::size_t c = 0; c < 3; ++c) dustColors_[i][c] = 0.45F + leafColors_[i][c] * 0.55F;
        }
    }
    [[nodiscard]] Particle leaf(std::size_t i, float time, Vector3 center) const {
        return sample(leaves_[i], time, center, LeafRange, true);
    }
    [[nodiscard]] Particle dust(std::size_t i, float time, Vector3 center) const {
        return sample(dust_[i], time, center, DustRange, false);
    }
    [[nodiscard]] Color leafColor(const Particle& p) const {
        return tint(leafColors_[biome(p.position)], p.shade, p.opacity);
    }
    [[nodiscard]] Color dustColor(const Particle& p) const {
        return tint(dustColors_[biome(p.position)], 1, p.opacity);
    }
};

// Pointed, curved leaf with a raised midrib, lighter vein and short stem.
// Both faces are explicit: fluttering leaves remain visible with backface culling.
[[nodiscard]] inline std::array<Vertex, 72> leafVertices(const Particle& p, Color color) {
    constexpr std::array<Vector2, 8> rim{{
        {-.43F, 0}, {-.27F, .18F}, {-.04F, .29F}, {.28F, .19F},
        {.55F, 0}, {.28F, -.19F}, {-.04F, -.29F}, {-.27F, -.18F},
    }};
    const float cosine = std::cos(p.yaw), sine = std::sin(p.yaw);
    const float tiltCosine = std::cos(p.tilt), tiltSine = std::sin(p.tilt);
    const Vector3 right{cosine, 0, sine};
    const Vector3 up{-sine * tiltSine, tiltCosine, cosine * tiltSine};
    const Vector3 normal = Vector3CrossProduct(right, up);
    const auto point = [&](float x, float y, float fold = 0.0F) {
        return Vector3Add(p.position, Vector3Scale(
            Vector3Add(Vector3Add(Vector3Scale(right, x), Vector3Scale(up, y)), Vector3Scale(normal, fold)), p.size));
    };
    const auto shade = [color](float multiplier) {
        return Color{static_cast<unsigned char>(std::min(255L, std::lround(color.r * multiplier))),
                     static_cast<unsigned char>(std::min(255L, std::lround(color.g * multiplier))),
                     static_cast<unsigned char>(std::min(255L, std::lround(color.b * multiplier))), color.a};
    };
    std::array<Vertex, 72> vertices{};
    std::size_t cursor = 0;
    const auto triangle = [&](Vector3 a, Vector3 b, Vector3 c, Color tintColor) {
        vertices[cursor++] = {a, tintColor}; vertices[cursor++] = {b, tintColor}; vertices[cursor++] = {c, tintColor};
    };
    for (std::size_t i = 0; i < rim.size(); ++i) {
        const auto a = rim[i], b = rim[(i + 1) % rim.size()];
        triangle(point(0, 0, .065F), point(b.x, b.y), point(a.x, a.y), shade(i < 4 ? 1.10F : .78F));
        triangle(point(0, 0, -.025F), point(a.x, a.y), point(b.x, b.y), shade(.72F));
    }
    // A narrow midrib sits above each folded face rather than z-fighting with it.
    for (const float face : {1.0F, -1.0F}) {
        const auto a = point(-.36F, -.012F * face, .072F * face);
        const auto b = point(.45F, -.006F * face, .016F * face);
        const auto c = point(.45F, .006F * face, .016F * face);
        const auto d = point(-.36F, .012F * face, .072F * face);
        triangle(a, b, c, shade(1.22F)); triangle(a, c, d, shade(1.22F));
        const auto e = point(-.60F, -.016F * face), f = point(-.41F, -.016F * face);
        const auto g = point(-.41F, .016F * face), h = point(-.60F, .016F * face);
        triangle(e, f, g, shade(.64F)); triangle(e, g, h, shade(.64F));
    }
    return vertices;
}

// The fixed fan eliminates per-segment sine/cosine calls for each dust mote.
[[nodiscard]] inline std::array<Vertex, 24> dustVertices(const Particle& p, Color color, Vector3 right, Vector3 up) {
    constexpr float Diagonal = .70710678F;
    constexpr std::array<Vector2, 8> ring{{{1,0}, {Diagonal,Diagonal}, {0,1}, {-Diagonal,Diagonal},
        {-1,0}, {-Diagonal,-Diagonal}, {0,-1}, {Diagonal,-Diagonal}}};
    std::array<Vertex, 24> vertices{};
    const auto point = [&](Vector2 v) { return Vector3Add(p.position, Vector3Add(
        Vector3Scale(right, v.x * p.size), Vector3Scale(up, v.y * p.size))); };
    const Color edge{color.r, color.g, color.b, 0};
    for (std::size_t i = 0; i < ring.size(); ++i) {
        vertices[i * 3] = {p.position, color}; vertices[i * 3 + 1] = {point(ring[i]), edge};
        vertices[i * 3 + 2] = {point(ring[(i + 1) % ring.size()]), edge};
    }
    return vertices;
}

} // namespace ian::atmosphere
