#pragma once

#include "app/AppEffectDrawing.hpp"

#include <array>

namespace ian::app_freeze_drawing {

enum class Pass { Ice, Atmosphere };

inline void beginBatch() {
    rlDrawRenderBatchActive();
    BeginBlendMode(BLEND_ALPHA);
    rlDisableDepthMask();
    rlDisableBackfaceCulling();
    rlSetTexture(0);
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
    color.a = app_effect_drawing::atmosphereAlpha(
        static_cast<float>(color.a) / 255.0F * amount);
    return color;
}

inline float ease(float t) {
    t = std::clamp(t, 0.0F, 1.0F);
    return t * t * (3.0F - 2.0F * t);
}

inline void vertex(Vector3 point, Color color) {
    rlColor4ub(color.r, color.g, color.b, color.a);
    rlTexCoord2f(0, 0);
    rlVertex3f(point.x, point.y, point.z);
}

inline void triangle(Vector3 a, Vector3 b, Vector3 c, Color color) {
    vertex(a, color); vertex(b, color); vertex(c, color);
}

inline void quad(Vector3 a, Vector3 b, Vector3 c, Vector3 d, Color color) {
    triangle(a, b, c, color); triangle(a, c, d, color);
}

inline Vector3 radial(Vector3 center, float angle, float radius, float height) {
    return {center.x + std::cos(angle) * radius, center.y + height,
            center.z + std::sin(angle) * radius};
}

// Chunky hexagonal ice, with a bright bevel and distinct cyan shaded faces.
// Shared geometry for the frozen collar and the shards released on impact.
inline void crystal(Vector3 base, float width, float height, float yaw,
                    float lean, float amount) {
    const Vector3 tip{base.x + std::cos(yaw) * lean, base.y + height,
                      base.z + std::sin(yaw) * lean};
    const Vector3 shoulder{base.x + std::cos(yaw) * lean * .55F,
        base.y + height * .65F, base.z + std::sin(yaw) * lean * .55F};
    constexpr std::array<Color, 6> faces{{
        {73, 162, 237, 225}, {107, 208, 254, 235}, {192, 246, 255, 240},
        {131, 221, 255, 235}, {68, 172, 237, 225}, {88, 191, 246, 230}}};
    for (int face = 0; face < 6; ++face) {
        const float angle = yaw + static_cast<float>(face) * PI / 3;
        const float next = angle + PI / 3;
        const Vector3 a = radial(base, angle, width * .75F, 0);
        const Vector3 b = radial(base, next, width * .75F, 0);
        const Vector3 c = radial(shoulder, next, width, 0);
        const Vector3 d = radial(shoulder, angle, width, 0);
        quad(a, b, c, d, opacity(faces[static_cast<std::size_t>(face)], amount));
        triangle(d, c, tip, opacity(face < 3 ? Color{223, 252, 255, 245}
                                           : Color{115, 212, 255, 235}, amount));
    }
}

inline Vector3 planePoint(Vector3 center, Vector3 right, Vector3 up, Vector2 p) {
    return Vector3Add(center, Vector3Add(Vector3Scale(right, p.x), Vector3Scale(up, p.y)));
}

inline void stroke(Vector3 center, Vector3 right, Vector3 up,
                   Vector2 a, Vector2 b, float width, Color color) {
    const Vector2 delta{b.x - a.x, b.y - a.y};
    const float length = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    if (length <= .00001F) return;
    const Vector2 normal{-delta.y * width / length, delta.x * width / length};
    quad(planePoint(center, right, up, {a.x - normal.x, a.y - normal.y}),
         planePoint(center, right, up, {b.x - normal.x, b.y - normal.y}),
         planePoint(center, right, up, {b.x + normal.x, b.y + normal.y}),
         planePoint(center, right, up, {a.x + normal.x, a.y + normal.y}), color);
}

inline void snowflake(Vector3 center, Vector3 right, Vector3 up,
                      float size, float rotation, float amount, bool branches) {
    const Color color = opacity({223, 250, 255, 240}, amount);
    for (int arm = 0; arm < 6; ++arm) {
        const float angle = rotation + static_cast<float>(arm) * PI / 3;
        const Vector2 direction{std::cos(angle), std::sin(angle)};
        stroke(center, right, up, {}, {direction.x * size, direction.y * size}, size * .07F, color);
        if (!branches) continue;
        const Vector2 join{direction.x * size * .60F, direction.y * size * .60F};
        for (const float sign : {-1.0F, 1.0F}) {
            const float branch = angle + sign * PI * .30F;
            stroke(center, right, up, join,
                {join.x + std::cos(branch) * size * .28F,
                 join.y + std::sin(branch) * size * .28F}, size * .045F, color);
        }
    }
}

inline void frostPatch(Vector3 center, float radius, float amount) {
    center.y += .025F;
    for (int edge = 0; edge < 12; ++edge) {
        const float angle = static_cast<float>(edge) * PI / 6;
        vertex(center, opacity({132, 229, 255, 72}, amount));
        vertex(radial(center, angle, radius * (edge % 2 == 0 ? 1 : .88F), 0),
               opacity({187, 244, 255, 25}, amount));
        vertex(radial(center, angle + PI / 6, radius * (edge % 2 == 0 ? .88F : 1), 0),
               opacity({187, 244, 255, 25}, amount));
    }
}

inline void mist(Vector3 center, Vector3 right, float size, float amount) {
    for (int lobe = -1; lobe <= 1; ++lobe) {
        const Vector3 p = Vector3Add(center,
            Vector3Scale(right, static_cast<float>(lobe) * size * .62F));
        app_effect_drawing::emitSoftBillboardDisc(p, right, {0, 1, 0},
            size, opacity({188, 233, 255, 38}, amount), 10);
    }
}

inline void emitFrozen(Vector3 position, Vector3 right, Vector3 towardCamera,
                       float radius, float bodyHeight, std::uint32_t seed,
                       double time, float amount, bool thawing, bool detailed, Pass pass) {
    amount = std::clamp(amount, 0.0F, 1.0F);
    if (amount <= .001F) return;
    const float growth = ease(amount);
    const float collarHeight = bodyHeight * (.12F + .36F * growth);
    const float release = thawing ? (1 - growth) : 0;
    const float identity = static_cast<float>(seed % 97U) * .17F;
    if (pass == Pass::Ice) {
        frostPatch(position, radius * (1.1F + release * .25F), amount);
        constexpr std::array<float, 8> rim{{.83F, 1, .73F, .95F, .80F, .92F, .71F, .96F}};
        for (int edge = 0; edge < 8; ++edge) {
            const float angle = static_cast<float>(edge) * PI / 4 + identity;
            const float next = angle + PI / 4;
            const Vector3 a = radial(position, angle, radius * .97F, .03F);
            const Vector3 b = radial(position, next, radius * .97F, .03F);
            const Vector3 c = radial(position, next, radius * .90F,
                collarHeight * rim[static_cast<std::size_t>((edge + 1) % 8)]);
            const Vector3 d = radial(position, angle, radius * .90F,
                collarHeight * rim[static_cast<std::size_t>(edge)]);
            quad(a, b, c, d, opacity(edge % 2 == 0 ? Color{80, 181, 245, 118}
                                                               : Color{143, 224, 255, 100}, amount));
            Vector3 innerC = c, innerD = d;
            innerC.y -= bodyHeight * .025F; innerD.y -= bodyHeight * .025F;
            quad(innerD, innerC, c, d, opacity({212, 249, 255, 195}, amount));
        }
        for (int shard = 0; shard < (detailed ? 6 : 3); ++shard) {
            const float s = static_cast<float>(shard);
            const float angle = identity + s * 2.39996F;
            Vector3 base = radial(position, angle, radius * (.86F + release * .48F), .04F);
            const float variation = app_effect_drawing::atmosphereUnit(seed + static_cast<std::uint32_t>(shard) * 37U);
            crystal(base, radius * (.14F + variation * .08F),
                collarHeight * (.66F + variation * .58F), angle,
                radius * (.10F + release * .18F), amount);
        }
        // A broad frost glint on the front of the shell, rather than thin
        // rings/lines targeting enemies. They shrink continuously on thaw.
        Vector3 front = Vector3Add(position, Vector3Scale(towardCamera, radius * 1.015F));
        front.y += collarHeight * .47F;
        snowflake(front, right, {0, 1, 0}, radius * .15F * growth,
                  .2F, amount * .8F, detailed);
        return;
    }
    if (!detailed) return;
    for (int flake = 0; flake < 5; ++flake) {
        const float f = static_cast<float>(flake);
        const float p = static_cast<float>(std::fmod(time * (.28 + f * .03) + identity + f * .19, 1.0));
        const float fade = std::sin(p * PI) * amount;
        Vector3 center = Vector3Add(position,
            Vector3Scale(towardCamera, radius * (1.02F + .10F * static_cast<float>(std::sin(time + f)))));
        center = Vector3Add(center, Vector3Scale(right,
            radius * (.9F * std::sin(identity + f * 2.1F) + .08F * static_cast<float>(std::sin(time * 1.3 + f)))));
        center.y += bodyHeight * (.45F + (1 - p) * .48F);
        snowflake(center, right, {0, 1, 0}, radius * (.065F + f * .007F),
                  f + .2F * static_cast<float>(std::sin(time + f)), fade, true);
    }
    for (int puff = 0; puff < 2; ++puff) {
        const float p = static_cast<float>(std::fmod(time * .45 + identity + puff * .5, 1.0));
        Vector3 center = Vector3Add(position, Vector3Scale(right, radius * std::sin(identity + p * 5)));
        center.y += collarHeight * .8F + p * bodyHeight * .28F;
        mist(center, right, radius * (.16F + p * .14F), amount * std::sin(p * PI));
    }
}

inline void emitImpact(Vector3 origin, Vector3 right, Vector3 up,
                       float progress, float scale, std::uint32_t seed,
                       bool crack, bool detailed, Pass pass) {
    progress = std::clamp(progress, 0.0F, 1.0F);
    const float fade = 1 - ease((progress - .3F) / .7F);
    if (fade <= .001F) return;
    const float travel = 1 - (1 - progress) * (1 - progress);
    const int count = crack ? 5 : (detailed ? 12 : 6);
    if (pass == Pass::Ice) {
        for (int shard = 0; shard < count; ++shard) {
            const auto id = seed + static_cast<std::uint32_t>(shard) * 61U;
            const float random = app_effect_drawing::atmosphereUnit(id);
            const float angle = static_cast<float>(shard) * 2.39996F + random * .4F;
            const float reach = scale * (crack ? .6F : 1.4F) * (.4F + random) * travel;
            Vector3 base = radial(origin, angle, reach,
                scale * (.15F + progress * (1.2F + random) - progress * progress * 2));
            crystal(base, scale * (.055F + random * .07F) * fade,
                scale * (.16F + random * .17F) * fade, angle + progress * 4,
                scale * .10F * std::sin(progress * PI), fade);
        }
        snowflake(origin, right, up, scale * (.12F + travel * .52F),
                  .1F, (1 - ease(progress / .25F)) * .95F, true);
    } else if (detailed) {
        for (int puff = 0; puff < (crack ? 2 : 4); ++puff) {
            const float angle = static_cast<float>(puff) * PI * .5F;
            Vector3 center = radial(origin, angle, scale * travel * .6F, progress * scale * .5F);
            mist(center, right, scale * (.12F + travel * .22F), fade);
        }
    }
}

} // namespace ian::app_freeze_drawing
