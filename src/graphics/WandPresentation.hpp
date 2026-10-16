#pragma once

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace ian::wand_presentation {

inline float ease(float t) {
    t = std::clamp(t, 0.0F, 1.0F);
    return t * t * (3.0F - 2.0F * t);
}

inline float chargeProgress(double remaining, double duration) {
    return remaining > 0.0 && duration > 0.0
        ? std::clamp(static_cast<float>(1.0 - remaining / duration), 0.0F, 1.0F)
        : 0.0F;
}

struct Pose {
    Vector3 offset{};
    Vector3 rotation{};
};

inline Pose mix(Pose a, Pose b, float t) {
    const auto component = [t](float x, float y) { return x + (y - x) * t; };
    return {{component(a.offset.x, b.offset.x), component(a.offset.y, b.offset.y),
             component(a.offset.z, b.offset.z)},
            {component(a.rotation.x, b.rotation.x), component(a.rotation.y, b.rotation.y),
             component(a.rotation.z, b.rotation.z)}};
}

// Recoil is the existing countdown (1 at release, 0 at rest). The release
// begins at the fully charged pose, then thrusts, kicks back and settles.
inline Pose pose(bool fire, float charge, float recoilRemaining) {
    const Pose ready = fire
        ? Pose{{-.028F, .026F, .032F}, {11, -6, -13}}
        : Pose{{-.014F, .032F, .018F}, {8, 7, 9}};
    if (recoilRemaining <= 0.0F) return mix({}, ready, ease(charge));
    const Pose thrust = fire
        ? Pose{{-.046F, .008F, -.095F}, {-21, 3, 8}}
        : Pose{{-.033F, .015F, -.075F}, {-16, -4, -6}};
    const Pose kick = fire
        ? Pose{{-.008F, -.012F, .032F}, {5, -1, -6}}
        : Pose{{.008F, -.006F, .035F}, {5, 2, 4}};
    const float t = 1.0F - std::clamp(recoilRemaining, 0.0F, 1.0F);
    const float contact = fire ? .23F : .18F;
    const float recovery = fire ? .52F : .43F;
    if (t < contact) return mix(ready, thrust, ease(t / contact));
    if (t < recovery) return mix(thrust, kick, ease((t - contact) / (recovery - contact)));
    return mix(kick, {}, ease((t - recovery) / (1.0F - recovery)));
}

inline float releaseFlash(float recoilRemaining) {
    const float t = 1.0F - std::clamp(recoilRemaining, 0.0F, 1.0F);
    return recoilRemaining > 0.0F
        ? ease(t / .15F) * (1.0F - ease((t - .18F) / .42F)) : 0.0F;
}

} // namespace ian::wand_presentation
