#pragma once

#include "core/Types.hpp"

#include <cstddef>
#include <vector>

namespace ian {

class TerrainHeightfield;

struct PondLilyPlacement {
    Vec3 position{};
    std::size_t variant{};
    double scale{1.0};
    double yaw{};
    double collisionRadius{};
    double surfaceHeight{};
};

[[nodiscard]] std::vector<PondLilyPlacement>
generatePondLilyPlacements(const TerrainHeightfield& terrain);

} // namespace ian
