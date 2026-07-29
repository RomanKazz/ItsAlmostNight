#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ian {

struct WorldConfig {
    double cellSize{1.0};
    double verticalGridStep{1.0};
    int maxStoreys{3};
    double maxWoodSupportLength{4.0};
    int terrainResolution{129};
    double terrainWorldSize{96.0};
    double terrainAmplitude{3.2};
    double terrainFrequency{0.032};
    std::uint32_t terrainSeed{73129U};
    double coreFlatRadius{10.0};
    double buildPreviewDistance{10.0};
    double minimumGroundClearance{0.12};

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
