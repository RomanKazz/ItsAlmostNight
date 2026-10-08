#pragma once

#include <algorithm>

namespace ian::wall_upgrade_model_detail {

// Original mesh = 0; tools/generate_wall_upgrades.py appends levels 2..8.
inline constexpr int MeshCount = 8;

[[nodiscard]] inline int upgradeMeshIndex(int requestedLevel) {
    return std::clamp(requestedLevel, 1, 8) - 1;
}

} // namespace ian::wall_upgrade_model_detail
