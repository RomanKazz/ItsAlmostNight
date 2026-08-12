#pragma once

#include <cstdint>
#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace ian {

struct WorldConfig {
    double cellSize{1.0};
    double verticalGridStep{1.0};
    int maxStoreys{3};
    double maxWoodSupportLength{4.0};
    int terrainResolution{513};
    double terrainWorldSize{384.0};
    double terrainAmplitude{9.0};
    double terrainFrequency{0.032};
    std::uint32_t terrainSeed{73129U};
    double terrainFeatureSize{78.0};
    double terrainTerraceHeight{2.5};
    double terrainSlopeWidth{28.0};
    double terrainSurfaceNoiseAmplitude{0.08};
    double terrainBuildPlateauRadius{13.0};
    double coreFlatRadius{13.0};
    double terrainBoundaryRiseWidth{48.0};
    double terrainBoundaryRiseHeight{34.0};
    double terrainChunkWorldSize{48.0};
    double terrainRenderDistance{156.0};
    double buildPreviewDistance{10.0};
    double minimumGroundClearance{0.12};
    double maximumFoundationHeightDifference{1.35};
    int pondMinimumCount{3};
    int pondMaximumCount{7};
    double pondMaximumAreaFraction{0.12};
    double pondMinimumDepth{0.65};
    double pondMaximumDepth{1.55};
    double pondMinimumRadius{14.0};
    double pondMaximumRadius{30.0};
    double pondShorelineWidth{2.4};
    double pondWaveSpeed{0.22};
    double pondBuildDepthLimit{0.35};
    double pondShallowMovementMultiplier{0.62};
    double pondDeepWaterDepth{0.78};
    std::uint32_t pondSeed{92821U};
    std::array<double, 3> pondShallowColor{{0.27, 0.84, 0.82}};
    std::array<double, 3> pondDeepColor{{0.075, 0.35, 0.65}};

    [[nodiscard]] static WorldConfig defaults();
};

struct WorldConfigLoadResult {
    WorldConfig config;
    std::vector<std::string> errors;

    [[nodiscard]] bool valid() const {
        return errors.empty();
    }
};

[[nodiscard]] WorldConfigLoadResult
parseWorldConfig(std::string_view json);
[[nodiscard]] WorldConfigLoadResult
loadWorldConfig(std::string_view path);

} // namespace ian
