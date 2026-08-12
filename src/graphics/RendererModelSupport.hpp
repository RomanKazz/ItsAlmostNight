#pragma once

#include "graphics/GraphicsResources.hpp"
#include "resources/ResourceSystem.hpp"

#include <raylib.h>

#include <cstddef>
#include <optional>

namespace ian::renderer_model_detail {

inline constexpr float TreeModelScale = 1.0F;
inline constexpr float RockModelScale =
    static_cast<float>(StoneVisualModelScale);

[[nodiscard]] constexpr float rockGroundOffset(
    std::size_t variant) {
    return static_cast<float>(StoneVisualGroundOffsets[
        variant % StoneVisualVariantCount]);
}

[[nodiscard]] constexpr std::size_t propModelIndex(ResourceType type) {
    if (type == ResourceType::Barrel) return 0U;
    if (type == ResourceType::Crate) return 1U;
    return 2U;
}

[[nodiscard]] std::optional<double> modelColliderRaycastDistance(
    const ModelResource& resource, Matrix transform,
    Ray ray, double maxDistance);

} // namespace ian::renderer_model_detail
