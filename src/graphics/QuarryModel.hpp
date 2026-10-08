#pragma once

#include <raymath.h>

#include <algorithm>

namespace ian::quarry_model_detail {

// Authored textured stones = 0; replacement collector bodies = 1..8.
inline constexpr int MeshCount = 9;

template <typename Emit>
void forEachLevelPart(int requestedLevel, Emit&& emit) {
    const int level = std::clamp(requestedLevel, 1, 8);
    const float progress = static_cast<float>(level - 1);
    emit(level, MatrixIdentity());
    emit(0, MatrixMultiply(
        MatrixScale(.72F + progress * .025F, .72F + progress * .06F,
                    .64F + progress * .025F),
        MatrixTranslate(0, .16F + progress * .025F, 0)));
}

} // namespace ian::quarry_model_detail
