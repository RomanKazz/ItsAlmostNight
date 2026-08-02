#include "graphics/Renderer.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>

namespace ian {
namespace {

using Json = nlohmann::json;

float finiteClamped(float value, float fallback,
                    float minimum, float maximum) {
    return std::isfinite(value)
               ? std::clamp(value, minimum, maximum)
               : fallback;
}

} // namespace

bool loadFirstPersonToolTuning(
    std::string_view path, FirstPersonToolTuning& tuning) {
    try {
        std::ifstream stream{std::string(path)};
        if (!stream) {
            return false;
        }
        const Json value = Json::parse(stream);
        FirstPersonToolTuning loaded = tuning;
        const Json& position = value.at("position");
        const Json& rotation = value.at("rotation");
        loaded.position = {
            finiteClamped(position.value("x", loaded.position.x),
                          loaded.position.x, -0.8F, 0.8F),
            finiteClamped(position.value("y", loaded.position.y),
                          loaded.position.y, -1.0F, 0.3F),
            finiteClamped(position.value("z", loaded.position.z),
                          loaded.position.z, -2.0F, -0.25F),
        };
        loaded.rotation = {
            finiteClamped(rotation.value("x", loaded.rotation.x),
                          loaded.rotation.x, -180.0F, 180.0F),
            finiteClamped(rotation.value("y", loaded.rotation.y),
                          loaded.rotation.y, -180.0F, 180.0F),
            finiteClamped(rotation.value("z", loaded.rotation.z),
                          loaded.rotation.z, -180.0F, 180.0F),
        };
        loaded.scale = finiteClamped(
            value.value("scale", loaded.scale), loaded.scale,
            0.2F, 2.0F);
        loaded.windupDegrees = finiteClamped(
            value.value("windupDegrees", loaded.windupDegrees),
            loaded.windupDegrees, -140.0F, 140.0F);
        loaded.strikeDegrees = finiteClamped(
            value.value("strikeDegrees", loaded.strikeDegrees),
            loaded.strikeDegrees, -160.0F, 160.0F);
        loaded.depthPush = finiteClamped(
            value.value("depthPush", loaded.depthPush),
            loaded.depthPush, -0.25F, 0.25F);
        loaded.swingDuration = finiteClamped(
            value.value("swingDuration", loaded.swingDuration),
            loaded.swingDuration, 0.15F, 1.5F);
        loaded.movementBob = finiteClamped(
            value.value("movementBob", loaded.movementBob),
            loaded.movementBob, 0.0F, 2.0F);
        loaded.outlineEnabled = value.value(
            "outlineEnabled", loaded.outlineEnabled);
        loaded.outlineWidth = finiteClamped(
            value.value("outlineWidth", loaded.outlineWidth),
            loaded.outlineWidth, 0.5F, 5.0F);
        loaded.outlineStrength = finiteClamped(
            value.value("outlineStrength", loaded.outlineStrength),
            loaded.outlineStrength, 0.0F, 1.0F);
        loaded.rimStrength = finiteClamped(
            value.value("rimStrength", loaded.rimStrength),
            loaded.rimStrength, 0.0F, 1.5F);
        loaded.brightness = finiteClamped(
            value.value("brightness", loaded.brightness),
            loaded.brightness, 0.5F, 2.0F);
        loaded.saturation = finiteClamped(
            value.value("saturation", loaded.saturation),
            loaded.saturation, 0.0F, 2.0F);
        tuning = loaded;
        return true;
    } catch (...) {
        return false;
    }
}

bool saveFirstPersonToolTuning(
    std::string_view path,
    const FirstPersonToolTuning& tuning) {
    try {
        const std::filesystem::path filePath{path};
        if (filePath.has_parent_path()) {
            std::filesystem::create_directories(
                filePath.parent_path());
        }
        const Json value{
            {"position",
             {{"x", tuning.position.x},
              {"y", tuning.position.y},
              {"z", tuning.position.z}}},
            {"rotation",
             {{"x", tuning.rotation.x},
              {"y", tuning.rotation.y},
              {"z", tuning.rotation.z}}},
            {"scale", tuning.scale},
            {"windupDegrees", tuning.windupDegrees},
            {"strikeDegrees", tuning.strikeDegrees},
            {"depthPush", tuning.depthPush},
            {"swingDuration", tuning.swingDuration},
            {"movementBob", tuning.movementBob},
            {"outlineEnabled", tuning.outlineEnabled},
            {"outlineWidth", tuning.outlineWidth},
            {"outlineStrength", tuning.outlineStrength},
            {"rimStrength", tuning.rimStrength},
            {"brightness", tuning.brightness},
            {"saturation", tuning.saturation},
        };
        std::ofstream stream(filePath);
        if (!stream) {
            return false;
        }
        stream << value.dump(2) << '\n';
        return static_cast<bool>(stream);
    } catch (...) {
        return false;
    }
}

} // namespace ian
