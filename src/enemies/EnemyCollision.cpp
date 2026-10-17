#include "enemies/EnemyCollision.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace ian {
namespace {

constexpr double ContactPadding = 0.025;
constexpr int DetailedSolverIterations = 12;
constexpr double ContactTolerance = 0.001;
constexpr double MinimumDistance = 1e-9;
constexpr double CollisionCellSize = 2.0;
constexpr double CollisionGridMinimum = -192.0;
constexpr int CollisionGridSize = 192;
constexpr int CollisionCellCount =
    CollisionGridSize * CollisionGridSize;

[[nodiscard]] int collisionCellCoordinate(double value) {
    if (!std::isfinite(value)) {
        return value < 0.0 ? 0 : CollisionGridSize - 1;
    }
    const double scaled =
        (value - CollisionGridMinimum) / CollisionCellSize;
    if (scaled <= 0.0) {
        return 0;
    }
    if (scaled >= static_cast<double>(CollisionGridSize - 1)) {
        return CollisionGridSize - 1;
    }
    return std::clamp(
        static_cast<int>(std::floor(scaled)),
        0, CollisionGridSize - 1);
}

[[nodiscard]] int collisionCellIndex(
    int x, int z) {
    return z * CollisionGridSize + x;
}

[[nodiscard]] double capsuleMass(EnemyType type) {
    const double radius = enemyPhysicalCapsule(type).radius;
    if (type == EnemyType::Boss) {
        return radius * radius * 3.0;
    }
    if (type == EnemyType::Heavy) {
        return radius * radius * 1.8;
    }
    return radius * radius;
}

struct CollisionBody {
    double radius{}, centerY{}, halfHeight{}, inverseMass{};
    int searchCells{};
    bool valid{};
};

[[nodiscard]] bool resolvePair(
    EnemyInstance& left, EnemyInstance& right,
    const CollisionBody& leftBody, const CollisionBody& rightBody) {
    if (!left.active || !right.active || !leftBody.valid || !rightBody.valid)
        return false;
    if (std::abs(leftBody.centerY - rightBody.centerY) >=
        leftBody.halfHeight + rightBody.halfHeight) return false;
    const double minimumDistance = leftBody.radius + rightBody.radius;
    const double offsetX = right.position.x - left.position.x;
    const double offsetZ = right.position.z - left.position.z;
    // Most grid neighbours do not touch. Preserve pair order and all twelve
    // solver iterations, but only compute the exact distance for nearby pairs.
    if (std::abs(offsetX) >= minimumDistance || std::abs(offsetZ) >= minimumDistance)
        return false;
    // Corner neighbours pass the axis test but still cannot touch. Keep
    // hypot for actual contacts so their corrections stay bit-for-bit equal.
    if (offsetX * offsetX + offsetZ * offsetZ >= minimumDistance * minimumDistance)
        return false;
    const double distance = std::hypot(offsetX, offsetZ);
    const double penetration = std::max(0.0, minimumDistance - distance);
    if (penetration <= ContactTolerance) return false;
    double normalX{}, normalZ{};
    if (distance <= MinimumDistance) {
        const bool leftFirst = left.id.index < right.id.index ||
            (left.id.index == right.id.index && left.id.generation < right.id.generation);
        const auto first = leftFirst ? left.id : right.id;
        const auto second = leftFirst ? right.id : left.id;
        const std::uint32_t hash = first.index * 0x9e3779b9U ^ second.index * 0x85ebca6bU ^
            first.generation * 17U ^ second.generation * 31U;
        // Stable pair-dependent direction, with no gameplay RNG consumption.
        // Keep X positive for the ordered pair; avoid collapsing a pile into a row.
        const double angle = (static_cast<double>(hash % 1024U) / 1023.0 - 0.5) * 2.4;
        normalX = std::cos(angle) * (leftFirst ? 1 : -1);
        normalZ = std::sin(angle) * (leftFirst ? 1 : -1);
    } else {
        normalX = offsetX / distance;
        normalZ = offsetZ / distance;
    }
    const double leftInverseMass = leftBody.inverseMass;
    const double rightInverseMass = rightBody.inverseMass;
    const double inverseMassSum =
        leftInverseMass + rightInverseMass;
    const double leftCorrection =
        penetration * leftInverseMass / inverseMassSum;
    const double rightCorrection =
        penetration * rightInverseMass / inverseMassSum;

    left.position.x -= normalX * leftCorrection;
    left.position.z -= normalZ * leftCorrection;
    right.position.x += normalX * rightCorrection;
    right.position.z += normalZ * rightCorrection;
    return true;
}

[[nodiscard]] bool resolveStructure(
    EnemyInstance& enemy,
    const EnemyStructureTarget& structure) {
    if (!enemy.active || (enemy.type==EnemyType::Boss && enemy.bossHoverOffset>-.4) || structure.traversable ||
        enemy.worldSurfaceHeight + 1e-6 <
            structure.minimumEnemySurfaceHeight ||
        enemy.worldSurfaceHeight - 1e-6 >
            structure.maximumEnemySurfaceHeight ||
        (enemy.type == EnemyType::Flying &&
         structure.buildingType != BuildingType::Core)) {
        return false;
    }

    if (enemy.type != EnemyType::Flying) {
        const double maximumCollisionHeight =
            maximumGroundStructureInteractionHeight(
                enemy.type, enemy.position.y + enemy.worldSurfaceHeight);
        if (structure.position.y >
            maximumCollisionHeight) {
            return false;
        }
    }

    const Vec3 center = structure.position;
    const double minimumDistance =
        enemyCollisionCylinder(enemy).radius + structure.radius;
    double offsetX = enemy.position.x - center.x;
    double offsetZ = enemy.position.z - center.z;
    if (std::abs(offsetX) >= minimumDistance || std::abs(offsetZ) >= minimumDistance)
        return false;
    const double distance = std::hypot(offsetX, offsetZ);
    if (distance >= minimumDistance) {
        return false;
    }
    if (distance <= MinimumDistance) {
        const double angle =
            static_cast<double>(enemy.id.index % 16U) *
            0.39269908169872414;
        offsetX = std::cos(angle);
        offsetZ = std::sin(angle);
    }

    const double correction = minimumDistance - distance;
    const double divisor = distance > MinimumDistance ? distance : 1.0;
    enemy.position.x += offsetX / divisor * correction;
    enemy.position.z += offsetZ / divisor * correction;
    return true;
}

} // namespace

