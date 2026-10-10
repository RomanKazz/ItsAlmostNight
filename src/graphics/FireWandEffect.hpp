#pragma once

#include "combat/IceWandSystem.hpp"

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace ian::fire_wand_effect {

inline float ease(float t) {
    t = std::clamp(t, 0.0F, 1.0F);
    return t * t * (3 - 2 * t);
}
inline Color opacity(Color color, float amount) {
    color.a = static_cast<unsigned char>(std::lround(color.a * std::clamp(amount, 0.0F, 1.0F)));
    return color;
}
inline Vector3 normalized(Vector3 value, Vector3 fallback) {
    return Vector3LengthSqr(value) > .000001F ? Vector3Normalize(value) : fallback;
}
inline void beginBatch(bool depthWrite = false) {
    rlDrawRenderBatchActive();
    BeginBlendMode(BLEND_ALPHA);
    if (!depthWrite) rlDisableDepthMask();
    else rlEnableDepthMask();
    rlDisableBackfaceCulling();
    rlSetTexture(0);
    rlBegin(RL_TRIANGLES);
}
inline void endBatch() {
    rlEnd(); rlDrawRenderBatchActive();
    rlEnableBackfaceCulling(); rlEnableDepthMask(); EndBlendMode();
}
inline void vertex(Vector3 p, Color color) {
    rlColor4ub(color.r, color.g, color.b, color.a);
    rlTexCoord2f(0, 0); rlVertex3f(p.x, p.y, p.z);
}
inline Vector3 planePoint(Vector3 center, Vector3 right, Vector3 up, float x, float y) {
    return Vector3Add(center, Vector3Add(Vector3Scale(right, x), Vector3Scale(up, y)));
}
inline void disc(Vector3 center, Vector3 right, Vector3 up, float radius,
                 Color core, Color edge, int segments = 20) {
    for (int i = 0; i < segments; ++i) {
        const float a = static_cast<float>(i) * 2 * PI / static_cast<float>(segments);
        const float b = static_cast<float>(i + 1) * 2 * PI / static_cast<float>(segments);
        vertex(center, core);
        vertex(planePoint(center, right, up, std::cos(a) * radius, std::sin(a) * radius), edge);
        vertex(planePoint(center, right, up, std::cos(b) * radius, std::sin(b) * radius), edge);
    }
}

// Rounded flame lobe: a broad base and a curling, narrow tip. Direction is a
// plane basis, so it works for rising staff fire and the moving comet's wake.
inline void flame(Vector3 center, Vector3 side, Vector3 direction,
                  float width, float length, float curl, Color color) {
    constexpr std::array<Vector2, 12> outline{{
        {0, -.08F}, {-.72F, -.04F}, {-1, .16F}, {-.91F, .38F},
        {-.58F, .58F}, {-.25F, .79F}, {.12F, 1}, {.12F, .76F},
        {.48F, .56F}, {.83F, .4F}, {1, .18F}, {.72F, -.04F}}};
    const Vector3 fan = planePoint(center, side, direction, 0, length * .24F);
    const auto point = [&](Vector2 p) {
        return planePoint(center, side, direction,
            (p.x + curl * p.y * p.y) * width, p.y * length);
    };
    for (std::size_t i = 0; i < outline.size(); ++i) {
        vertex(fan, color); vertex(point(outline[i]), color);
        vertex(point(outline[(i + 1) % outline.size()]), color);
    }
}

inline void orb(Vector3 center, Vector3 right, Vector3 up, Vector3 front,
                float radius, double time, float amount = 1) {
    // Solid layered silhouettes read like a lit ball without transparent wire
    // rings. Each layer sits on the camera-facing hemisphere.
    rlNormal3f(front.x, front.y, front.z);
    const float pulse = 1 + .035F * static_cast<float>(std::sin(time * 9));
    radius *= pulse;
    disc(center, right, up, radius, opacity({255, 113, 16, 255}, amount),
         opacity({221, 47, 16, 255}, amount));
    Vector3 inner = Vector3Add(center, Vector3Scale(front, radius * .08F));
    inner = planePoint(inner, right, up, -radius * .12F, radius * .12F);
    disc(inner, right, up, radius * .78F, opacity({255, 235, 119, 255}, amount),
         opacity({255, 166, 32, 255}, amount));
    Vector3 core = Vector3Add(inner, Vector3Scale(front, radius * .04F));
    disc(core, right, up, radius * .39F, opacity({255, 255, 222, 255}, amount),
         opacity({255, 237, 142, 255}, amount));
}

// Keep the original layered cartoon core. The wake and radial impact petals
// are gone; hits use the existing explosion cloud instead.
inline void emitProjectile(const IceWandProjectile& projectile, Vector3 camera,
                           double time, float interpolation) {
    if (!projectile.active || projectile.element != WandElement::Fire || projectile.radius <= 0) return;
    const auto point = [](Vec3 p) {
        return Vector3{static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(p.z)};
    };
    const Vector3 head = Vector3Lerp(point(projectile.previousPosition), point(projectile.position),
        std::clamp(interpolation, 0.0F, 1.0F));
    const Vector3 front = normalized(Vector3Subtract(camera, head), {0, 0, 1});
    const Vector3 right = normalized(Vector3CrossProduct({0, 1, 0}, front), {1, 0, 0});
    const Vector3 up = normalized(Vector3CrossProduct(front, right), {0, 1, 0});
    const float radius = static_cast<float>(projectile.radius) * 1.18F;
    const double clock = time + static_cast<double>(projectile.id.index % 97U) * .13;
    orb(head, right, up, front, radius, clock);
}

inline void emitStaff(Vector3 tip, double time, float charge, float recoil, bool detailed,
                      Vector3 right = {1, 0, 0}, Vector3 up = {0, 1, 0},
                      Vector3 front = {0, 0, 1}) {
    charge = ease(charge);
    const float flash = recoil > 0 ? 1 - ease((1 - recoil) / .55F) : 0;
    const float radius = .026F + charge * .035F + flash * .025F;
    // Place the flame just outside the authored crystal, facing the camera.
    tip = Vector3Add(tip, Vector3Scale(front, .075F));
    disc(tip, right, up, radius * 2.8F,
         opacity({255, 150, 24, 70}, .35F + charge + flash), {255, 86, 12, 0});
    for (int lobe = -1; lobe <= 1; ++lobe) {
        const float f = static_cast<float>(lobe);
        const Vector3 base = Vector3Add(tip, {f * radius * .4F, 0, -.002F});
        const float curl = .3F * static_cast<float>(std::sin(time * 9 + f * 2));
        const float length = radius * (1.6F + charge + flash * .8F);
        flame(base, {1, 0, 0}, {0, 1, 0}, radius * .5F, length, curl, {247, 88, 18, 240});
        flame(base, {1, 0, 0}, {0, 1, 0}, radius * .3F, length * .7F, curl, {255, 209, 74, 250});
    }
    orb(tip, right, up, front, radius, time);
    if (detailed && charge > .01F) {
        for (int ember = 0; ember < 5; ++ember) {
            const float f = static_cast<float>(ember);
            const float p = static_cast<float>(std::fmod(time * 2 + f * .2, 1.0));
            const float angle = f * 2.39996F + static_cast<float>(time * 2);
            const float distance = (.12F * (1 - p) + radius) * charge;
            disc(planePoint(Vector3Add(tip, Vector3Scale(front, .01F)), right, up,
                            std::cos(angle) * distance, std::sin(angle) * distance),
                right, up, .006F * std::sin(p * PI),
                {255, 246, 159, 230}, {255, 171, 36, 150}, 8);
        }
    }
}

} // namespace ian::fire_wand_effect
