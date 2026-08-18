#pragma once

#include "buildings/BuildingSystem.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ian {

inline constexpr double PiRadians = 3.14159265358979323846;

[[nodiscard]] inline bool isDirectionalDefense(
    BuildingType type) {
    return type == BuildingType::Turret ||
           type == BuildingType::GunTurret ||
           type == BuildingType::Cannon ||
           type == BuildingType::Catapult;
}

[[nodiscard]] inline bool supportsManualBuildingRotation(
    BuildingType type) {
    return isDirectionalDefense(type) ||
           type == BuildingType::Wall ||
           type == BuildingType::Gate;
}

[[nodiscard]] inline std::uint8_t buildingRotationStepCount(
    BuildingType type) {
    return isDirectionalDefense(type) ? 8U : 4U;
}

[[nodiscard]] inline double buildingRotationYaw(
    BuildingType type, std::uint8_t rotation) {
    const double step = isDirectionalDefense(type)
        ? PiRadians * 0.25
        : PiRadians * 0.5;
    return static_cast<double>(rotation) * step;
}

// Directional defenses begin with a meaningful 90-degree firing sector.
// Every building level adds ten degrees, reaching 160 degrees at level 8.
[[nodiscard]] inline double defenseAttackArcDegrees(
    std::uint8_t level) {
    const int clampedLevel = std::clamp(
        static_cast<int>(level), 1, 8);
    return 90.0 + 10.0 * static_cast<double>(clampedLevel - 1);
}

[[nodiscard]] inline double defenseAttackHalfAngleRadians(
    std::uint8_t level) {
    return defenseAttackArcDegrees(level) *
        PiRadians / 360.0;
}

[[nodiscard]] inline double wrapBuildingAngle(double angle) {
    constexpr double TwoPi = PiRadians * 2.0;
    while (angle > PiRadians) angle -= TwoPi;
    while (angle < -PiRadians) angle += TwoPi;
    return angle;
}

[[nodiscard]] inline double smoothBuildingAngle(
    double current, double target, double deltaSeconds,
    double responsiveness = 18.0) {
    const double blend = 1.0 - std::exp(
        -std::max(0.0, responsiveness) *
        std::max(0.0, deltaSeconds));
    return wrapBuildingAngle(
        current + wrapBuildingAngle(target - current) * blend);
}

[[nodiscard]] inline bool directionInsideDefenseArc(
    Vec3 origin, Vec3 target, double restYaw,
    std::uint8_t level) {
    const double deltaX = target.x - origin.x;
    const double deltaZ = target.z - origin.z;
    if (deltaX * deltaX + deltaZ * deltaZ <= 1e-10) {
        return true;
    }
    const double targetYaw = std::atan2(-deltaX, -deltaZ);
    return std::abs(wrapBuildingAngle(targetYaw - restYaw)) <=
        defenseAttackHalfAngleRadians(level) + 1e-9;
}

} // namespace ian
