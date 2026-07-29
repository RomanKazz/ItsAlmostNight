#include "world/WorldConfig.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace ian {
namespace {

using Json = nlohmann::json;

void validate(const WorldConfig& config) {
    const bool validResolution =
        config.terrainResolution >= 3 &&
        config.terrainResolution <= 255;
    const bool validStoreys =
        config.maxStoreys > 0 &&
        config.maxStoreys <= 16;
    if (config.cellSize <= 0.0 ||
        config.verticalGridStep <= 0.0 ||
        !validStoreys ||
        config.maxWoodSupportLength <= 0.0 ||
        !validResolution ||
        config.terrainWorldSize <= config.cellSize * 4.0 ||
        config.terrainAmplitude < 0.0 ||
        config.terrainFrequency <= 0.0 ||
        config.coreFlatRadius < 0.0 ||
        config.coreFlatRadius >=
            config.terrainWorldSize * 0.5 ||
        config.buildPreviewDistance <= 0.0 ||
        config.minimumGroundClearance < 0.0) {
        throw std::runtime_error(
            "invalid world configuration");
    }
}

} // namespace

WorldConfig WorldConfig::defaults() {
    return {};
}

WorldConfigLoadResult
parseWorldConfig(std::string_view json) {
    WorldConfigLoadResult result{
        .config = WorldConfig::defaults()};
    try {
        const Json document = Json::parse(json);
        WorldConfig config{
            .cellSize =
                document.at("cellSize").get<double>(),
            .verticalGridStep =
                document.at("verticalGridStep").get<double>(),
            .maxStoreys =
                document.at("maxStoreys").get<int>(),
            .maxWoodSupportLength =
                document.at("maxWoodSupportLength").get<double>(),
            .terrainResolution =
                document.at("terrainResolution").get<int>(),
            .terrainWorldSize =
                document.at("terrainWorldSize").get<double>(),
            .terrainAmplitude =
                document.at("terrainAmplitude").get<double>(),
            .terrainFrequency =
                document.at("terrainFrequency").get<double>(),
            .terrainSeed =
                document.at("terrainSeed").get<std::uint32_t>(),
            .coreFlatRadius =
                document.at("coreFlatRadius").get<double>(),
            .buildPreviewDistance =
                document.at("buildPreviewDistance").get<double>(),
            .minimumGroundClearance =
                document.at("minimumGroundClearance").get<double>(),
        };
        validate(config);
        result.config = config;
    } catch (const std::exception& error) {
        result.errors.push_back(
            "world config: " + std::string(error.what()));
    }
    return result;
}

WorldConfigLoadResult
loadWorldConfig(std::string_view path) {
    try {
        std::ifstream stream{std::string(path)};
        if (!stream) {
            throw std::runtime_error(
                "cannot open " + std::string(path));
        }
        const std::string json{
            std::istreambuf_iterator<char>{stream},
            std::istreambuf_iterator<char>{}};
        return parseWorldConfig(json);
    } catch (const std::exception& error) {
        return {
            .config = WorldConfig::defaults(),
            .errors = {error.what()},
        };
    }
}

} // namespace ian
