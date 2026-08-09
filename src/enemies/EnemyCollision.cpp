#include "enemies/EnemyCollision.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace ian {
namespace {

constexpr double ContactPadding = 0.025;
constexpr int DetailedSolverIterations = 6;
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

[[nodiscard]] EnemyCapsule physicalEnemyCapsule(EnemyType type) {
    if (type == EnemyType::Basic) {
        // Keep crowd spacing compact even though the Pink Blob deliberately
        // has a very forgiving combat hit capsule.
        return {.radius = 0.60, .segmentHalfHeight = 0.44};
    }
    return enemyCapsule(type);
}

[[nodiscard]] double capsuleMass(EnemyType type) {
    const double radius = physicalEnemyCapsule(type).radius;
    if (type == EnemyType::Boss) {
        return radius * radius * 3.0;
    }
    if (type == EnemyType::Heavy) {
        return radius * radius * 1.8;
    }
    return radius * radius;
}

[[nodiscard]] bool capsulesShareHeight(
    const EnemyInstance& left, const EnemyInstance& right) {
    const bool leftFlying = left.type == EnemyType::Flying;
    const bool rightFlying = right.type == EnemyType::Flying;
    if (leftFlying != rightFlying) {
        return false;
    }
    const EnemyCapsule leftCapsule =
        physicalEnemyCapsule(left.type);
    const EnemyCapsule rightCapsule =
        physicalEnemyCapsule(right.type);
    const double verticalDistance =
        std::abs(left.position.y - right.position.y);
    return verticalDistance <=
           leftCapsule.segmentHalfHeight +
               rightCapsule.segmentHalfHeight +
               leftCapsule.radius + rightCapsule.radius;
}

[[nodiscard]] bool resolvePair(
    EnemyInstance& left, EnemyInstance& right) {
    if (!left.active || !right.active ||
        !capsulesShareHeight(left, right)) {
        return false;
    }

    const double minimumDistance =
        physicalEnemyCapsule(left.type).radius +
        physicalEnemyCapsule(right.type).radius +
        ContactPadding;
    double offsetX = right.position.x - left.position.x;
    double offsetZ = right.position.z - left.position.z;
    double distance = std::hypot(offsetX, offsetZ);
    if (distance >= minimumDistance) {
        return false;
    }

    if (distance <= MinimumDistance) {
        const bool leftFirst =
            left.id.index < right.id.index ||
            (left.id.index == right.id.index &&
             left.id.generation < right.id.generation);
        offsetX = leftFirst ? 1.0 : -1.0;
        offsetZ = 0.0;
        distance = 1.0;
    }

    const double normalX = offsetX / distance;
    const double normalZ = offsetZ / distance;
    const double penetration = minimumDistance - distance;
    const double leftInverseMass = 1.0 / capsuleMass(left.type);
    const double rightInverseMass = 1.0 / capsuleMass(right.type);
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
    if (!enemy.active ||
        (enemy.type == EnemyType::Flying &&
         structure.buildingType != BuildingType::Core)) {
        return false;
    }

    if (enemy.type != EnemyType::Flying) {
        const double maximumCollisionHeight =
            maximumGroundStructureInteractionHeight(
                enemy.type, enemy.position.y);
        if (structure.position.y >
            maximumCollisionHeight) {
            return false;
        }
    }

    const Vec3 center = structure.position;
    const double minimumDistance =
        physicalEnemyCapsule(enemy.type).radius +
        structure.radius;
    double offsetX = enemy.position.x - center.x;
    double offsetZ = enemy.position.z - center.z;
    double distance = std::hypot(offsetX, offsetZ);
    if (distance >= minimumDistance) {
        return false;
    }
    if (distance <= MinimumDistance) {
        const double angle =
            static_cast<double>(enemy.id.index % 16U) *
            0.39269908169872414;
        offsetX = std::cos(angle);
        offsetZ = std::sin(angle);
        distance = 1.0;
    }

    const double correction = minimumDistance - distance;
    enemy.position.x += offsetX / distance * correction;
    enemy.position.z += offsetZ / distance * correction;
    return true;
}

} // namespace

EnemyCapsule enemyCapsule(EnemyType type) {
    switch (type) {
    case EnemyType::Fast:
        return {.radius = 0.34, .segmentHalfHeight = 0.26};
    case EnemyType::Heavy:
        return {.radius = 0.56, .segmentHalfHeight = 0.42};
    case EnemyType::Boss:
        return {.radius = 0.92, .segmentHalfHeight = 0.62};
    case EnemyType::Ranged:
        return {.radius = 0.42, .segmentHalfHeight = 0.32};
    case EnemyType::Sapper:
        return {.radius = 0.44, .segmentHalfHeight = 0.30};
    case EnemyType::Flying:
        return {.radius = 0.42, .segmentHalfHeight = 0.24};
    case EnemyType::Basic:
        // Pink Blob has a broad, rounded silhouette, so its gameplay capsule
        // needs to cover more of the visible body than the old minion did.
        return {.radius = 0.75, .segmentHalfHeight = 0.44};
    }
    return {.radius = 0.43, .segmentHalfHeight = 0.32};
}

double maximumGroundStructureInteractionHeight(
    EnemyType type, double enemyCenterHeight) {
    const EnemyCapsule capsule = physicalEnemyCapsule(type);
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
    std::array<int, CollisionCellCount> bucketHeads{};
    std::array<int, CollisionCellCount> localStructureHeads{};
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
    const int structureSearchCells = std::max(
        2, static_cast<int>(std::ceil(
               (maximumStructureRadius + 1.0) /
               CollisionCellSize)));
    const std::size_t activeEnemyCount =
        static_cast<std::size_t>(std::count_if(
            enemies.begin(), enemies.end(),
            [](const EnemyInstance& enemy) {
                return enemy.active;
            }));
    const int solverIterations =
        activeEnemyCount > 384U
            ? 3
            : (activeEnemyCount > 192U
                   ? 4
                   : DetailedSolverIterations);
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
            for (int z = std::max(0, centerZ - 1);
                 z <= std::min(
                          CollisionGridSize - 1,
                          centerZ + 1);
                 ++z) {
                for (int x = std::max(0, centerX - 1);
                     x <= std::min(
                              CollisionGridSize - 1,
                              centerX + 1);
                     ++x) {
                    int rightIndex = bucketHeads[
                        static_cast<std::size_t>(
                            collisionCellIndex(x, z))];
                    while (rightIndex >= 0) {
                        if (static_cast<std::size_t>(
                                rightIndex) > leftIndex) {
                            corrected =
                                resolvePair(
                                    enemies[leftIndex],
                                    enemies[static_cast<
                                        std::size_t>(
                                        rightIndex)]) ||
                                corrected;
                        }
                        rightIndex = enemyLinks[
                            static_cast<std::size_t>(
                                rightIndex)];
                    }
                }
            }
        }
        for (EnemyInstance& enemy : enemies) {
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
