#pragma once

#include "enemies/EnemySystem.hpp"

#include <memory>
#include <optional>

namespace ian {

[[nodiscard]] std::shared_ptr<MultiLevelNavigation>
makeMultiLevelNavigation(
    EnemyNavigationView view, Vec3 corePosition, int coreStorey);

[[nodiscard]] std::optional<Vec3> multiLevelWaypoint(
    const MultiLevelNavigation& navigation,
    const EnemyInstance& enemy, double terrainHeight,
    Vec3 corePosition);

} // namespace ian
