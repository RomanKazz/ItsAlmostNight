#pragma once

#include "enemies/EnemySystem.hpp"

#include <span>
#include <vector>

namespace ian {

struct EnemyCapsule {
    double radius;
    double segmentHalfHeight;
};

// Upright finite cylinder in world coordinates; dimensions are independent of
// the forgiving ray-hit capsule. Surface height includes terrain and platforms.
struct EnemyCylinder {
    Vec3 center;
    double radius;
    double halfHeight;
};

[[nodiscard]] EnemyCylinder enemyCollisionCylinder(const EnemyInstance& enemy);
[[nodiscard]] double enemyCylinderPenetration(
    const EnemyCylinder& left, const EnemyCylinder& right);

[[nodiscard]] EnemyCapsule enemyCapsule(EnemyType type);
[[nodiscard]] EnemyCapsule enemyPhysicalCapsule(EnemyType type);
[[nodiscard]] double maximumGroundStructureInteractionHeight(
    EnemyType type, double enemyCenterHeight);

void resolveEnemyCapsuleCollisions(
    std::span<EnemyInstance> enemies,
    std::span<const EnemyStructureTarget> structures);
void resolveEnemyCapsuleCollisions(
    std::span<EnemyInstance> enemies,
    std::span<const EnemyStructureTarget> structures,
    std::vector<int>& enemyLinks,
    std::vector<int>& structureLinks,
    std::span<const int> cachedStructureHeads = {});

} // namespace ian
