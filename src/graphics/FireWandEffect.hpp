#pragma once

#include "combat/IceWandSystem.hpp"

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace ian::fire_wand_effect {

enum class Pass { Trail, Body };

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

inline Vector3 trailPoint(const IceWandProjectile& projectile, std::size_t index, float alpha) {
    const auto point = [](Vec3 p) {
        return Vector3{static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(p.z)};
    };
    if (index == 0) return Vector3Lerp(point(projectile.previousPosition), point(projectile.position), alpha);
    return Vector3Lerp(point(projectile.trail[index]), point(projectile.trail[index - 1]), alpha);
}

inline void emitProjectile(const IceWandProjectile& projectile, Vector3 camera,
                           double time, float interpolation, bool detailed, Pass pass) {
    if (!projectile.active || projectile.element != WandElement::Fire || projectile.radius <= 0) return;
    const float alpha = std::clamp(interpolation, 0.0F, 1.0F);
    const Vector3 head = trailPoint(projectile, 0, alpha);
    const Vector3 front = normalized(Vector3Subtract(camera, head), {0, 0, 1});
    const Vector3 right = normalized(Vector3CrossProduct({0, 1, 0}, front), {1, 0, 0});
    const Vector3 up = normalized(Vector3CrossProduct(front, right), {0, 1, 0});
    const float radius = static_cast<float>(projectile.radius) * 1.18F;
    const double clock = time + static_cast<double>(projectile.id.index % 97U) * .13;
    if (pass == Pass::Body) {
        orb(head, right, up, front, radius, clock);
        return;
    }
    disc(head, right, up, radius * 2.1F, {255, 141, 22, 46}, {255, 91, 11, 0});
    const std::size_t count = std::min(projectile.trailCount, projectile.trail.size());
    std::array<Vector3, IceWandTrailPointCount> points{}, sides{};
    std::array<float, IceWandTrailPointCount> widths{};
    for (std::size_t i = 0; i < count; ++i) {
        points[i] = trailPoint(projectile, i, alpha);
        const Vector3 tangent = Vector3Subtract(trailPoint(projectile, i > 0 ? i - 1 : i, alpha),
            trailPoint(projectile, std::min(i + 1, count - 1), alpha));
        sides[i] = normalized(Vector3CrossProduct(tangent, Vector3Subtract(camera, points[i])), right);
        const float age = static_cast<float>(i) / static_cast<float>(std::max<std::size_t>(1, count - 1));
        const float wave = static_cast<float>(std::sin(clock * 13 - age * 17));
        points[i] = Vector3Add(points[i], Vector3Scale(sides[i], wave * radius * age * .18F));
        widths[i] = radius * (1 - age) * (1 - age * .25F) * (1 + age * .18F * wave);
    }
    for (int layer = 0; layer < 3; ++layer) {
        const float scale = layer == 0 ? 1 : layer == 1 ? .65F : .27F;
        const Color hot = layer == 0 ? Color{246, 73, 16, 235} :
            layer == 1 ? Color{255, 177, 28, 245} : Color{255, 249, 170, 255};
        const Color cool = layer == 0 ? Color{198, 39, 23, 0} :
            layer == 1 ? Color{246, 103, 21, 0} : Color{255, 174, 45, 0};
        for (std::size_t i = 1; i < count; ++i) {
            if (Vector3DistanceSqr(points[i - 1], points[i]) < .000001F) continue;
            const float a = static_cast<float>(i - 1) / static_cast<float>(count - 1);
            const float b = static_cast<float>(i) / static_cast<float>(count - 1);
            const Color ca = ColorLerp(hot, cool, ease(a)), cb = ColorLerp(hot, cool, ease(b));
            const Vector3 offset = Vector3Scale(front, radius * .006F * static_cast<float>(layer));
            const Vector3 leftA = Vector3Add(Vector3Subtract(points[i - 1], Vector3Scale(sides[i - 1], widths[i - 1] * scale)), offset);
            const Vector3 rightA = Vector3Add(Vector3Add(points[i - 1], Vector3Scale(sides[i - 1], widths[i - 1] * scale)), offset);
            const Vector3 leftB = Vector3Add(Vector3Subtract(points[i], Vector3Scale(sides[i], widths[i] * scale)), offset);
            const Vector3 rightB = Vector3Add(Vector3Add(points[i], Vector3Scale(sides[i], widths[i] * scale)), offset);
            vertex(leftA, ca); vertex(rightA, ca); vertex(rightB, cb);
            vertex(leftA, ca); vertex(rightB, cb); vertex(leftB, cb);
        }
    }
    const Vector3 velocity{static_cast<float>(projectile.velocity.x),
        static_cast<float>(projectile.velocity.y), static_cast<float>(projectile.velocity.z)};
    const Vector3 wake = normalized(Vector3Negate(velocity), up);
    const Vector3 flameSide = normalized(Vector3CrossProduct(wake, front), right);
    for (int lobe = 0; lobe < 3; ++lobe) {
        const float f = static_cast<float>(lobe - 1);
        Vector3 base = Vector3Add(head, Vector3Scale(flameSide, radius * f * .55F));
        base = Vector3Add(base, Vector3Scale(wake, radius * .25F));
        const float curl = .35F * static_cast<float>(std::sin(clock * 11 + f * 2));
        const float length = radius * (1.7F + .25F * static_cast<float>(std::sin(clock * 8 + f)));
        flame(base, flameSide, wake, radius * .46F, length, curl, {245, 82, 18, 245});
        flame(base, flameSide, wake, radius * .27F, length * .75F, curl, {255, 199, 55, 250});
    }
    if (detailed && count > 3) {
        for (std::size_t ember = 0; ember < 4; ++ember) {
            const float cycle = static_cast<float>(std::fmod(clock * 2.5 + static_cast<double>(ember) * .25, 1.0));
            const std::size_t i = std::min(count - 1, 2 + ember * 3);
            Vector3 center = Vector3Add(points[i], Vector3Scale(sides[i], radius * (.6F + cycle) * (ember % 2 == 0 ? 1 : -1)));
            center = Vector3Add(center, Vector3Scale(up, radius * cycle * .5F));
            const float fade = std::sin(cycle * PI);
            disc(center, right, up, radius * .12F * fade,
                opacity({255, 247, 161, 245}, fade), opacity({255, 156, 24, 170}, fade), 8);
        }
    }
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

inline void emitImpact(Vector3 origin, Vector3 right, Vector3 up, Vector3 front,
                       float progress, float scale, bool detailed) {
    progress = std::clamp(progress, 0.0F, 1.0F);
    const float fade = 1 - ease((progress - .35F) / .65F);
    if (fade < .001F || scale <= 0) return;
    const float travel = 1 - (1 - progress) * (1 - progress);
    disc(origin, right, up, scale * (.25F + travel * 1.6F),
         opacity({255, 149, 26, 75}, fade), {255, 75, 14, 0});
    for (int i = 0; i < (detailed ? 9 : 5); ++i) {
        const float f = static_cast<float>(i);
        const float angle = f * 2.39996F;
        const Vector3 direction = Vector3Add(Vector3Scale(right, std::cos(angle)), Vector3Scale(up, std::sin(angle)));
        const Vector3 side = Vector3Add(Vector3Scale(right, -std::sin(angle)), Vector3Scale(up, std::cos(angle)));
        Vector3 center = Vector3Add(origin, Vector3Scale(direction, scale * travel * .8F));
        center = Vector3Add(center, {0, progress * scale * .7F, 0});
        const float curl = .4F * std::sin(f * 2 + progress * 7);
        const float length = scale * (.45F + .15F * std::sin(f)) * (1 + travel) * fade;
        flame(center, side, direction, scale * .24F * fade, length, curl, opacity({247, 78, 18, 250}, fade));
        flame(Vector3Add(center, Vector3Scale(front, .005F)), side, direction,
            scale * .14F * fade, length * .72F, curl, opacity({255, 201, 58, 255}, fade));
    }
    Vector3 core = Vector3Add(origin, Vector3Scale(front, .01F));
    core.y += progress * scale * .5F;
    orb(core, right, up, front, scale * (.3F + travel * .5F) * fade, progress, fade);
}

} // namespace ian::fire_wand_effect
