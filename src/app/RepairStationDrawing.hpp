#pragma once

#include "game/Simulation.hpp"

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <cmath>

namespace ian::repair_station_drawing {

// A soft wave travels out to the actual repair radius. Sample every band on the
// terrain so the wide effect follows hills instead of cutting through them.
inline void drawPulses(const SimulationSnapshot& snapshot, const Camera3D& camera,
    const TerrainHeightfield& terrain, bool reduceFlashes) {
    constexpr int Segments = 64;
    constexpr std::array<float, 5> Bands{-1, -.5F, 0, .5F, 1};
    constexpr std::array<float, 5> Opacity{0, .4F, 1, .4F, 0};
    const Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    int visible = 0;
    rlBegin(RL_TRIANGLES);
    for (const auto& station : snapshot.buildings) {
        if (station.type != BuildingType::RepairStation) continue;
        if (!std::ranges::any_of(snapshot.repairStationLinks, [&](const auto& link) {
                return link.stationId == station.id;
            })) continue;
        const Vec3 position = buildingWorldPosition(station);
        const Vector3 center{float(position.x), float(position.y), float(position.z)};
        const auto offset = Vector3Subtract(center, camera.position);
        const float range = static_cast<float>(repairStationRadius(station.level));
        if (Vector3LengthSqr(offset) > 50 * 50 || Vector3DotProduct(offset, forward) < -range) continue;
        if (++visible > 16) break;
        const float progress = std::clamp(static_cast<float>(snapshot.repairStationPulseAge / RepairStationVisualDuration), 0.0F, 1.0F);
        const auto smooth = [](float value) {
            const float t = std::clamp(value, 0.0F, 1.0F);
            return t * t * (3 - 2 * t);
        };
        const float envelope = smooth(progress / .16F) * (1 - smooth((progress - .68F) / .32F));
        const float radius = range * (.10F + .90F * progress);
        const float width = .16F + .025F * range;
        const auto point = [&](int segment, float band) {
            const float angle = float(segment) * (2 * PI / Segments);
            const float distance = std::max(.025F, radius + band * width);
            const float x = center.x + std::cos(angle) * distance;
            const float z = center.z + std::sin(angle) * distance;
            return Vector3{x, float(terrain.getHeight(x, z)) + .075F, z};
        };
        const auto emit = [&](Vector3 vertex, float opacity) {
            rlColor4ub(83, 235, 225, static_cast<unsigned char>(
                (reduceFlashes ? 45 : 90) * envelope * opacity));
            rlVertex3f(vertex.x, vertex.y, vertex.z);
        };
        std::array<Vector3, Bands.size()> previous{};
        for (std::size_t band = 0; band < Bands.size(); ++band) previous[band] = point(0, Bands[band]);
        for (int segment = 1; segment <= Segments; ++segment) {
            std::array<Vector3, Bands.size()> next{};
            for (std::size_t band = 0; band < Bands.size(); ++band) next[band] = point(segment, Bands[band]);
            for (std::size_t band = 0; band + 1 < Bands.size(); ++band) {
                emit(previous[band], Opacity[band]); emit(next[band], Opacity[band]);
                emit(next[band + 1], Opacity[band + 1]);
                emit(previous[band], Opacity[band]); emit(next[band + 1], Opacity[band + 1]);
                emit(previous[band + 1], Opacity[band + 1]);
            }
            previous = next;
        }
    }
    rlEnd();
}

// One untextured batch, a fixed visual cap, and no per-frame particle allocations.
inline void draw(const SimulationSnapshot& snapshot, const Camera3D& camera, bool reduceFlashes,
    const TerrainHeightfield& terrain) {
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
    drawPulses(snapshot, camera, terrain, reduceFlashes);
    rlBegin(RL_QUADS);
    const float pulseFade = std::clamp(static_cast<float>(snapshot.repairStationPulseAge / .12), 0.0F, 1.0F) *
        std::clamp(static_cast<float>((RepairStationVisualDuration - snapshot.repairStationPulseAge) / .3), 0.0F, 1.0F);
    const auto rectangle = [&](Vector3 center, float halfWidth, float halfHeight, Color color) {
        rlColor4ub(color.r, color.g, color.b, static_cast<unsigned char>(color.a * pulseFade));
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