EnemyCapsule enemyCapsule(EnemyType type) {
    switch (type) {
    case EnemyType::Fast:
        // Ninja has a broader animated silhouette than the previous Fast
        // model. Enlarge both capsule dimensions by exactly 20%.
        return {.radius = 0.408, .segmentHalfHeight = 0.312};
    case EnemyType::Heavy:
        // Mushnub Evolved is substantially broader and taller than the old
        // Heavy model: enlarge both capsule dimensions by exactly 50%.
        return {.radius = 0.84, .segmentHalfHeight = 0.63};
    case EnemyType::Boss:
        return {.radius = 1.35, .segmentHalfHeight = 0.65};
    case EnemyType::Ranged:
        return {.radius = 0.42, .segmentHalfHeight = 0.32};
    case EnemyType::Sapper:
        return {.radius = 0.44, .segmentHalfHeight = 0.30};
    case EnemyType::Flying:
        return {.radius = 0.42, .segmentHalfHeight = 0.24};
    case EnemyType::Splitter:
        return {.radius = 0.92, .segmentHalfHeight = 0.68};
    case EnemyType::Splitling:
        return {.radius = 0.46, .segmentHalfHeight = 0.31};
    case EnemyType::Basic:
        // Pink Blob has a broad, rounded silhouette, so its gameplay capsule
        // needs to cover more of the visible body than the old minion did.
        // Crowd spacing remains compact in enemyPhysicalCapsule().
        return {.radius = 0.8625, .segmentHalfHeight = 0.506};
    }
    return {.radius = 0.43, .segmentHalfHeight = 0.32};
}

namespace {
struct CylinderDimensions { double radius, height, bottom; };
// EnemyType order. Measured native silhouettes at the gameplay import scale;
// rounded outward to cover small walk squash and the authored protrusions.
constexpr std::array<CylinderDimensions, 9> PhysicalCylinders{{
    {0.86, 1.62, 0.0}, // Pink Blob
    {1.06, 1.60, 0.0}, // Ninja
    {1.34, 2.65, 0.0}, // Mushnub
    {3.18, 3.65, 0.50}, // flying Alpaking Evolved, including crown/wings
    {1.20, 2.24, 0.0}, // Wizard, including hat brim
    {1.14, 1.82, 0.0}, // spiky sapper
    {1.27, 0.92, 1.74}, // Dragon, wing envelope and rendered hover offset
    {1.27, 2.03, 0.0}, // spiky splitter
    {0.66, 1.04, 0.0}, // splitling
}};
CylinderDimensions cylinderDimensions(EnemyType type) {
    const auto index = static_cast<std::size_t>(type);
    return index < PhysicalCylinders.size() ? PhysicalCylinders[index] : PhysicalCylinders[0];
}
} // namespace

