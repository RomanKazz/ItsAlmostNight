#pragma once

#include <raylib.h>

#include <algorithm>
#include <cmath>

namespace ian {

struct SplitterPose {
    Vector3 stretch{1, 1, 1};
    float hop{};
    float roll{};
};

// Presentation only: foot-pivot squash/stretch, followed by a short hop.
// Existing split timers, child spawning and knockback remain authoritative.
inline SplitterPose splitterPose(bool parent, float progress) {
    const float p = std::isfinite(progress) ? std::clamp(progress, 0.0F, 1.0F) : 1.0F;
    const auto tween = [](float a, float b, float t) {
        t = std::clamp(t, 0.0F, 1.0F);
        return a + (b - a) * t * t * (3.0F - 2.0F * t);
    };
    const auto stretch = [&](Vector3 a, Vector3 b, float t) {
        return Vector3{tween(a.x, b.x, t), tween(a.y, b.y, t), tween(a.z, b.z, t)};
    };
    SplitterPose pose{};
    if (parent) {
        if (p < .28F) pose.stretch = stretch({1, 1, 1}, {1.16F, .68F, 1.12F}, p / .28F);
        else if (p < .62F) pose.stretch = stretch({1.16F, .68F, 1.12F}, {.83F, 1.38F, .85F}, (p - .28F) / .34F);
        else if (p < .82F) pose.stretch = stretch({.83F, 1.38F, .85F}, {1.26F, .72F, 1.2F}, (p - .62F) / .2F);
        else pose.stretch = stretch({1.26F, .72F, 1.2F}, {.44F, .38F, .44F}, (p - .82F) / .18F);
        pose.hop = .16F * std::pow(std::max(0.0F, std::sin((p - .28F) / .72F * PI)), 2.0F);
        pose.roll = .10F * std::sin(p * PI * 2) * std::sin(p * PI);
    } else {
        if (p < .44F) pose.stretch = stretch({.72F, 1.28F, .72F}, {.94F, 1.03F, .94F}, p / .44F);
        else if (p < .72F) pose.stretch = stretch({.94F, 1.03F, .94F}, {1.20F, .73F, 1.16F}, (p - .44F) / .28F);
        else pose.stretch = stretch({1.20F, .73F, 1.16F}, {1, 1, 1}, (p - .72F) / .28F);
        const float flight = std::clamp(p / .68F, 0.0F, 1.0F);
        pose.hop = .64F * std::max(0.0F, std::sin(flight * PI));
        pose.roll = -.18F * std::sin(p * PI * 2) * (1 - p);
        if (p >= 1) pose = {};
    }
    return pose;
}

} // namespace ian
