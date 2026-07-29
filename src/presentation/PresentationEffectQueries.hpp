#pragma once

#include "presentation/PresentationTypes.hpp"

#include <span>

namespace ian::presentation {

[[nodiscard]] float resourceHitFlash(
    std::span<const PresentationEffect> effects,
    EntityId id);
[[nodiscard]] float resourceHitScale(
    std::span<const PresentationEffect> effects,
    EntityId id);
[[nodiscard]] Vec3 resourceHitOffset(
    std::span<const PresentationEffect> effects,
    EntityId id, Vec3 position);

} // namespace ian::presentation
