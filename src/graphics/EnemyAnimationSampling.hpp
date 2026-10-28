#pragma once

#include <raylib.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>

namespace ian {

inline constexpr float EnemyAnimationFps = 30.0F;
inline constexpr int EnemyCrowdPoseCount = 6;
inline constexpr float EnemySmoothAnimationDistance = 40.0F;

struct EnemyAnimationSample {
    int frame{};
    int nextFrame{};
    float blend{};
};

inline EnemyAnimationSample sampleEnemyAnimation(
    float seconds, int frameCount, bool loop, bool coarse = false) {
    if (frameCount <= 0) return {};
    float frame = std::isfinite(seconds) ? std::max(0.0F, seconds) * EnemyAnimationFps : 0.0F;
    frame = loop ? std::fmod(frame, static_cast<float>(frameCount))
                 : std::min(frame, static_cast<float>(frameCount - 1));
    const int current = static_cast<int>(frame);
    if (coarse) {
        const int pose = std::min(EnemyCrowdPoseCount - 1,
            current * EnemyCrowdPoseCount / frameCount);
        const int sampled = pose * frameCount / EnemyCrowdPoseCount;
        return {sampled, sampled, 0.0F};
    }
    return {current, loop ? (current + 1) % frameCount : std::min(current + 1, frameCount - 1),
        frame - static_cast<float>(current)};
}

// Matches the GPU's blend of adjacent skinning palettes for selection masks.
inline Matrix blendEnemyBoneMatrices(Matrix from, Matrix to, float blend) {
    const auto a = std::bit_cast<std::array<float, 16>>(from);
    const auto b = std::bit_cast<std::array<float, 16>>(to);
    std::array<float, 16> mixed{};
    for (std::size_t i = 0; i < mixed.size(); ++i) mixed[i] = a[i] + (b[i] - a[i]) * blend;
    return std::bit_cast<Matrix>(mixed);
}

} // namespace ian
