#pragma once

#include "graphics/WorldTransforms.hpp"

#include <optional>

namespace ian {

// A conservative screen rectangle for a rigid viewmodel. Crossing the near
// plane falls back to the full composite rather than risking clipped effects.
[[nodiscard]] inline std::optional<Rectangle> viewModelCompositeBounds(
    BoundingBox bounds, Matrix clipTransform, int width, int height,
    float padding) {
    if (!world_transforms::finite(bounds) ||
        !world_transforms::finite(clipTransform) || width <= 0 || height <= 0 ||
        !std::isfinite(padding)) return std::nullopt;
    float left = static_cast<float>(width), top = static_cast<float>(height);
    float right = 0.0F, bottom = 0.0F;
    for (int corner = 0; corner < 8; ++corner) {
        const float x = (corner & 1) ? bounds.max.x : bounds.min.x;
        const float y = (corner & 2) ? bounds.max.y : bounds.min.y;
        const float z = (corner & 4) ? bounds.max.z : bounds.min.z;
        const float clipX = clipTransform.m0*x + clipTransform.m4*y + clipTransform.m8*z + clipTransform.m12;
        const float clipY = clipTransform.m1*x + clipTransform.m5*y + clipTransform.m9*z + clipTransform.m13;
        const float clipZ = clipTransform.m2*x + clipTransform.m6*y + clipTransform.m10*z + clipTransform.m14;
        const float clipW = clipTransform.m3*x + clipTransform.m7*y + clipTransform.m11*z + clipTransform.m15;
        if (clipW <= 0.0001F || clipZ <= -clipW) return std::nullopt;
        const float screenX = (clipX / clipW + 1.0F) * 0.5F * static_cast<float>(width);
        const float screenY = (1.0F - clipY / clipW) * 0.5F * static_cast<float>(height);
        if (!std::isfinite(screenX) || !std::isfinite(screenY)) return std::nullopt;
        left = std::min(left, screenX); right = std::max(right, screenX);
        top = std::min(top, screenY); bottom = std::max(bottom, screenY);
    }
    padding = std::max(padding, 0.0F);
    left = std::clamp(std::floor(left - padding), 0.0F, static_cast<float>(width));
    top = std::clamp(std::floor(top - padding), 0.0F, static_cast<float>(height));
    right = std::clamp(std::ceil(right + padding), left, static_cast<float>(width));
    bottom = std::clamp(std::ceil(bottom + padding), top, static_cast<float>(height));
    return Rectangle{left, top, right - left, bottom - top};
}

} // namespace ian
