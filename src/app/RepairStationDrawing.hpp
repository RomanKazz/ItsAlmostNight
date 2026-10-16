#pragma once

#include "game/Simulation.hpp"

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <cmath>

namespace ian::repair_station_drawing {

// One untextured batch, a fixed visual cap, and no per-frame particle allocations.
inline void draw(const SimulationSnapshot& snapshot, const Camera3D& camera, bool reduceFlashes) {
    if (snapshot.repairStationLinks.empty()) return;
    const Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    const Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera.up));
    const Vector3 up = Vector3Normalize(Vector3CrossProduct(right, forward));
    const auto vector = [](Vec3 value) { return Vector3{float(value.x), float(value.y), float(value.z)}; };
    rlDrawRenderBatchActive();
    BeginBlendMode(BLEND_ALPHA);
    rlDisableDepthMask();
    rlDisableBackfaceCulling();
    rlSetTexture(0);
    rlBegin(RL_QUADS);
    const auto rectangle = [&](Vector3 center, float halfWidth, float halfHeight, Color color) {
        rlColor4ub(color.r, color.g, color.b, color.a);
        for (const auto corner : std::array<Vector2, 4>{{{-1,-1},{1,-1},{1,1},{-1,1}}}) {
            const auto point = Vector3Add(center, Vector3Add(
                Vector3Scale(right, corner.x * halfWidth), Vector3Scale(up, corner.y * halfHeight)));
            rlVertex3f(point.x, point.y, point.z);
        }
    };
    const float time = static_cast<float>(snapshot.elapsedSeconds);
    for (const auto& link : snapshot.repairStationLinks) {
        const auto origin = vector(link.origin);
        const auto target = vector(link.target);
        const float seed = float(link.targetId.index % 37U) * .173F;
        for (int i = 0; i < 3; ++i) {
            const float progress = std::fmod(time * .8F + seed + float(i) / 3, 1.0F);
            auto position = Vector3Lerp(origin, target, progress);
            position.y += std::sin(progress * PI) * .5F;
            const float fade = std::sin(progress * PI);
            const float size = .055F + .022F * fade;
            const Color tint{126, 248, 243, static_cast<unsigned char>((reduceFlashes ? 130 : 205) * fade)};
            rectangle(position, size, size * .25F, tint);
            rectangle(position, size * .25F, size, tint);
        }
        const float size = .08F + (reduceFlashes ? 0 : .015F * std::sin(time * 4 + seed));
        rectangle(target, size, size * .24F, {150, 255, 217, 160});
        rectangle(target, size * .24F, size, {150, 255, 217, 160});
    }
    rlEnd();
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
    EndBlendMode();
}

} // namespace ian::repair_station_drawing
