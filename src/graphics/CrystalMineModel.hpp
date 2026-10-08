#pragma once

#include <raymath.h>

#include <algorithm>

namespace ian::crystal_mine_model_detail {

// Original body/shards = 0/1; eight replacement housings = 2..9; original shard copy = 10.
// The old green body stays hidden. Poses are static at every level.
inline constexpr int MeshCount = 11;

template <typename Emit>
void forEachLevelPart(int requestedLevel, Emit&& emit) {
    const int level = std::clamp(requestedLevel, 1, 8);
    const float progress = static_cast<float>(level - 1);
    emit(level + 1, MatrixIdentity());
    const Matrix crystals = MatrixMultiply(
        MatrixScale(1.0F + progress * .025F, 1.0F + progress * .045F,
                    1.0F + progress * .025F),
        MatrixTranslate(0, .005F + progress * .018F, 0));
    emit(1, crystals);
    const float floor = .16F + progress * .023F;
    const auto shard = [&](float x, float z, float width, float height) {
        emit(10, MatrixMultiply(MatrixScale(width, height, width),
                                MatrixTranslate(x, floor - .015F, z)));
    };
    if (level >= 3) shard(.20F, -.21F, .42F, .60F);
    if (level >= 5) shard(.025F, -.035F, .70F, 1.30F + static_cast<float>(level - 5) * .06F);
    if (level >= 7) shard(.04F, .27F, .40F, .60F);
}
} // namespace ian::crystal_mine_model_detail
