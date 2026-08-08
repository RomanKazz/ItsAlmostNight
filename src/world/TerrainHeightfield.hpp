#pragma once

#include "core/Types.hpp"
#include "world/WorldConfig.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace ian {

struct PondDefinition {
    double x{};
    double z{};
    double radiusX{};
    double radiusZ{};
    double rotation{};
    double waterLevel{};
    double depth{};
    double phase{};
    double bayAngle{};
    double bayRadius{};
    double islandX{};
    double islandZ{};
    double islandRadius{};
};

class TerrainHeightfield {
  public:
    explicit TerrainHeightfield(
        WorldConfig config = WorldConfig::defaults());

    void generate(std::uint32_t seed);

    [[nodiscard]] double getHeight(
        double worldX, double worldZ) const;
    // Height used by the visual mountain backdrop outside the playable
    // terrain. It is intentionally not part of collision or raycast space.
    [[nodiscard]] double getBackdropHeight(
        double worldX, double worldZ) const;
    [[nodiscard]] Vec3 getNormal(
        double worldX, double worldZ) const;
    [[nodiscard]] bool isInside(
        double worldX, double worldZ) const;
    [[nodiscard]] std::optional<Vec3> raycast(
        Vec3 origin, Vec3 direction,
        double maximumDistance) const;
    [[nodiscard]] std::pair<double, double>
    minMaxHeight() const;

    [[nodiscard]] const WorldConfig& config() const;
    [[nodiscard]] std::uint32_t seed() const;
    [[nodiscard]] int resolution() const;
    [[nodiscard]] double spacing() const;
    [[nodiscard]] std::span<const float> samples() const;
    [[nodiscard]] std::span<const PondDefinition> ponds() const;
    [[nodiscard]] double waterSignedDistance(
        double worldX, double worldZ) const;
    [[nodiscard]] double waterDepth(
        double worldX, double worldZ) const;
    [[nodiscard]] std::optional<double> waterSurfaceHeight(
        double worldX, double worldZ) const;
    [[nodiscard]] bool isDeepWater(
        double worldX, double worldZ) const;
    [[nodiscard]] double waterMovementMultiplier(
        double worldX, double worldZ) const;

  private:
    [[nodiscard]] std::size_t sampleIndex(
        int x, int z) const;

    WorldConfig config_;
    std::uint32_t seed_{};
    double spacing_{};
    std::vector<float> heights_;
    std::vector<PondDefinition> ponds_;
    double minimumHeight_{};
    double maximumHeight_{};
};

} // namespace ian
