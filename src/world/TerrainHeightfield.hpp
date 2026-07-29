#pragma once

#include "core/Types.hpp"
#include "world/WorldConfig.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace ian {

class TerrainHeightfield {
  public:
    explicit TerrainHeightfield(
        WorldConfig config = WorldConfig::defaults());

    void generate(std::uint32_t seed);

    [[nodiscard]] double getHeight(
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

  private:
    [[nodiscard]] std::size_t sampleIndex(
        int x, int z) const;

    WorldConfig config_;
    std::uint32_t seed_{};
    double spacing_{};
    std::vector<float> heights_;
    double minimumHeight_{};
    double maximumHeight_{};
};

} // namespace ian
