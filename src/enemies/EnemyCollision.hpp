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
[[nodiscard]] double maximumGroundStructureInteractionHeight(
    EnemyType type, double enemyCenterHeight);

void resolveEnemyCapsuleCollisions(
    std::span<EnemyInstance> enemies,
    std::span<const BuildingInstance> buildings);
void resolveEnemyCapsuleCollisions(
    std::span<EnemyInstance> enemies,
    std::span<const BuildingInstance> buildings,
    std::vector<int>& enemyLinks,
    std::vector<int>& buildingLinks);

} // namespace ian
