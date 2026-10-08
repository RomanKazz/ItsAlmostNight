#pragma once

#include <raymath.h>

#include <algorithm>

namespace ian::crystal_mine_model_detail {

// Original fixed body and shards are meshes 0/1. The authored
// upgrade mesh for each level is a single static overlay after them.
inline constexpr int MeshCount = 9;

template <typename Emit>
void forEachLevelPart(int requestedLevel, Emit&& emit) {
    emit(0, MatrixIdentity());
    emit(1, MatrixIdentity());
    const int level = std::clamp(requestedLevel, 1, 8);
    if (level > 1) emit(level, MatrixIdentity());
}
} // namespace ian::crystal_mine_model_detail
