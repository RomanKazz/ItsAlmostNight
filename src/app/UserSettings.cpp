#include "app/UserSettings.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>

namespace ian {
namespace {

using Json = nlohmann::json;

template <typename Value>
void readValue(const Json& object, const char* name, Value& destination) {
    const auto value = object.find(name);
    if (value == object.end()) {
        return;
    }
    try {
        destination = value->get<Value>();
    } catch (...) {
    }
}

float finiteClamped(float value, float fallback,
                    float minimum, float maximum) {
    return std::isfinite(value)
               ? std::clamp(value, minimum, maximum)
               : fallback;
}

int nearestPixelSize(int value) {
    constexpr std::array PixelSizes{1, 2, 3, 4, 6, 8};
    return *std::min_element(
        PixelSizes.begin(), PixelSizes.end(),
        [value](int left, int right) {
            return std::abs(left - value) <
                   std::abs(right - value);
        });
}

int nearestShadowMapSize(int value) {
    constexpr std::array ShadowMapSizes{512, 1024, 2048};
    return *std::min_element(
        ShadowMapSizes.begin(), ShadowMapSizes.end(),
        [value](int left, int right) {
            return std::abs(left - value) <
                   std::abs(right - value);
        });
}

int nearestFrameRateLimit(int value) {
    constexpr std::array FrameRateLimits{0, 60, 120, 144};
    return *std::min_element(
        FrameRateLimits.begin(), FrameRateLimits.end(),
        [value](int left, int right) {
            return std::abs(left - value) <
                   std::abs(right - value);
        });
}

const char* qualityName(GraphicsQuality quality) {
    switch (quality) {
    case GraphicsQuality::Low:
        return "low";
    case GraphicsQuality::Medium:
        return "medium";
    case GraphicsQuality::High:
        return "high";
    }
    return "high";
}

void readGraphics(const Json& value, GraphicsSettings& settings) {
    readValue(value, "sky", settings.sky);
    readValue(value, "worldShader", settings.worldShader);
    readValue(value, "shadows", settings.shadows);
    readValue(value, "fog", settings.fog);
    readValue(value, "postProcessing", settings.postProcessing);
    readValue(value, "particles", settings.particles);
    readValue(value, "grass", settings.grass);
    readValue(value, "contactAo", settings.blobShadows);
    readValue(value, "bloom", settings.bloom);
    readValue(value, "ssao", settings.ssao);
    readValue(value, "paletteQuantization", settings.paletteQuantization);
    readValue(value, "dithering", settings.dithering);
    readValue(value, "posterizedLighting", settings.posterizedLighting);
    readValue(value, "inkOutlines", settings.inkOutlines);
    readValue(value, "fogBands", settings.fogBands);
    readValue(value, "paperGrain", settings.paperGrain);
    readValue(value, "shadowMapSize", settings.shadowMapSize);
    readValue(value, "frameRateLimit", settings.frameRateLimit);
    readValue(value, "pixelSize", settings.pixelSize);
    readValue(value, "shadowDistance", settings.shadowDistance);
    readValue(value, "constantBias", settings.constantBias);
    readValue(value, "slopeBias", settings.slopeBias);
    readValue(value, "shadowStrength", settings.shadowStrength);
    readValue(value, "aoStrength", settings.aoStrength);
    readValue(value, "postExposure", settings.postExposure);
    readValue(value, "brightness", settings.brightness);
    readValue(value, "contrast", settings.contrast);
    readValue(value, "colorSaturation", settings.colorSaturation);
    readValue(value, "hueDegrees", settings.hueDegrees);
    readValue(value, "temperature", settings.temperature);
    readValue(value, "tint", settings.tint);
    readValue(value, "gamma", settings.gamma);
    readValue(value, "blackPoint", settings.blackPoint);
    readValue(value, "curveShadows", settings.curveShadows);
    readValue(value, "curveMidtones", settings.curveMidtones);
    readValue(value, "curveHighlights", settings.curveHighlights);
    readValue(value, "sharpness", settings.sharpness);
    readValue(value, "vignette", settings.vignette);
    readValue(value, "paletteLevels", settings.paletteLevels);
    readValue(value, "ditherStrength", settings.ditherStrength);
    readValue(value, "lightingSteps", settings.lightingSteps);
    readValue(value, "bloomStrength", settings.bloomStrength);
    readValue(value, "outlineStrength", settings.outlineStrength);
    readValue(value, "outlineWidth", settings.outlineWidth);
    readValue(value, "fogBandCount", settings.fogBandCount);
    readValue(value, "paperGrainStrength", settings.paperGrainStrength);

    std::string quality;
    readValue(value, "quality", quality);
    if (quality == "low") {
        settings.quality = GraphicsQuality::Low;
    } else if (quality == "medium") {
        settings.quality = GraphicsQuality::Medium;
    } else if (quality == "high") {
        settings.quality = GraphicsQuality::High;
    }

    const GraphicsSettings defaults;
    settings.shadowMapSize = nearestShadowMapSize(settings.shadowMapSize);
    settings.frameRateLimit = nearestFrameRateLimit(settings.frameRateLimit);
    settings.pixelSize = nearestPixelSize(settings.pixelSize);
    settings.shadowDistance = finiteClamped(
        settings.shadowDistance, defaults.shadowDistance, 10.0F, 180.0F);
    settings.constantBias = finiteClamped(
        settings.constantBias, defaults.constantBias, 0.0F, 0.02F);
    settings.slopeBias = finiteClamped(
        settings.slopeBias, defaults.slopeBias, 0.0F, 0.05F);
    settings.shadowStrength = finiteClamped(
        settings.shadowStrength, defaults.shadowStrength, 0.0F, 1.0F);
    settings.aoStrength = finiteClamped(
        settings.aoStrength, defaults.aoStrength, 0.0F, 1.0F);
    settings.postExposure = finiteClamped(
        settings.postExposure, defaults.postExposure, -2.0F, 2.0F);
    settings.brightness = finiteClamped(
        settings.brightness, defaults.brightness, -0.5F, 0.5F);
    settings.contrast = finiteClamped(
        settings.contrast, defaults.contrast, 0.5F, 1.8F);
    settings.colorSaturation = finiteClamped(
        settings.colorSaturation, defaults.colorSaturation, 0.0F, 2.0F);
    settings.hueDegrees = finiteClamped(
        settings.hueDegrees, defaults.hueDegrees, -180.0F, 180.0F);
    settings.temperature = finiteClamped(
        settings.temperature, defaults.temperature, -1.0F, 1.0F);
    settings.tint = finiteClamped(
        settings.tint, defaults.tint, -1.0F, 1.0F);
    settings.gamma = finiteClamped(
        settings.gamma, defaults.gamma, 0.5F, 2.2F);
    settings.blackPoint = finiteClamped(
        settings.blackPoint, defaults.blackPoint, 0.0F, 0.25F);
    settings.curveShadows = finiteClamped(
        settings.curveShadows, defaults.curveShadows, -1.0F, 1.0F);
    settings.curveMidtones = finiteClamped(
        settings.curveMidtones, defaults.curveMidtones, -1.0F, 1.0F);
    settings.curveHighlights = finiteClamped(
        settings.curveHighlights, defaults.curveHighlights, -1.0F, 1.0F);
    settings.sharpness = finiteClamped(
        settings.sharpness, defaults.sharpness, 0.0F, 1.0F);
    settings.vignette = finiteClamped(
        settings.vignette, defaults.vignette, 0.0F, 0.7F);
    settings.paletteLevels = finiteClamped(
        settings.paletteLevels, defaults.paletteLevels, 2.0F, 16.0F);
    settings.ditherStrength = finiteClamped(
        settings.ditherStrength, defaults.ditherStrength, 0.0F, 1.0F);
    settings.lightingSteps = finiteClamped(
        settings.lightingSteps, defaults.lightingSteps, 2.0F, 12.0F);
    settings.bloomStrength = finiteClamped(
        settings.bloomStrength, defaults.bloomStrength, 0.0F, 1.0F);
    settings.outlineStrength = finiteClamped(
        settings.outlineStrength, defaults.outlineStrength, 0.0F, 1.0F);
    settings.outlineWidth = finiteClamped(
        settings.outlineWidth, defaults.outlineWidth, 1.0F, 4.0F);
    settings.fogBandCount = finiteClamped(
        settings.fogBandCount, defaults.fogBandCount, 2.0F, 12.0F);
    settings.paperGrainStrength = finiteClamped(
        settings.paperGrainStrength, defaults.paperGrainStrength,
        0.0F, 0.15F);
}

Json graphicsJson(const GraphicsSettings& settings) {
    return {
        {"sky", settings.sky},
        {"worldShader", settings.worldShader},
        {"shadows", settings.shadows},
        {"fog", settings.fog},
        {"postProcessing", settings.postProcessing},
        {"particles", settings.particles},
        {"grass", settings.grass},
        {"contactAo", settings.blobShadows},
        {"bloom", settings.bloom},
        {"ssao", settings.ssao},
        {"paletteQuantization", settings.paletteQuantization},
        {"dithering", settings.dithering},
        {"posterizedLighting", settings.posterizedLighting},
        {"inkOutlines", settings.inkOutlines},
        {"fogBands", settings.fogBands},
        {"paperGrain", settings.paperGrain},
        {"shadowMapSize", settings.shadowMapSize},
        {"frameRateLimit", settings.frameRateLimit},
        {"pixelSize", settings.pixelSize},
        {"shadowDistance", settings.shadowDistance},
        {"constantBias", settings.constantBias},
        {"slopeBias", settings.slopeBias},
        {"shadowStrength", settings.shadowStrength},
        {"aoStrength", settings.aoStrength},
        {"postExposure", settings.postExposure},
        {"brightness", settings.brightness},
        {"contrast", settings.contrast},
        {"colorSaturation", settings.colorSaturation},
        {"hueDegrees", settings.hueDegrees},
        {"temperature", settings.temperature},
        {"tint", settings.tint},
        {"gamma", settings.gamma},
        {"blackPoint", settings.blackPoint},
        {"curveShadows", settings.curveShadows},
        {"curveMidtones", settings.curveMidtones},
        {"curveHighlights", settings.curveHighlights},
        {"sharpness", settings.sharpness},
        {"vignette", settings.vignette},
        {"paletteLevels", settings.paletteLevels},
        {"ditherStrength", settings.ditherStrength},
        {"lightingSteps", settings.lightingSteps},
        {"bloomStrength", settings.bloomStrength},
        {"outlineStrength", settings.outlineStrength},
        {"outlineWidth", settings.outlineWidth},
        {"fogBandCount", settings.fogBandCount},
        {"paperGrainStrength", settings.paperGrainStrength},
        {"quality", qualityName(settings.quality)},
    };
}

void readAudio(const Json& value, AudioSettings& settings) {
    readValue(value, "masterVolume", settings.masterVolume);
    readValue(value, "musicVolume", settings.musicVolume);
    readValue(value, "sfxVolume", settings.sfxVolume);
    readValue(value, "muted", settings.muted);
    const AudioSettings defaults;
    settings.masterVolume = finiteClamped(
        settings.masterVolume, defaults.masterVolume, 0.0F, 1.0F);
    settings.musicVolume = finiteClamped(
        settings.musicVolume, defaults.musicVolume, 0.0F, 1.0F);
    settings.sfxVolume = finiteClamped(
        settings.sfxVolume, defaults.sfxVolume, 0.0F, 1.0F);
}

void readMotion(const Json& value, MotionSettings& settings) {
    readValue(value, "bobIntensity", settings.bobIntensity);
    readValue(value, "shakeIntensity", settings.shakeIntensity);
    readValue(value, "landingIntensity", settings.landingIntensity);
    readValue(value, "swayIntensity", settings.swayIntensity);
    const MotionSettings defaults;
    settings.bobIntensity = finiteClamped(
        settings.bobIntensity, defaults.bobIntensity, 0.0F, 1.5F);
    settings.shakeIntensity = finiteClamped(
        settings.shakeIntensity, defaults.shakeIntensity, 0.0F, 1.5F);
    settings.landingIntensity = finiteClamped(
        settings.landingIntensity, defaults.landingIntensity, 0.0F, 1.5F);
    settings.swayIntensity = finiteClamped(
        settings.swayIntensity, defaults.swayIntensity, 0.0F, 1.5F);
}

void readControls(const Json& value, ControlSettings& settings) {
    readValue(value, "mouseSensitivity", settings.mouseSensitivity);
    readValue(value, "invertMouseY", settings.invertMouseY);
    const ControlSettings defaults;
    settings.mouseSensitivity = finiteClamped(
        settings.mouseSensitivity, defaults.mouseSensitivity,
        0.1F, 3.0F);
}

void readAccessibility(
    const Json& value, AccessibilitySettings& settings) {
    readValue(value, "showFps", settings.showFps);
    readValue(value, "reduceFlashes", settings.reduceFlashes);
}

} // namespace

bool loadUserSettings(
    std::string_view path, UserSettings& settings) {
    try {
        std::ifstream stream{std::string(path)};
        if (!stream) {
            return false;
        }
        const Json document = Json::parse(stream);
        if (!document.is_object()) {
            return false;
        }
        UserSettings loaded = settings;
        if (const auto value = document.find("graphics");
            value != document.end() && value->is_object()) {
            readGraphics(*value, loaded.graphics);
        }
        if (const auto value = document.find("audio");
            value != document.end() && value->is_object()) {
            readAudio(*value, loaded.audio);
        }
        if (const auto value = document.find("motion");
            value != document.end() && value->is_object()) {
            readMotion(*value, loaded.motion);
        }
        if (const auto value = document.find("controls");
            value != document.end() && value->is_object()) {
            readControls(*value, loaded.controls);
        }
        if (const auto value = document.find("accessibility");
            value != document.end() && value->is_object()) {
            readAccessibility(*value, loaded.accessibility);
        }
        settings = loaded;
        return true;
    } catch (...) {
        return false;
    }
}

bool saveUserSettings(
    std::string_view path, const UserSettings& settings) {
    try {
        const std::filesystem::path filePath{path};
        if (filePath.has_parent_path()) {
            std::filesystem::create_directories(filePath.parent_path());
        }
        const Json document{
            {"version", 2},
            {"graphics", graphicsJson(settings.graphics)},
            {"audio",
             {{"masterVolume", settings.audio.masterVolume},
              {"musicVolume", settings.audio.musicVolume},
              {"sfxVolume", settings.audio.sfxVolume},
              {"muted", settings.audio.muted}}},
            {"motion",
             {{"bobIntensity", settings.motion.bobIntensity},
              {"shakeIntensity", settings.motion.shakeIntensity},
              {"landingIntensity", settings.motion.landingIntensity},
              {"swayIntensity", settings.motion.swayIntensity}}},
            {"controls",
             {{"mouseSensitivity", settings.controls.mouseSensitivity},
              {"invertMouseY", settings.controls.invertMouseY}}},
            {"accessibility",
             {{"showFps", settings.accessibility.showFps},
              {"reduceFlashes", settings.accessibility.reduceFlashes}}},
        };
        std::ofstream stream(filePath);
        if (!stream) {
            return false;
        }
        stream << document.dump(2) << '\n';
        return static_cast<bool>(stream);
    } catch (...) {
        return false;
    }
}

void resetDisplaySettings(GraphicsSettings& settings) {
    const GraphicsSettings defaults;
    settings.sky = defaults.sky;
    settings.worldShader = defaults.worldShader;
    settings.shadows = defaults.shadows;
    settings.fog = defaults.fog;
    settings.postProcessing = defaults.postProcessing;
    settings.particles = defaults.particles;
    settings.grass = defaults.grass;
    settings.blobShadows = defaults.blobShadows;
    settings.bloom = defaults.bloom;
    settings.ssao = defaults.ssao;
    settings.shadowMapSize = defaults.shadowMapSize;
    settings.frameRateLimit = defaults.frameRateLimit;
    settings.pixelSize = defaults.pixelSize;
    settings.shadowDistance = defaults.shadowDistance;
    settings.constantBias = defaults.constantBias;
    settings.slopeBias = defaults.slopeBias;
    settings.shadowStrength = defaults.shadowStrength;
    settings.aoStrength = defaults.aoStrength;
    settings.quality = defaults.quality;
}

void resetColorSettings(GraphicsSettings& settings) {
    const GraphicsSettings defaults;
    settings.postExposure = defaults.postExposure;
    settings.brightness = defaults.brightness;
    settings.contrast = defaults.contrast;
    settings.colorSaturation = defaults.colorSaturation;
    settings.hueDegrees = defaults.hueDegrees;
    settings.temperature = defaults.temperature;
    settings.tint = defaults.tint;
    settings.gamma = defaults.gamma;
    settings.blackPoint = defaults.blackPoint;
    settings.curveShadows = defaults.curveShadows;
    settings.curveMidtones = defaults.curveMidtones;
    settings.curveHighlights = defaults.curveHighlights;
    settings.sharpness = defaults.sharpness;
    settings.vignette = defaults.vignette;
}

void resetStyleSettings(GraphicsSettings& settings) {
    const GraphicsSettings defaults;
    settings.paletteQuantization = defaults.paletteQuantization;
    settings.dithering = defaults.dithering;
    settings.posterizedLighting = defaults.posterizedLighting;
    settings.bloom = defaults.bloom;
    settings.inkOutlines = defaults.inkOutlines;
    settings.fogBands = defaults.fogBands;
    settings.paperGrain = defaults.paperGrain;
    settings.paletteLevels = defaults.paletteLevels;
    settings.ditherStrength = defaults.ditherStrength;
    settings.lightingSteps = defaults.lightingSteps;
    settings.bloomStrength = defaults.bloomStrength;
    settings.outlineStrength = defaults.outlineStrength;
    settings.outlineWidth = defaults.outlineWidth;
    settings.fogBandCount = defaults.fogBandCount;
    settings.paperGrainStrength = defaults.paperGrainStrength;
}

void applyGraphicsPreset(
    GraphicsSettings& settings, GraphicsQuality preset) {
    settings.quality = preset;
    settings.sky = true;
    settings.worldShader = true;
    settings.fog = true;
    settings.postProcessing = true;
    settings.particles = true;
    settings.blobShadows = true;
    settings.ssao = false;

    if (preset == GraphicsQuality::Low) {
        settings.shadows = false;
        settings.grass = false;
        settings.bloom = false;
        settings.shadowMapSize = 512;
        settings.shadowDistance = 32.0F;
        settings.pixelSize = 6;
        settings.aoStrength = 0.2F;
    } else if (preset == GraphicsQuality::Medium) {
        settings.shadows = true;
        settings.grass = true;
        settings.bloom = false;
        settings.shadowMapSize = 1024;
        settings.shadowDistance = 44.0F;
        settings.pixelSize = 4;
        settings.aoStrength = 0.3F;
    } else {
        settings.shadows = true;
        settings.grass = true;
        settings.bloom = true;
        settings.shadowMapSize = 2048;
        settings.shadowDistance = 55.0F;
        settings.pixelSize = 3;
        settings.aoStrength = 0.35F;
    }
}

std::optional<GraphicsQuality>
detectGraphicsPreset(const GraphicsSettings& settings) {
    for (const GraphicsQuality preset : {
             GraphicsQuality::Low,
             GraphicsQuality::Medium,
             GraphicsQuality::High}) {
        GraphicsSettings expected = settings;
        applyGraphicsPreset(expected, preset);
        if (settings.quality == expected.quality &&
            settings.sky == expected.sky &&
            settings.worldShader == expected.worldShader &&
            settings.shadows == expected.shadows &&
            settings.fog == expected.fog &&
            settings.postProcessing == expected.postProcessing &&
            settings.particles == expected.particles &&
            settings.grass == expected.grass &&
            settings.blobShadows == expected.blobShadows &&
            settings.bloom == expected.bloom &&
            settings.ssao == expected.ssao &&
            settings.shadowMapSize == expected.shadowMapSize &&
            settings.pixelSize == expected.pixelSize &&
            settings.shadowDistance == expected.shadowDistance &&
            settings.aoStrength == expected.aoStrength) {
            return preset;
        }
    }
    return std::nullopt;
}

} // namespace ian