EnemyCylinder enemyCollisionCylinder(const EnemyInstance& enemy) {
    auto dimensions = cylinderDimensions(enemy.type);
    if(enemy.type==EnemyType::Boss) dimensions.bottom += enemy.bossHoverOffset;
    if(enemy.summonerId && enemy.type==EnemyType::Flying) dimensions = {1.20,1.06,1.68};
    const double scale = enemy.eliteAffixes ? 1.08 : 1.0;
    const double halfHeight = dimensions.height * scale * 0.5;
    return {{enemy.position.x, enemy.worldSurfaceHeight + dimensions.bottom + halfHeight,
             enemy.position.z}, dimensions.radius * scale, halfHeight};
}

double enemyCylinderPenetration(const EnemyCylinder& left, const EnemyCylinder& right) {
    if (!std::isfinite(left.radius) || !std::isfinite(right.radius) ||
        !std::isfinite(left.halfHeight) || !std::isfinite(right.halfHeight) ||
        left.radius <= 0 || right.radius <= 0 || left.halfHeight <= 0 || right.halfHeight <= 0 ||
        !std::isfinite(left.center.x) || !std::isfinite(left.center.y) || !std::isfinite(left.center.z) ||
        !std::isfinite(right.center.x) || !std::isfinite(right.center.y) || !std::isfinite(right.center.z)) return 0;
    if (std::abs(left.center.y - right.center.y) >= left.halfHeight + right.halfHeight) return 0;
    return std::max(0.0, left.radius + right.radius -
        std::hypot(left.center.x - right.center.x, left.center.z - right.center.z));
}

EnemyCapsule enemyPhysicalCapsule(EnemyType type) {
    const auto dimensions = cylinderDimensions(type);
    // Compatibility view for existing navigation/structure reach callers.
    return {dimensions.radius, std::max(0.0, dimensions.height * 0.5 - dimensions.radius)};
}

double maximumGroundStructureInteractionHeight(
    EnemyType type, double enemyCenterHeight) {
    const EnemyCapsule capsule = enemyPhysicalCapsule(type);
    constexpr double ReachAboveBody = 0.85;
    return enemyCenterHeight +
           capsule.segmentHalfHeight + capsule.radius +
           ReachAboveBody;
}

void resolveEnemyCapsuleCollisions(
    std::span<EnemyInstance> enemies,
    std::span<const EnemyStructureTarget> structures) {
    std::vector<int> enemyLinks;
    std::vector<int> structureLinks;
    resolveEnemyCapsuleCollisions(
        enemies, structures, enemyLinks, structureLinks, {});
}

