#pragma once

#include "graphics/GraphicsResources.hpp"
#include "resources/ResourceSystem.hpp"

#include <raylib.h>

#include <cstddef>
#include <optional>

namespace ian::renderer_model_detail {

inline constexpr float TreeModelScale = 1.0F;
inline constexpr float RockModelScale = 2.0F;
inline constexpr float RockGroundOffset = 0.204F;

[[nodiscard]] constexpr std::size_t propModelIndex(ResourceType type) {
    if (type == ResourceType::Barrel) return 0U;
    if (type == ResourceType::Crate) return 1U;
    return 2U;
}

[[nodiscard]] std::optional<double> modelColliderRaycastDistance(
    const ModelResource& resource, Matrix transform,
    Ray ray, double maxDistance);

} // namespace ian::renderer_model_detail
