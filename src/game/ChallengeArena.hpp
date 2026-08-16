#pragma once

namespace ian::challenge_arena {

inline constexpr double Radius = 18.0;
inline constexpr double FenceRadius = 17.45;
inline constexpr double FenceHalfThickness = 0.70;
inline constexpr double ActorCollisionRadius = 0.55;
inline constexpr double InteriorActorRadius =
    FenceRadius - FenceHalfThickness - ActorCollisionRadius;

} // namespace ian::challenge_arena
