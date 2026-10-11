#pragma once

#include "graphics/FireWandEffect.hpp"

#include <array>
#include <cmath>

namespace ian::bomb_drawing {

// Shared by the held item and physical projectile. Preserve the existing
// world visual footprint; all detail stays in the presentation layer.
inline constexpr float Radius = .255F;

inline Color mix(Color a, Color b, float t) {
    t = std::clamp(t, 0.0F, 1.0F);
    const auto channel = [t](unsigned char low, unsigned char high) {
        return static_cast<unsigned char>(std::lround(
            float(low) + (float(high) - float(low)) * t));
    };
    return {channel(a.r, b.r), channel(a.g, b.g), channel(a.b, b.b), a.a};
}

inline void vertex(Vector3 p, Vector3 n, Color color) {
    rlNormal3f(n.x, n.y, n.z);
    rlColor4ub(color.r, color.g, color.b, color.a);
    rlTexCoord2f(0, 0);
    rlVertex3f(p.x, p.y, p.z);
}

inline void sphere(Vector3 center, float radius, Color color,
                   bool painted = false, float urgency = 0) {
    constexpr std::size_t Rings = 16, Sides = 24;
    static const auto normals = [] {
        std::array<std::array<Vector3, Sides + 1>, Rings + 1> points{};
        for (std::size_t y = 0; y <= Rings; ++y) {
            const float latitude = PI * float(y) / float(Rings);
            for (std::size_t x = 0; x <= Sides; ++x) {
                const float longitude = 2 * PI * float(x) / float(Sides);
                points[y][x] = {std::sin(latitude) * std::cos(longitude),
                    std::cos(latitude), std::sin(latitude) * std::sin(longitude)};
            }
        }
        return points;
    }();
    const auto emit = [&](std::size_t y, std::size_t x) {
        const Vector3 n = normals[y][x];
        Color tint = color;
        if (painted) {
            tint = mix({28, 38, 62, 255}, {87, 133, 170, 255},
                       std::clamp(n.y * .5F + .5F, 0.0F, 1.0F));
            // A quiet warm-up, not a flashing red warning sphere.
            tint = mix(tint, {160, 68, 60, 255}, urgency * urgency * .38F);
        }
        vertex(Vector3Add(center, Vector3Scale(n, radius)), n, tint);
    };
    rlBegin(RL_TRIANGLES);
    for (std::size_t y = 0; y < Rings; ++y) {
        for (std::size_t x = 0; x < Sides; ++x) {
            emit(y, x); emit(y + 1, x + 1); emit(y + 1, x);
            emit(y, x); emit(y, x + 1); emit(y + 1, x + 1);
        }
    }
    rlEnd();
}

inline void gloss(Vector3 direction, float width, float height, Color color) {
    const Vector3 normal = Vector3Normalize(direction);
    const Vector3 right = Vector3Normalize(Vector3CrossProduct({0, 1, 0}, normal));
    const Vector3 up = Vector3CrossProduct(normal, right);
    const auto point = [&](float angle, float radius) {
        return Vector3Normalize(Vector3Add(normal, Vector3Add(
            Vector3Scale(right, std::cos(angle) * width * radius),
            Vector3Scale(up, std::sin(angle) * height * radius))));
    };
    const auto emit = [&](float angle, float radius) {
        const Vector3 n = point(angle, radius);
        vertex(Vector3Scale(n, Radius + .0015F), n, color);
    };
    rlBegin(RL_TRIANGLES);
    // Concentric curved strips prevent a flat fan from cutting through the
    // round shell and leaving holes in the painted highlight.
    for (int ring = 0; ring < 6; ++ring) {
        const float inner = float(ring) / 6, outer = float(ring + 1) / 6;
        for (int i = 0; i < 24; ++i) {
            const float a = float(i) * 2 * PI / 24, b = float(i + 1) * 2 * PI / 24;
            emit(a, inner); emit(a, outer); emit(b, outer);
            emit(a, inner); emit(b, outer); emit(b, inner);
        }
    }
    rlEnd();
}

inline void cylinder(Vector3 start, Vector3 end, float bottom, float top, Color color) {
    const Vector3 axis = Vector3Normalize(Vector3Subtract(end, start));
    const Vector3 right = Vector3Normalize(Vector3CrossProduct(axis, {0, 0, 1}));
    const Vector3 side = Vector3CrossProduct(axis, right);
    const float slope = (bottom - top) / Vector3Distance(start, end);
    const auto radial = [&](int i) {
        const float angle = float(i) * 2 * PI / 16;
        return Vector3Add(Vector3Scale(right, std::cos(angle)), Vector3Scale(side, std::sin(angle)));
    };
    rlBegin(RL_TRIANGLES);
    for (int i = 0; i < 16; ++i) {
        const Vector3 r = radial(i), s = radial(i + 1);
        const Vector3 a = Vector3Add(start, Vector3Scale(r, bottom));
        const Vector3 b = Vector3Add(start, Vector3Scale(s, bottom));
        const Vector3 c = Vector3Add(end, Vector3Scale(r, top));
        const Vector3 d = Vector3Add(end, Vector3Scale(s, top));
        const Vector3 n = Vector3Normalize(Vector3Add(r, Vector3Scale(axis, slope)));
        const Vector3 m = Vector3Normalize(Vector3Add(s, Vector3Scale(axis, slope)));
        vertex(a, n, color); vertex(b, m, color); vertex(c, n, color);
        vertex(b, m, color); vertex(d, m, color); vertex(c, n, color);
        const Vector3 down = Vector3Negate(axis);
        vertex(start, down, color); vertex(b, down, color); vertex(a, down, color);
        vertex(end, axis, color); vertex(c, axis, color); vertex(d, axis, color);
    }
    rlEnd();
}

inline Vector3 fusePoint(float t, float time, float urgency) {
    t *= 1 - .65F * std::clamp(urgency, 0.0F, 1.0F);
    return {.025F * t + .105F * t * t,
            .307F + .18F * t - .075F * t * t,
            .012F * std::sin(time * 7) * t * t};
}

inline Vector3 drawBody(float time, float urgency = 0) {
    sphere({}, Radius, WHITE, true, urgency);
    // Painted highlights follow the round surface and rotate with the bomb.
    for (const float side : {1.0F, -1.0F}) {
        gloss({-.46F, .54F, side * .74F}, .15F, .29F, {203, 233, 242, 255});
        gloss({-.14F, .77F, side * .62F}, .075F, .12F, {245, 251, 239, 255});
    }
    cylinder({0, .226F, 0}, {0, .278F, 0}, .096F, .085F, {233, 148, 38, 255});
    cylinder({0, .274F, 0}, {0, .296F, 0}, .085F, .074F, {255, 208, 70, 255});
    cylinder({0, .294F, 0}, {0, .313F, 0}, .053F, .045F, {118, 72, 38, 255});
    Vector3 previous = fusePoint(0, time, urgency);
    for (int i = 1; i <= 12; ++i) {
        const Vector3 next = fusePoint(float(i) / 12, time, urgency);
        const Color rope = i % 3 == 0 ? Color{187, 133, 68, 255} : Color{255, 222, 145, 255};
        cylinder(previous, next, .019F, .019F, rope);
        previous = next;
    }
    sphere(previous, .021F, {255, 144, 37, 255});
    return previous;
}

// Camera-facing flames, small arcing embers and rounded smoke puffs. The
// caller selects a color-only world pass; depth test remains enabled.
inline void drawFuse(Vector3 tip, Vector3 camera, Vector3 cameraUp,
                     float time, float urgency, bool detailed) {
    using namespace fire_wand_effect;
    const Vector3 front = normalized(Vector3Subtract(camera, tip), {0, 0, 1});
    const Vector3 right = normalized(Vector3CrossProduct(cameraUp, front), {1, 0, 0});
    const Vector3 up = Vector3CrossProduct(front, right);
    const float size = .026F + urgency * .009F;
    beginBatch();
    rlNormal3f(front.x, front.y, front.z);
    disc(tip, right, up, size * 2.6F, {255, 171, 51, 105}, {255, 118, 30, 0});
    const float curl = .28F * std::sin(time * 13);
    flame(tip, right, up, size, size * (2.2F + .3F * std::sin(time * 17)), curl,
          {255, 142, 26, 255});
    flame(Vector3Add(tip, Vector3Scale(front, .001F)), right, up,
          size * .55F, size * 1.5F, curl, {255, 247, 179, 255});
    if (detailed) {
        for (int i = 0; i < 4; ++i) {
            const float age = std::fmod(time * (1.8F + urgency) + float(i) * .25F, 1.0F);
            const float angle = float(i) * 2.4F + time * .65F;
            Vector3 spark = Vector3Add(tip, {std::cos(angle) * age * .13F,
                age * .17F - age * age * .09F, std::sin(angle) * age * .13F});
            const float width = size * .24F * (1 - age);
            disc(spark, right, up, width, {255, 243, 151, 255}, {255, 183, 57, 210}, 6);
        }
        // Each puff is three round lobes; no textured square smoke sprites.
        for (int i = 0; i < 3; ++i) {
            const float age = std::fmod(time * .85F + float(i) / 3, 1.0F);
            Vector3 puff = Vector3Add(tip, {.035F * std::sin(time * 2 + float(i)),
                .05F + age * .22F, -.012F});
            const float radius = .012F + age * .025F;
            const auto tint = opacity({185, 164, 135, 110}, (1 - age) * (1 - age));
            for (int lobe = -1; lobe <= 1; ++lobe) {
                disc(Vector3Add(puff, Vector3Scale(right, float(lobe) * radius * .55F)),
                    right, up, radius * (lobe == 0 ? 1 : .75F), tint,
                    opacity(tint, .55F), 12);
            }
        }
    }
    endBatch();
}

} // namespace ian::bomb_drawing
