#pragma once
#include "core/Types.hpp"
#include <cmath>

namespace ian {
[[nodiscard]] inline Vec3 crossbowMuzzleWorldPosition(Vec3 position, double yaw, double pitch) {
    constexpr double PivotY = 0.14931100606918335;
    constexpr Vec3 Muzzle{0.0, 0.7464950680732727, -0.1994807869195938};
    const double y = PivotY + (Muzzle.y - PivotY) * std::cos(pitch) - Muzzle.z * std::sin(pitch);
    const double z = (Muzzle.y - PivotY) * std::sin(pitch) + Muzzle.z * std::cos(pitch);
    return {position.x + z * std::sin(yaw), position.y + y, position.z + z * std::cos(yaw)};
}
[[nodiscard]] inline Vec3 gunTurretMuzzleWorldPosition(Vec3 position, double yaw, std::size_t muzzle) {
    // Authored sockets including the barrel parent transform in gun_turret.glb.
    const double x = muzzle % 2 == 0 ? 0.1403883397579193 : -0.14038792252540588;
    constexpr double y = 0.6854454874992371;
    constexpr double z = -0.6140207319986075;
    return {position.x + x * std::cos(yaw) + z * std::sin(yaw), position.y + y,
        position.z - x * std::sin(yaw) + z * std::cos(yaw)};
}
inline constexpr double FlamethrowerPitchPivotY = 0.924;
[[nodiscard]] inline Vec3 flamethrowerMuzzleWorldPosition(Vec3 position, double yaw, double pitch) {
    constexpr double SocketZ = -0.59;
    const double z = SocketZ * std::cos(pitch);
    return {position.x + z * std::sin(yaw),
        position.y + FlamethrowerPitchPivotY - SocketZ * std::sin(pitch),
        position.z + z * std::cos(yaw)};
}
inline constexpr double MissileLauncherMinimumRange = 2.5;
inline constexpr double MissileLauncherPitchPivotY = 1.09;
inline constexpr double MissileLauncherMinimumPitch = -8.0 * 3.14159265358979323846 / 180.0;
inline constexpr double MissileLauncherMaximumPitch = 30.0 * 3.14159265358979323846 / 180.0;
[[nodiscard]] inline Vec3 missileLauncherMuzzleWorldPosition(Vec3 position, double yaw, double pitch, std::size_t muzzle) {
    const double x = muzzle % 2 == 0 ? -0.113 : 0.113;
    const double localY = muzzle % 4 < 2 ? 0.113 : -0.113;
    constexpr double localZ = -0.4388;
    const double y = MissileLauncherPitchPivotY + localY * std::cos(pitch) - localZ * std::sin(pitch);
    const double z = localY * std::sin(pitch) + localZ * std::cos(pitch);
    return {position.x + x * std::cos(yaw) + z * std::sin(yaw), position.y + y,
        position.z - x * std::sin(yaw) + z * std::cos(yaw)};
}
} // namespace ian

namespace ian {
inline constexpr double LaserPitchPivotY = 1.020;
inline constexpr double LaserPitchPivotZ = .170;
inline constexpr double LaserMinimumPitch = -12.0 * 3.14159265358979323846 / 180.0;
inline constexpr double LaserMaximumPitch = 20.0 * 3.14159265358979323846 / 180.0;
[[nodiscard]] inline Vec3 laserMuzzleWorldPosition(Vec3 position, double yaw, double pitch) {
    constexpr double SocketFromPivotZ = -.629;
    const double y = LaserPitchPivotY - SocketFromPivotZ * std::sin(pitch);
    const double z = LaserPitchPivotZ + SocketFromPivotZ * std::cos(pitch);
    return {position.x + z * std::sin(yaw), position.y + y, position.z + z * std::cos(yaw)};
}
}
