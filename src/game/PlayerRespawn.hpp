#pragma once

#include "enemies/EnemyCollision.hpp"
#include "world/CollisionWorld.hpp"
#include "world/TerrainHeightfield.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <span>

namespace ian {
inline constexpr double PlayerRespawnProtectionSeconds = 2.5;

// Search happens once per death. Reuse the movement broadphase and actual enemy
// cylinders; no temporary runtime container or collision proxy is allocated.
inline Vec3 findPlayerRespawn(Vec3 fallback, std::optional<Vec3> core,
                              const TerrainHeightfield& terrain,
                              const CollisionWorld& collisions,
                              std::span<const EnemyInstance> enemies,
                              double eyeHeight) {
    std::optional<Vec3> best;
    double bestClearance = -std::numeric_limits<double>::infinity();
    const auto candidate = [&](Vec3 point) -> bool {
        if (!terrain.isInside(point.x, point.z) || terrain.waterDepth(point.x, point.z) > .12) return false;
        const double ground = terrain.getHeight(point.x, point.z);
        const double feet = std::max(ground, collisions.playerSupportHeight(
            point.x, point.z, CollisionWorld::PlayerRadius, ground + .65).value_or(ground));
        point.y = feet + eyeHeight;
        if (!collisions.canOccupyCircle(point, CollisionWorld::PlayerRadius + .08)) return false;
        if (collisions.modularCeilingHeight(point.x, point.z, feet + .1,
                                           point.y + .20)) return false;
        double clearance = 20;
        for (const auto& enemy : enemies) {
            if (!enemy.active || enemy.health <= 0) continue;
            const auto body = enemyCollisionCylinder(enemy);
            if (body.center.y - body.halfHeight > point.y + .2 ||
                body.center.y + body.halfHeight < feet) continue;
            clearance = std::min(clearance, std::hypot(point.x - body.center.x,
                point.z - body.center.z) - body.radius - CollisionWorld::PlayerRadius);
        }
        if (clearance < .1) return false;
        if (!best || clearance > bestClearance) { best = point; bestClearance = clearance; }
        return clearance >= 2.5;
    };
    const auto around = [&](Vec3 origin, bool atOrigin) -> bool {
        if (atOrigin && candidate(origin)) return true;
        for (double radius : {3.0, 4.5, 6.0, 8.0, 11.0, 15.0}) {
            for (int i = 0; i < 24; ++i) {
                const double angle = static_cast<double>(i) * 6.283185307179586 / 24;
                if (candidate({origin.x + std::sin(angle) * radius, 0,
                               origin.z + std::cos(angle) * radius})) return true;
            }
        }
        return false;
    };
    if (core) {
        if (around(*core, false) || best) return *best;
    }
    if (around(fallback, true) || best) return *best;
    // A completely sealed map has no valid solution; retain the map's authored
    // fallback rather than inventing a point beyond the world boundary.
    fallback.y = terrain.getHeight(fallback.x, fallback.z) + eyeHeight;
    return fallback;
}
} // namespace ian
