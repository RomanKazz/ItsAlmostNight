#include "world/PondDecorationLayout.hpp"

#include "world/TerrainHeightfield.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace ian {
namespace {

[[nodiscard]] std::uint32_t decorHash(std::uint32_t value) {
    value ^= value >> 16U;
    value *= 0x7feb352dU;
    value ^= value >> 15U;
    value *= 0x846ca68bU;
    value ^= value >> 16U;
    return value;
}

[[nodiscard]] float decorUnit(std::uint32_t value) {
    return static_cast<float>(decorHash(value) & 0x00ffffffU) /
        static_cast<float>(0x01000000U);
}

struct Point2 {
    float x{};
    float z{};
};

[[nodiscard]] Point2 pondPoint(
    const PondDefinition& pond, float angle, float radial) {
    const float directionX = std::cos(angle);
    const float directionZ = std::sin(angle);
    const float localX =
        directionX * static_cast<float>(pond.radiusX) * radial;
    const float localZ =
        directionZ * static_cast<float>(pond.radiusZ) * radial;
    const float cosine =
        std::cos(static_cast<float>(pond.rotation));
    const float sine =
        std::sin(static_cast<float>(pond.rotation));
    return {
        static_cast<float>(pond.x) +
            localX * cosine - localZ * sine,
        static_cast<float>(pond.z) +
            localX * sine + localZ * cosine,
    };
}

} // namespace

std::vector<PondLilyPlacement>
generatePondLilyPlacements(const TerrainHeightfield& terrain) {
    std::vector<PondLilyPlacement> result;
    result.reserve(terrain.ponds().size() * 16U);

    constexpr int CandidateCount = 24;
    constexpr float GoldenAngle = 2.39996323F;
    constexpr float MinimumSpacing = 1.75F;
    constexpr std::array<double, 2> ModelRadii{
        0.07257224, 0.10572642};
    constexpr double ColliderInset = 0.86;
    constexpr double ModelTop = 0.01714863;

    std::size_t pondIndex = 0U;
    for (const PondDefinition& pond : terrain.ponds()) {
        const std::uint32_t pondHash = decorHash(
            terrain.seed() ^
            static_cast<std::uint32_t>(pondIndex + 1U) * 0x9e3779b9U);
        std::vector<Point2> acceptedPoints;
        acceptedPoints.reserve(CandidateCount);
        for (int item = 0; item < CandidateCount; ++item) {
            const std::uint32_t hash = decorHash(
                pondHash + static_cast<std::uint32_t>(item + 1) *
                    0x27d4eb2fU);
            const float angle = static_cast<float>(item) * GoldenAngle +
                (decorUnit(hash ^ 0x165667b1U) - 0.5F) * 0.34F;
            const float radial = 0.20F +
                decorUnit(hash ^ 0xd3a2646cU) * 0.56F;
            const Point2 point = pondPoint(pond, angle, radial);
            const bool overlaps = std::any_of(
                acceptedPoints.begin(), acceptedPoints.end(),
                [point](Point2 other) {
                    const float x = point.x - other.x;
                    const float z = point.z - other.z;
                    return x * x + z * z <
                        MinimumSpacing * MinimumSpacing;
                });
            if (overlaps) {
                continue;
            }
            const auto waterSurface =
                terrain.waterSurfaceHeight(point.x, point.z);
            if (!waterSurface ||
                terrain.waterDepth(point.x, point.z) < 0.08) {
                continue;
            }
            acceptedPoints.push_back(point);
            const std::size_t variant =
                static_cast<std::size_t>(hash & 1U);
            const double scale = variant == 0U
                ? 9.2 + decorUnit(hash ^ 0x63d83595U) * 2.8
                : 6.4 + decorUnit(hash ^ 0xb5297a4dU) * 2.4;
            const double modelOriginY = *waterSurface + 0.075;
            result.push_back({
                .position = {point.x, modelOriginY, point.z},
                .variant = variant,
                .scale = scale,
                .yaw = decorUnit(hash ^ 0x7f4a7c15U) *
                    std::numbers::pi_v<double> * 2.0,
                .collisionRadius =
                    ModelRadii[variant] * scale * ColliderInset,
                .surfaceHeight = modelOriginY + ModelTop * scale,
            });
        }
        ++pondIndex;
    }
    return result;
}

} // namespace ian
