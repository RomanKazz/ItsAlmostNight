#pragma once

#include "enemies/EnemySystem.hpp"

#include <span>
#include <vector>

namespace ian {

struct EnemyCapsule {
    double radius;
    double segmentHalfHeight;
};

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
