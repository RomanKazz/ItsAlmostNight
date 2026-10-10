#pragma once

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace ian::app_burn_drawing {

enum class Pass { Glow, Smoke, Flame };

// All layers use one batch per pass. Test the opaque world, never write depth:
// overlapping transparent flames must not cut holes in smoke or other VFX.
inline void beginBatch(Pass pass) {
    rlDrawRenderBatchActive();
    BeginBlendMode(pass == Pass::Glow ? BLEND_ADDITIVE : BLEND_ALPHA);
    rlDisableDepthMask();
    rlDisableBackfaceCulling();
    rlBegin(RL_TRIANGLES);
}

inline void endBatch() {
    rlEnd();
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
    EndBlendMode();
}

inline Color opacity(Color color, float amount) {
    color.a = static_cast<unsigned char>(std::lround(
        static_cast<float>(color.a) * std::clamp(amount, 0.0F, 1.0F)));
    return color;
}

inline void vertex(Vector3 point, Color color) {
    rlColor4ub(color.r, color.g, color.b, color.a);
    rlVertex3f(point.x, point.y, point.z);
}

inline Vector3 billboardPoint(Vector3 base, Vector3 right, Vector2 point) {
    base = Vector3Add(base, Vector3Scale(right, point.x));
    base.y += point.y;
    return base;
}

inline Vector2 bezier(Vector2 a, Vector2 b, Vector2 c, Vector2 d, float t) {
    const float u = 1.0F - t;
    return {u*u*u*a.x + 3*u*u*t*b.x + 3*u*t*t*c.x + t*t*t*d.x,
            u*u*u*a.y + 3*u*u*t*b.y + 3*u*t*t*c.y + t*t*t*d.y};
}

inline void disc(Vector3 center, Vector3 right, float width, float height,
                 Color core, Color edge, int segments = 12) {
    Vector3 previous = billboardPoint(center, right, {width, 0});
    for (int step = 1; step <= segments; ++step) {
        const float angle = static_cast<float>(step) * 2.0F * PI /
                            static_cast<float>(segments);
        const Vector3 next = billboardPoint(center, right,
            {std::cos(angle) * width, std::sin(angle) * height});
        vertex(center, core); vertex(previous, edge); vertex(next, edge);
        previous = next;
    }
}

// A rounded, asymmetric teardrop rather than a fading triangle. Nested solid
// silhouettes keep the orange rim and yellow core readable even in daylight.
inline void tongue(Vector3 base, Vector3 right, float width, float height,
                   float curl, Color bottom, Color top, int steps) {
    const std::array<Vector2, 19> curve{{
        {0, -0.03F}, {-0.70F, -0.10F}, {-1, 0.02F}, {-1, 0.18F},
        {-1, 0.36F}, {-0.70F, 0.48F}, {-0.62F, 0.62F},
        {-0.52F, 0.80F}, {0.02F + curl, 0.91F}, {0.24F + curl, 1},
        {0.10F + curl, 0.80F}, {0.48F, 0.69F}, {0.55F, 0.52F},
        {0.64F, 0.35F}, {1, 0.30F}, {0.95F, 0.16F},
        {0.90F, -0.03F}, {0.26F, -0.06F}, {0, -0.03F},
    }};
    const auto point = [&](Vector2 p) {
        return billboardPoint(base, right, {p.x * width, p.y * height});
    };
    const auto color = [&](float y) {
        const float t = std::clamp(y, 0.0F, 1.0F);
        return Color{
            static_cast<unsigned char>(bottom.r + (top.r - bottom.r) * t),
            static_cast<unsigned char>(bottom.g + (top.g - bottom.g) * t),
            static_cast<unsigned char>(bottom.b + (top.b - bottom.b) * t),
            bottom.a};
    };
    const Vector3 center = point({0, 0.32F});
    Vector2 previous = curve.front();
    for (std::size_t segment = 0; segment < 6; ++segment) {
        const std::size_t index = segment * 3;
        for (int step = 1; step <= steps; ++step) {
            const Vector2 next = bezier(curve[index], curve[index + 1],
                curve[index + 2], curve[index + 3],
                static_cast<float>(step) / static_cast<float>(steps));
            vertex(center, color(0.32F));
            vertex(point(previous), color(previous.y));
            vertex(point(next), color(next.y));
            previous = next;
        }
    }
}

inline float phase(double time, float speed, float seed) {
    return static_cast<float>(std::fmod(time * speed + seed, 2.0 * PI));
}

inline float cycle(double time, float speed, float seed) {
    return static_cast<float>(std::fmod(time * speed + seed, 1.0));
}

inline void emit(Vector3 position, Vector3 right, Vector3 toCamera,
                 float scale, std::uint32_t seed, double time, float amount,
                 bool detailed, Pass pass, float surfaceRadius = 0.0F) {
    if (amount <= 0.001F || scale <= 0.0F) return;
    const float identity = static_cast<float>(seed % 997U) * 0.137F;
    const float flicker = phase(time, 8.5F, identity);
    const Vector3 front = Vector3Add(position,
        Vector3Scale(toCamera, std::max(surfaceRadius, scale * 0.44F) + scale * 0.025F));
    if (pass == Pass::Glow) {
        Vector3 center = front;
        center.y += scale * 0.45F;
        disc(center, right, scale * 0.75F, scale * 0.86F,
             opacity({255, 97, 16, 32}, amount * (0.9F + 0.1F * std::sin(flicker))),
             {255, 65, 8, 0}, 16);
        return;
    }
    if (pass == Pass::Smoke) {
        if (!detailed) return;
        for (int puff = 0; puff < 2; ++puff) {
            const float puffIndex = static_cast<float>(puff);
            const float p = cycle(time, 0.56F, identity + puffIndex * 0.5F);
            const float fade = std::sin(p * PI) * (1.0F - p) * amount;
            Vector3 center = billboardPoint(position, right,
                {scale * (std::sin(p * 5 + identity) * 0.21F),
                 scale * (1.10F + p * 1.15F)});
            const float radius = scale * (0.085F + p * 0.19F);
            // Three overlapping round lobes form a tiny cartoon smoke cloud.
            for (int lobe = -1; lobe <= 1; ++lobe) {
                Vector3 c = billboardPoint(center, right,
                    {static_cast<float>(lobe) * radius * 0.75F,
                     lobe == 0 ? radius * 0.18F : 0});
                const Color ink = opacity({83, 66, 69, 85}, fade);
                disc(c, right, radius, radius * 0.85F, ink,
                     opacity({123, 103, 97, 55}, fade), 10);
            }
        }
        return;
    }
    const int count = detailed ? 5 : 3;
    // Wide side flames first; a smaller central tongue leaves the face visible.
    constexpr std::array<float, 5> offsets{{-0.40F, 0.39F, -0.19F, 0.18F, 0}};
    for (int lobe = 0; lobe < count; ++lobe) {
        const float lobeIndex = static_cast<float>(lobe);
        const float p = phase(time, 7.2F + lobeIndex * 0.7F, identity + lobeIndex * 2.3F);
        const float x = !detailed && lobe == 2 ? 0.0F :
                        offsets[static_cast<std::size_t>(lobe)];
        Vector3 base = billboardPoint(front, right,
            {scale * (x + 0.025F * std::sin(p)), scale * (0.06F + lobeIndex * 0.022F)});
        const float width = scale * (lobe < 2 ? 0.235F : 0.19F) *
                            (1.0F + 0.10F * std::sin(p + 1.7F));
        const float height = scale * (lobe < 2 ? 1.10F : 0.77F) *
                             (1.0F + 0.12F * std::sin(p));
        const float curl = 0.31F * std::sin(phase(time,
            (7.2F + lobeIndex * 0.7F) * 0.8F, identity + lobeIndex * 2.3F + 0.5F));
        const int steps = detailed ? 4 : 3;
        const float fade = amount * 0.96F;
        tongue(base, right, width, height, curl,
               opacity({255, 92, 12, 255}, fade), {241, 56, 19, 255}, steps);
        base.y += height * 0.025F;
        tongue(base, right, width * 0.82F, height * 0.87F, curl * 0.85F,
               opacity({255, 188, 30, 255}, fade), {255, 126, 12, 255}, steps);
        tongue(base, right, width * 0.53F, height * 0.65F, curl * 0.6F,
               opacity({255, 244, 121, 255}, fade), {255, 209, 45, 255}, steps);
        tongue(base, right, width * 0.25F, height * 0.38F, curl * 0.3F,
               opacity({255, 255, 226, 255}, fade), {255, 244, 152, 255}, steps);
    }
    for (int ember = 0; ember < (detailed ? 5 : 2); ++ember) {
        const float emberIndex = static_cast<float>(ember);
        const float p = cycle(time, 0.83F + emberIndex * 0.07F,
                              identity + emberIndex * 0.191F);
        const float drift = phase(time, 3.0F, identity + emberIndex * 1.9F);
        const float fade = std::min(1.0F, p * 12) * (1 - p) * amount;
        Vector3 center = billboardPoint(front, right,
            {scale * (std::sin(drift) * 0.32F + std::sin(p * 5) * 0.10F),
             scale * (0.70F + p * 1.45F)});
        const float radius = scale * (0.025F + 0.017F * (1 - p));
        disc(center, right, radius, radius * (1.3F + 0.5F * std::sin(drift)),
             opacity({255, 249, 175, 255}, fade),
             opacity({255, 160, 22, 230}, fade), 8);
    }
}

} // namespace ian::app_burn_drawing