void resolveEnemyCapsuleCollisions(
    std::span<EnemyInstance> enemies,
    std::span<const EnemyStructureTarget> structures,
    std::vector<int>& enemyLinks,
    std::vector<int>& structureLinks,
    std::span<const int> cachedStructureHeads) {
    // Heads are filled before any read; value-initializing both large grids
    // here would clear them a second time on every simulation tick.
    std::array<int, CollisionCellCount> bucketHeads;
    std::array<int, CollisionCellCount> localStructureHeads;
    enemyLinks.assign(enemies.size(), -1);
    double maximumStructureRadius = 0.0;
    for (const EnemyStructureTarget& structure : structures) {
        maximumStructureRadius = std::max(
            maximumStructureRadius, structure.radius);
    }
    const bool useCachedStructures =
        cachedStructureHeads.size() == CollisionCellCount &&
        structureLinks.size() == structures.size();
    if (!useCachedStructures) {
        structureLinks.assign(structures.size(), -1);
        localStructureHeads.fill(-1);
        for (std::size_t index = 0;
             index < structures.size(); ++index) {
            const Vec3 center = structures[index].position;
            const int bucket = collisionCellIndex(
                collisionCellCoordinate(center.x),
                collisionCellCoordinate(center.z));
            structureLinks[index] = localStructureHeads[
                static_cast<std::size_t>(bucket)];
            localStructureHeads[
                static_cast<std::size_t>(bucket)] =
                static_cast<int>(index);
        }
    }
    const std::span<const int> structureHeads =
        useCachedStructures
            ? cachedStructureHeads
            : std::span<const int>{localStructureHeads};
    // Dimensions/mass and vertical centers never change during this solve.
    // Keep capacity per thread instead of allocating per tick; there are no
    // callbacks/reentrant solves and separate simulations on threads are safe.
    thread_local std::vector<CollisionBody> bodies;
    bodies.resize(enemies.size());
    double maximumEnemyRadius = 0.0;
    for (std::size_t index = 0; index < enemies.size(); ++index) {
        const auto& enemy = enemies[index];
        if (!enemy.active) continue;
        const auto cylinder = enemyCollisionCylinder(enemy);
        maximumEnemyRadius = std::max(maximumEnemyRadius, cylinder.radius);
        bodies[index] = {
            cylinder.radius + ContactPadding * 0.5, cylinder.center.y, cylinder.halfHeight,
            1.0 / (capsuleMass(enemy.type) * (enemy.eliteAffixes ? 1.08 * 1.08 : 1.0)),
            0,
            std::isfinite(cylinder.radius) && std::isfinite(cylinder.halfHeight) &&
                cylinder.radius > 0 && cylinder.halfHeight > 0 &&
                std::isfinite(cylinder.center.x) && std::isfinite(cylinder.center.y) &&
                std::isfinite(cylinder.center.z)};
    }
    for (std::size_t index = 0; index < enemies.size(); ++index) {
        if (!enemies[index].active) continue;
        bodies[index].searchCells = std::max(1, static_cast<int>(std::ceil(
            (enemyCollisionCylinder(enemies[index]).radius + maximumEnemyRadius + ContactPadding) /
            CollisionCellSize)));
    }
    const int structureSearchCells = std::max(
        2, static_cast<int>(std::ceil(
               (maximumStructureRadius + maximumEnemyRadius) / CollisionCellSize)));
    // Keep the same quality in large waves. Most separated frames exit early.
    const int solverIterations = DetailedSolverIterations;
    for (int iteration = 0; iteration < solverIterations;
         ++iteration) {
        bool corrected = false;
        bucketHeads.fill(-1);
        std::fill(enemyLinks.begin(), enemyLinks.end(), -1);
        for (std::size_t index = 0;
             index < enemies.size(); ++index) {
            if (!enemies[index].active) {
                continue;
            }
            const int cellX = collisionCellCoordinate(
                enemies[index].position.x);
            const int cellZ = collisionCellCoordinate(
                enemies[index].position.z);
            const int bucket =
                collisionCellIndex(cellX, cellZ);
            enemyLinks[index] =
                bucketHeads[static_cast<std::size_t>(bucket)];
            bucketHeads[static_cast<std::size_t>(bucket)] =
                static_cast<int>(index);
        }
        for (std::size_t leftIndex = 0;
             leftIndex < enemies.size(); ++leftIndex) {
            if (!enemies[leftIndex].active) {
                continue;
            }
            const int centerX = collisionCellCoordinate(
                enemies[leftIndex].position.x);
            const int centerZ = collisionCellCoordinate(
                enemies[leftIndex].position.z);
            const int enemySearchCells = bodies[leftIndex].searchCells;
            for (int z = std::max(0, centerZ - enemySearchCells);
                 z <= std::min(
                          CollisionGridSize - 1,
                          centerZ + enemySearchCells);
                 ++z) {
                for (int x = std::max(0, centerX - enemySearchCells);
                     x <= std::min(
                              CollisionGridSize - 1,
                              centerX + enemySearchCells);
                     ++x) {
                    int rightIndex = bucketHeads[
                        static_cast<std::size_t>(
                            collisionCellIndex(x, z))];
                    while (rightIndex >= 0) {
                        if (static_cast<std::size_t>(
                                rightIndex) > leftIndex) {
                            corrected =
                                resolvePair(enemies[leftIndex],
                                    enemies[static_cast<std::size_t>(rightIndex)],
                                    bodies[leftIndex], bodies[static_cast<std::size_t>(rightIndex)]) ||
                                corrected;
                        }
                        rightIndex = enemyLinks[
                            static_cast<std::size_t>(
                                rightIndex)];
                    }
                }
            }
        }
        if (!structures.empty()) for (EnemyInstance& enemy : enemies) {
            if (!enemy.active) {
                continue;
            }
            const int centerX =
                collisionCellCoordinate(enemy.position.x);
            const int centerZ =
                collisionCellCoordinate(enemy.position.z);
            for (int z = std::max(
                     0, centerZ - structureSearchCells);
                 z <= std::min(
                     CollisionGridSize - 1,
                     centerZ + structureSearchCells);
                 ++z) {
                for (int x = std::max(
                         0, centerX - structureSearchCells);
                     x <= std::min(
                         CollisionGridSize - 1,
                         centerX + structureSearchCells);
                     ++x) {
                    int structureIndex =
                        structureHeads[
                            static_cast<std::size_t>(
                                collisionCellIndex(x, z))];
                    while (structureIndex >= 0) {
                        corrected =
                            resolveStructure(
                                enemy,
                                structures[
                                    static_cast<std::size_t>(
                                        structureIndex)]) ||
                            corrected;
                        structureIndex =
                            structureLinks[
                                static_cast<std::size_t>(
                                    structureIndex)];
                    }
                }
            }
        }
        if (!corrected) {
            break;
        }
    }
}

} // namespace ian
