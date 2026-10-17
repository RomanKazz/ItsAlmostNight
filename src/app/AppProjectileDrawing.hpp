#include "combat/AttackProjectile.hpp"
#pragma once

#include "combat/CannonSystem.hpp"

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace ian::app_projectile_drawing {

// Solid heads write depth; alpha ribbons only test the completed opaque world.
// Flush before changing the mask: rlEnd queues geometry rather than drawing it.
inline void beginProjectileBatch(bool transparent, int blendMode = BLEND_ALPHA) {
    rlDrawRenderBatchActive();
    BeginBlendMode(blendMode);
    if (transparent) rlDisableDepthMask();
    else rlEnableDepthMask();
    rlDisableBackfaceCulling();
    rlBegin(RL_TRIANGLES);
}

inline void endProjectileBatch() {
    rlEnd();
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
    EndBlendMode();
}

inline void emitProjectileVertex(Vector3 position, Color color) {
    rlColor4ub(color.r, color.g, color.b, color.a);
    rlVertex3f(position.x, position.y, position.z);
}

inline void emitProjectileQuad(
    Vector3 farLeft, Vector3 nearLeft,
    Vector3 nearRight, Vector3 farRight,
    Color farLeftColor, Color nearLeftColor,
    Color nearRightColor, Color farRightColor) {
    emitProjectileVertex(farLeft, farLeftColor);
    emitProjectileVertex(nearLeft, nearLeftColor);
    emitProjectileVertex(nearRight, nearRightColor);
    emitProjectileVertex(farLeft, farLeftColor);
    emitProjectileVertex(nearRight, nearRightColor);
    emitProjectileVertex(farRight, farRightColor);
}

inline void emitTurretMuzzleFlash(
    Vector3 muzzle, Vector3 direction,
    const Camera3D& camera, float fade) {
    Vector3 view = Vector3Subtract(camera.target, camera.position);
    view = Vector3LengthSqr(view) > 0.000001F
        ? Vector3Normalize(view) : Vector3{0.0F, 0.0F, -1.0F};
    Vector3 right = Vector3CrossProduct(view, camera.up);
    right = Vector3LengthSqr(right) > 0.000001F
        ? Vector3Normalize(right) : Vector3{1.0F, 0.0F, 0.0F};
    const Vector3 up = Vector3Normalize(
        Vector3CrossProduct(right, view));
    const Vector3 center = Vector3Add(
        muzzle, Vector3Scale(direction, 0.08F));
    const Color centerColor{255, 244, 187,
        static_cast<unsigned char>(225.0F * fade)};
    const Color edgeColor{255, 139, 38, 0};
    constexpr std::array<Vector2, 9> Circle{{
        {1.0F, 0.0F}, {0.7071068F, 0.7071068F},
        {0.0F, 1.0F}, {-0.7071068F, 0.7071068F},
        {-1.0F, 0.0F}, {-0.7071068F, -0.7071068F},
        {0.0F, -1.0F}, {0.7071068F, -0.7071068F},
        {1.0F, 0.0F},
    }};
    const float radius = 0.22F * fade;
    for (std::size_t index = 1; index < Circle.size(); ++index) {
        const auto point = [center, right, up, radius](Vector2 unit) {
            return Vector3Add(center, Vector3Add(
                Vector3Scale(right, unit.x * radius),
                Vector3Scale(up, unit.y * radius)));
        };
        emitProjectileVertex(center, centerColor);
        emitProjectileVertex(point(Circle[index - 1]), edgeColor);
        emitProjectileVertex(point(Circle[index]), edgeColor);
    }

    Vector3 forward = Vector3Add(
        Vector3Scale(right, Vector3DotProduct(direction, right)),
        Vector3Scale(up, Vector3DotProduct(direction, up)));
    if (Vector3LengthSqr(forward) <= 0.000001F) {
        forward = up;
    } else {
        forward = Vector3Normalize(forward);
    }
    const Vector3 lateral = Vector3Normalize(
        Vector3CrossProduct(forward, view));
    const auto ray = [center, centerColor](
                         Vector3 axis, Vector3 sideways,
                         float length, float halfWidth) {
        const Vector3 width = Vector3Scale(sideways, halfWidth);
        const Vector3 tip = Vector3Add(center,
            Vector3Scale(axis, length));
        emitProjectileVertex(Vector3Add(center, width), centerColor);
        emitProjectileVertex(tip, {255, 139, 38, 0});
        emitProjectileVertex(Vector3Subtract(center, width), centerColor);
    };
    ray(forward, lateral, 0.43F * fade, 0.075F * fade);
    ray(lateral, forward, 0.21F * fade, 0.052F * fade);
    ray(Vector3Negate(lateral), forward,
        0.21F * fade, 0.052F * fade);
}

inline Vector3 ballisticTrailPoint(
    const CannonProjectile& projectile, std::size_t age, float interpolationAlpha) {
    if (projectile.trailPointCount == 0U) {
        return {static_cast<float>(projectile.position.x),
                static_cast<float>(projectile.position.y),
                static_cast<float>(projectile.position.z)};
    }
    const auto point = [&projectile](std::size_t pointAge) {
        const auto& position = projectile.trailPositions[
            (projectile.trailHead + CannonProjectile::TrailCapacity - pointAge) %
            CannonProjectile::TrailCapacity];
        return Vector3{static_cast<float>(position.x),
                       static_cast<float>(position.y),
                       static_cast<float>(position.z)};
    };
    age = std::min(age, projectile.trailPointCount - 1U);
    return Vector3Lerp(point(std::min(age + 1U, projectile.trailPointCount - 1U)),
                       point(age), std::clamp(interpolationAlpha, 0.0F, 1.0F));
}

inline void emitCannonballGeometry(
    Vector3 center, Vector3 direction, Vector3 cameraPosition,
    float radius, Color tint) {
    // A round camera-facing body uses the turret's explicit color shading,
    // so the bright core stays readable at night as well as in daylight.
    Vector3 view = Vector3Subtract(center, cameraPosition);
    view = Vector3LengthSqr(view) > 0.000001F
        ? Vector3Normalize(view) : Vector3{0.0F, 0.0F, -1.0F};
    Vector3 right = Vector3CrossProduct(view, {0.0F, 1.0F, 0.0F});
    right = Vector3LengthSqr(right) > 0.000001F
        ? Vector3Normalize(right) : Vector3{1.0F, 0.0F, 0.0F};
    const Vector3 up = Vector3Normalize(Vector3CrossProduct(right, view));
    Vector2 forward{Vector3DotProduct(direction, right),
                    Vector3DotProduct(direction, up)};
    const float forwardLength = Vector2Length(forward);
    forward = forwardLength > 0.0001F
        ? Vector2Scale(forward, 1.0F / forwardLength) : Vector2{0.0F, 1.0F};
    const Vector3 highlight = Vector3Add(center, Vector3Add(
        Vector3Scale(right, forward.x * radius * 0.28F),
        Vector3Scale(up, (forward.y * 0.28F + 0.12F) * radius)));
    const Color core = ColorLerp(tint, WHITE, 0.42F);
    constexpr std::array<Vector2, 17> Circle{{
        {1, 0}, {.9238795F, .3826834F}, {.7071068F, .7071068F},
        {.3826834F, .9238795F}, {0, 1}, {-.3826834F, .9238795F},
        {-.7071068F, .7071068F}, {-.9238795F, .3826834F}, {-1, 0},
        {-.9238795F, -.3826834F}, {-.7071068F, -.7071068F},
        {-.3826834F, -.9238795F}, {0, -1}, {.3826834F, -.9238795F},
        {.7071068F, -.7071068F}, {.9238795F, -.3826834F}, {1, 0},
    }};
    const auto rimPoint = [center, right, up, radius](Vector2 unit) {
        return Vector3Add(center, Vector3Add(
            Vector3Scale(right, unit.x * radius),
            Vector3Scale(up, unit.y * radius)));
    };
    const auto rimColor = [forward, tint](Vector2 unit) {
        const float shade = -0.15F + 0.37F *
            (0.5F + 0.5F * Vector2DotProduct(unit, forward));
        return ColorLerp(tint, shade >= 0.0F ? WHITE : BLACK, std::abs(shade));
    };
    for (std::size_t index = 1; index < Circle.size(); ++index) {
        emitProjectileVertex(highlight, core);
        emitProjectileVertex(rimPoint(Circle[index - 1]), rimColor(Circle[index - 1]));
        emitProjectileVertex(rimPoint(Circle[index]), rimColor(Circle[index]));
    }
}

inline void emitDefenseShotGeometry(
    Vector3 head, Vector3 direction, Vector3 cameraPosition,
    float travelledDistance, float scale, float opacity,
    Color tint, bool drawTail, bool crossbowBolt = false) {
    Vector3 side = Vector3CrossProduct(
        direction, Vector3Subtract(cameraPosition, head));
    if (Vector3LengthSqr(side) <= 0.000001F) {
        side = Vector3CrossProduct(direction, {0.0F, 1.0F, 0.0F});
    }
    if (Vector3LengthSqr(side) <= 0.000001F) {
        side = {1.0F, 0.0F, 0.0F};
    } else {
        side = Vector3Normalize(side);
    }

    const float CapsuleLength = crossbowBolt ? 1.18F : 0.82F;
    const float CapsuleRadius = crossbowBolt ? 0.09F : 0.13F;
    const float TrailLength = crossbowBolt ? 1.65F : 1.30F;
    const float length = CapsuleLength * scale;
    const float radius = CapsuleRadius * scale;
    const Vector3 frontCenter = Vector3Subtract(
        head, Vector3Scale(direction, crossbowBolt ? 0.30F * scale : radius));
    const Vector3 rearCenter = Vector3Subtract(
        head, Vector3Scale(direction, length - radius));
    const Vector3 left = Vector3Scale(side, radius);
    const unsigned char bodyAlpha =
        static_cast<unsigned char>(255.0F * opacity);
    const auto shaded = [tint, bodyAlpha](Color shade, float amount) {
        Color color = ColorLerp(tint, shade, amount);
        color.a = bodyAlpha;
        return color;
    };
    const Color rearRim = shaded(BLACK, 0.15F);
    const Color rearCore = shaded(WHITE, 0.0F);
    const Color frontRim = shaded(WHITE, 0.22F);
    const Color frontCore = shaded(WHITE, 0.42F);

    if (drawTail) {
        // The tail sits underneath the rounded rear end, narrows gradually,
        // and fades from warm amber to smoke without spawning particles.
        const float visibleTrail = std::min(
            TrailLength * scale,
            std::max(0.0F, travelledDistance - (length - radius)));
        if (visibleTrail > 0.0001F) {
            const Vector3 far = Vector3Subtract(
                rearCenter, Vector3Scale(direction, visibleTrail));
            const Vector3 middle = Vector3Subtract(
                rearCenter, Vector3Scale(direction, visibleTrail * 0.45F));
            const Vector3 farSide = Vector3Scale(left, 0.72F);
            const Vector3 middleSide = Vector3Scale(left, 0.84F);
            const Vector3 nearSide = Vector3Scale(left, 0.92F);
            const Color farColor{112, 113, 112, 0};
            Color middleColor = ColorLerp(tint, {112, 113, 112, 255}, 0.65F);
            middleColor.a = static_cast<unsigned char>(95.0F * opacity);
            const Color nearColor{tint.r, tint.g, tint.b,
                static_cast<unsigned char>(160.0F * opacity)};
            emitProjectileQuad(
                Vector3Add(far, farSide),
                Vector3Add(middle, middleSide),
                Vector3Subtract(middle, middleSide),
                Vector3Subtract(far, farSide),
                farColor, middleColor, middleColor, farColor);
            emitProjectileQuad(
                Vector3Add(middle, middleSide),
                Vector3Add(rearCenter, nearSide),
                Vector3Subtract(rearCenter, nearSide),
                Vector3Subtract(middle, middleSide),
                middleColor, nearColor, nearColor, middleColor);
        }
        return;
    }

    // Split the body at its centerline: the core is lighter than the rim,
    // while the front warms from gold into pale yellow.
    emitProjectileQuad(
        Vector3Add(rearCenter, left), Vector3Add(frontCenter, left),
        frontCenter, rearCenter,
        rearRim, frontRim, frontCore, rearCore);
    emitProjectileQuad(
        rearCenter, frontCenter,
        Vector3Subtract(frontCenter, left),
        Vector3Subtract(rearCenter, left),
        rearCore, frontCore, frontRim, rearRim);

    if (crossbowBolt) {
        // A pointed, shaded tip makes this read as a bolt rather than a bullet.
        emitProjectileVertex(Vector3Add(frontCenter, left), frontRim);
        emitProjectileVertex(head, frontCore);
        emitProjectileVertex(frontCenter, frontCore);
        emitProjectileVertex(frontCenter, frontCore);
        emitProjectileVertex(head, frontCore);
        emitProjectileVertex(Vector3Subtract(frontCenter, left), frontRim);
    }

    // Six segments per end keep the capsule smooth while all turret shots
    // still fit in one small rlgl batch.
    constexpr std::array<Vector2, 7> Arc{{
        {0.0F, -1.0F}, {0.5F, -0.8660254F},
        {0.8660254F, -0.5F}, {1.0F, 0.0F},
        {0.8660254F, 0.5F}, {0.5F, 0.8660254F},
        {0.0F, 1.0F},
    }};
    const auto arcPoint = [direction, side, radius](
                              Vector3 center, float along, float across) {
        return Vector3Add(center, Vector3Add(
            Vector3Scale(direction, along * radius),
            Vector3Scale(side, across * radius)));
    };
    for (std::size_t index = 1; index < Arc.size(); ++index) {
        if (!crossbowBolt) {
            emitProjectileVertex(frontCenter, frontCore);
            emitProjectileVertex(arcPoint(
                frontCenter, Arc[index - 1].x, Arc[index - 1].y), frontRim);
            emitProjectileVertex(arcPoint(
                frontCenter, Arc[index].x, Arc[index].y), frontRim);
        }

        emitProjectileVertex(rearCenter, rearCore);
        emitProjectileVertex(arcPoint(
            rearCenter, -Arc[index - 1].x, Arc[index - 1].y), rearRim);
        emitProjectileVertex(arcPoint(
            rearCenter, -Arc[index].x, Arc[index].y), rearRim);
    }
}

// Keep the authored shot style independent of the physical flight runtime.
// Size is the only stat that changes body scale; fragments keep their authored ratio.
inline void emitDefenseProjectileGeometry(
    const AttackProjectile& projectile, Vector3 cameraPosition,
    float interpolationAlpha, bool drawTail) {
    const auto appearance = presentation::bulletAppearance(projectile.configuration.stats.damage);
    const float scale = appearance.scale *
        static_cast<float>(projectile.configuration.stats.size) *
        (projectile.shrapnel ? 0.56F : 1.0F);
    const auto position = geometry::lerpClamped(
        projectile.previousPosition, projectile.position, interpolationAlpha);
    Vector3 direction{static_cast<float>(projectile.runtime.velocity.x),
        static_cast<float>(projectile.runtime.velocity.y),
        static_cast<float>(projectile.runtime.velocity.z)};
    direction = Vector3LengthSqr(direction) > 0.000001F
        ? Vector3Normalize(direction) : Vector3{0, 0, -1};
    if (projectile.type == BuildingType::MissileLauncher) {
        const float rocketScale = static_cast<float>(projectile.configuration.stats.size);
        const Vector3 center{static_cast<float>(position.x), static_cast<float>(position.y), static_cast<float>(position.z)};
        Vector3 right = Vector3CrossProduct(direction, {0, 1, 0});
        right = Vector3LengthSqr(right) > 1e-6F ? Vector3Normalize(right) : Vector3{1, 0, 0};
        const Vector3 up = Vector3Normalize(Vector3CrossProduct(right, direction));
        const auto point = [&](float axial, float x, float y) {
            return Vector3Add(center, Vector3Scale(Vector3Add(Vector3Scale(direction, axial),
                Vector3Add(Vector3Scale(right, x), Vector3Scale(up, y))), rocketScale));
        };
        constexpr std::array<Vector2, 9> ring{{{1,0},{.7071F,.7071F},{0,1},{-.7071F,.7071F},
            {-1,0},{-.7071F,-.7071F},{0,-1},{.7071F,-.7071F},{1,0}}};
        if (!drawTail) {
            for (std::size_t i = 1; i < ring.size(); ++i) {
                const auto a = ring[i-1], b = ring[i];
                const Color body = i < 5 ? Color{218,235,247,255} : Color{107,142,170,255};
                emitProjectileQuad(point(-.22F,a.x*.09F,a.y*.09F), point(.13F,a.x*.09F,a.y*.09F),
                    point(.13F,b.x*.09F,b.y*.09F), point(-.22F,b.x*.09F,b.y*.09F), body,body,body,body);
                emitProjectileVertex(point(.13F,a.x*.09F,a.y*.09F), {255,143,34,255});
                emitProjectileVertex(point(.30F,0,0), {255,210,95,255});
                emitProjectileVertex(point(.13F,b.x*.09F,b.y*.09F), {226,78,26,255});
                emitProjectileVertex(point(-.22F,a.x*.09F,a.y*.09F),body);
                emitProjectileVertex(point(-.22F,b.x*.09F,b.y*.09F),body);
                emitProjectileVertex(point(-.22F,0,0),body);
            }
            for (int i=0; i<4; ++i) {
                const auto axis = ring[static_cast<std::size_t>(i*2)];
                emitProjectileVertex(point(-.20F,axis.x*.08F,axis.y*.08F),{255,170,48,255});
                emitProjectileVertex(point(-.27F,axis.x*.20F,axis.y*.20F),{244,114,27,255});
                emitProjectileVertex(point(-.03F,axis.x*.08F,axis.y*.08F),{255,206,81,255});
            }
        } else {
            const float pulse = .5F + .5F * std::sin(static_cast<float>(projectile.runtime.elapsedLifetime)*53.0F);
            for (std::size_t i=1;i<ring.size();++i) {
                const auto a=ring[i-1], b=ring[i];
                emitProjectileVertex(point(-.22F,a.x*.075F,a.y*.075F),{255,243,168,235});
                emitProjectileVertex(point(-.60F-.12F*pulse,0,0),{255,104,23,0});
                emitProjectileVertex(point(-.22F,b.x*.075F,b.y*.075F),{255,177,41,210});
            }
            // Fixed flight history follows curved trajectories; no particle allocations.
            const auto& trail = projectile.runtime;
            for (std::size_t puff=1;puff<trail.trailCount;++puff) {
                const float age = static_cast<float>(puff)/8.0F;
                const Vec3 stored = trail.trailPositions[(trail.trailHead + trail.trailPositions.size()-puff)%trail.trailPositions.size()];
                const Vector3 smoke{static_cast<float>(stored.x), static_cast<float>(stored.y)+age*.1F, static_cast<float>(stored.z)};
                emitCannonballGeometry(smoke,direction,cameraPosition,(.07F+.10F*age)*rocketScale,
                    {205,218,225,static_cast<unsigned char>(100*(1-age))});
            }
        }
        return;
    }
    // The simulation point is the capsule center, not its leading edge.
    // Retain the old launch lead so the body emerges from the muzzle centered.
    const float launchLead = 0.41F * scale;
    const Vector3 head = Vector3Add(
        {static_cast<float>(position.x), static_cast<float>(position.y),
         static_cast<float>(position.z)}, Vector3Scale(direction, launchLead));
    Color color{appearance.color.r, appearance.color.g, appearance.color.b, 255};
    const bool bolt = projectile.type == BuildingType::Turret;
    if (bolt) color = ColorLerp(color, {132, 210, 255, 255}, 0.65F);
    const float travelled = static_cast<float>(std::lerp(
        projectile.runtime.previousTravelledDistance, projectile.runtime.travelledDistance,
        std::clamp(static_cast<double>(interpolationAlpha), 0.0, 1.0)));
    emitDefenseShotGeometry(head, direction, cameraPosition,
        travelled + launchLead, scale, 1.0F, color, drawTail, bolt);
}

} // namespace ian::app_projectile_drawing
