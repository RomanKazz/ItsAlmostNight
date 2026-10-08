#pragma once

#include <raymath.h>

#include <algorithm>

namespace ian::quarry_model_detail {

// Original stone columns stay hidden at 0; complete baked levels = 1..8.
inline constexpr int MeshCount = 9;

template <typename Emit>
void forEachLevelPart(int requestedLevel, Emit&& emit) {
    const int level = std::clamp(requestedLevel, 1, 8);
    emit(level, MatrixIdentity());
}

} // namespace ian::quarry_model_detail
