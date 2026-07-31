#pragma once

#include "enemies/EnemySystem.hpp"

#include <span>

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

} // namespace ian
