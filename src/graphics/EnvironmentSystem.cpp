#include "graphics/EnvironmentSystem.hpp"

#include <nlohmann/json.hpp>
#include <raymath.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <utility>

namespace ian {
namespace {

using Json = nlohmann::json;

constexpr std::array<const char*, 4> ProfileNames{
    "dawn",
    "day",
    "dusk",
    "night",
};

float wrapTime(float value) {
    value = std::fmod(value, 1.0F);
    return value < 0.0F ? value + 1.0F : value;
}

Color lerpColor(Color from, Color to, float amount) {
    const auto channel = [amount](unsigned char start, unsigned char end) {
        return static_cast<unsigned char>(
            static_cast<float>(start) +
            (static_cast<float>(end) - static_cast<float>(start)) * amount);
    };
    return {
        channel(from.r, to.r),
        channel(from.g, to.g),
        channel(from.b, to.b),
        channel(from.a, to.a),
    };
}

EnvironmentProfile lerpProfile(const EnvironmentProfile& from,
                               const EnvironmentProfile& to, float amount) {
    return {
        .skyTop = lerpColor(from.skyTop, to.skyTop, amount),
        .skyHorizon = lerpColor(from.skyHorizon, to.skyHorizon, amount),
        .lowerSky = lerpColor(from.lowerSky, to.lowerSky, amount),
        .fogColor = lerpColor(from.fogColor, to.fogColor, amount),
        .celestialDirection =
            Vector3Normalize(Vector3Lerp(from.celestialDirection,
                                        to.celestialDirection, amount)),
        .celestialColor =
            Vector3Lerp(from.celestialColor, to.celestialColor, amount),
        .sunColor = Vector3Lerp(from.sunColor, to.sunColor, amount),
        .skyAmbientColor =
            Vector3Lerp(from.skyAmbientColor, to.skyAmbientColor, amount),
        .groundAmbientColor =
            Vector3Lerp(from.groundAmbientColor, to.groundAmbientColor,
                        amount),
        .dayNightTint =
            Vector3Lerp(from.dayNightTint, to.dayNightTint, amount),
        .sunIntensity =
            from.sunIntensity + (to.sunIntensity - from.sunIntensity) * amount,
        .ambientIntensity =
            from.ambientIntensity +
            (to.ambientIntensity - from.ambientIntensity) * amount,
        .fogStart = from.fogStart + (to.fogStart - from.fogStart) * amount,
        .fogEnd = from.fogEnd + (to.fogEnd - from.fogEnd) * amount,
        .exposure = from.exposure + (to.exposure - from.exposure) * amount,
        .saturation =
            from.saturation + (to.saturation - from.saturation) * amount,
        .nightFactor =
            from.nightFactor + (to.nightFactor - from.nightFactor) * amount,
    };
}

Color parseColor(const Json& value) {
    const auto channels = value.get<std::array<int, 3>>();
    for (const int channel : channels) {
        if (channel < 0 || channel > 255) {
            throw std::runtime_error("environment color channel out of range");
        }
    }
    return {
        static_cast<unsigned char>(channels[0]),
        static_cast<unsigned char>(channels[1]),
        static_cast<unsigned char>(channels[2]),
        255,
    };
}

Vector3 parseVector(const Json& value) {
    const auto components = value.get<std::array<float, 3>>();
    return {components[0], components[1], components[2]};
}

EnvironmentProfile parseProfile(const Json& value) {
    EnvironmentProfile profile{
        .skyTop = parseColor(value.at("skyTop")),
        .skyHorizon = parseColor(value.at("skyHorizon")),
        .lowerSky = parseColor(value.at("lowerSky")),
        .fogColor = parseColor(value.at("fogColor")),
        .celestialDirection =
            Vector3Normalize(parseVector(value.at("celestialDirection"))),
        .celestialColor = parseVector(value.at("celestialColor")),
        .sunColor = parseVector(value.at("sunColor")),
        .skyAmbientColor = parseVector(value.at("skyAmbientColor")),
        .groundAmbientColor = parseVector(value.at("groundAmbientColor")),
        .dayNightTint = parseVector(value.at("dayNightTint")),
        .sunIntensity = value.at("sunIntensity").get<float>(),
        .ambientIntensity = value.at("ambientIntensity").get<float>(),
        .fogStart = value.at("fogStart").get<float>(),
        .fogEnd = value.at("fogEnd").get<float>(),
        .exposure = value.at("exposure").get<float>(),
        .saturation = value.at("saturation").get<float>(),
        .nightFactor = value.at("nightFactor").get<float>(),
    };
    if (Vector3LengthSqr(profile.celestialDirection) < 0.5F ||
        profile.sunIntensity < 0.0F || profile.sunIntensity > 4.0F ||
        profile.ambientIntensity < 0.0F ||
        profile.ambientIntensity > 4.0F || profile.fogStart < 0.0F ||
        profile.fogEnd <= profile.fogStart || profile.exposure <= 0.0F ||
        profile.exposure > 4.0F || profile.saturation < 0.0F ||
        profile.saturation > 2.0F || profile.nightFactor < 0.0F ||
        profile.nightFactor > 1.0F) {
        throw std::runtime_error("invalid environment profile");
    }
    return profile;
}

} // namespace

EnvironmentSystem::EnvironmentSystem(
    std::array<EnvironmentProfile, 4> profiles)
    : profiles_(std::move(profiles)) {}

std::array<EnvironmentProfile, 4> EnvironmentSystem::defaults() {
    return {{
        {
            .skyTop = {105, 85, 168, 255},
            .skyHorizon = {255, 157, 76, 255},
            .lowerSky = {141, 78, 72, 255},
            .fogColor = {238, 160, 105, 255},
            .celestialDirection = {-0.76F, 0.18F, -0.24F},
            .celestialColor = {1.0F, 0.64F, 0.38F},
            .sunColor = {1.0F, 0.66F, 0.42F},
            .skyAmbientColor = {0.74F, 0.8F, 0.94F},
            .groundAmbientColor = {0.3F, 0.34F, 0.28F},
            .dayNightTint = {1.0F, 0.9F, 0.84F},
            .sunIntensity = 0.92F,
            .ambientIntensity = 0.36F,
            .fogStart = 38.0F,
            .fogEnd = 112.0F,
            .exposure = 0.98F,
            .saturation = 1.08F,
            .nightFactor = 0.35F,
        },
        {
            .skyTop = {72, 159, 224, 255},
            .skyHorizon = {158, 207, 228, 255},
            .lowerSky = {113, 183, 211, 255},
            .fogColor = {174, 211, 218, 255},
            .celestialDirection = {0.42F, 0.86F, 0.28F},
            .celestialColor = {1.0F, 0.83F, 0.52F},
            .sunColor = {1.0F, 0.91F, 0.78F},
            .skyAmbientColor = {0.82F, 0.9F, 1.0F},
            .groundAmbientColor = {0.3F, 0.4F, 0.28F},
            .dayNightTint = {1.0F, 0.98F, 0.94F},
            .sunIntensity = 1.0F,
            .ambientIntensity = 0.36F,
            .fogStart = 45.0F,
            .fogEnd = 128.0F,
            .exposure = 0.97F,
            .saturation = 1.1F,
            .nightFactor = 0.0F,
        },
        {
            .skyTop = {101, 57, 151, 255},
            .skyHorizon = {255, 116, 54, 255},
            .lowerSky = {137, 57, 67, 255},
            .fogColor = {230, 126, 82, 255},
            .celestialDirection = {0.88F, 0.12F, 0.26F},
            .celestialColor = {1.0F, 0.47F, 0.2F},
            .sunColor = {1.0F, 0.47F, 0.25F},
            .skyAmbientColor = {0.72F, 0.66F, 0.84F},
            .groundAmbientColor = {0.28F, 0.27F, 0.29F},
            .dayNightTint = {1.0F, 0.82F, 0.74F},
            .sunIntensity = 0.9F,
            .ambientIntensity = 0.34F,
            .fogStart = 35.0F,
            .fogEnd = 108.0F,
            .exposure = 0.97F,
            .saturation = 1.1F,
            .nightFactor = 0.55F,
        },
        {
            .skyTop = {8, 15, 42, 255},
            .skyHorizon = {34, 58, 96, 255},
            .lowerSky = {16, 31, 58, 255},
            .fogColor = {34, 54, 84, 255},
            .celestialDirection = {-0.38F, 0.78F, -0.48F},
            .celestialColor = {0.62F, 0.74F, 1.0F},
            .sunColor = {0.48F, 0.62F, 0.92F},
            .skyAmbientColor = {0.36F, 0.48F, 0.72F},
            .groundAmbientColor = {0.16F, 0.24F, 0.31F},
            .dayNightTint = {0.76F, 0.84F, 1.0F},
            .sunIntensity = 0.68F,
            .ambientIntensity = 0.42F,
            .fogStart = 30.0F,
            .fogEnd = 96.0F,
            .exposure = 0.96F,
            .saturation = 0.95F,
            .nightFactor = 1.0F,
        },
    }};
}

void EnvironmentSystem::setAutomaticTime(float normalizedTime) {
    if (!frozen_ && !manualOverride_) {
        timeOfDay_ = wrapTime(normalizedTime);
    }
}

void EnvironmentSystem::toggleFrozen() {
    frozen_ = !frozen_;
}

void EnvironmentSystem::adjustTime(float amount) {
    manualOverride_ = true;
    timeOfDay_ = wrapTime(timeOfDay_ + amount);
}

void EnvironmentSystem::cycleProfile() {
    manualOverride_ = true;
    debugProfileIndex_ = (debugProfileIndex_ + 1) % 4;
    timeOfDay_ = static_cast<float>(debugProfileIndex_) * 0.25F;
}

void EnvironmentSystem::useAutomaticTime() {
    frozen_ = false;
    manualOverride_ = false;
}

EnvironmentState EnvironmentSystem::state() const {
    const float scaledTime = timeOfDay_ * 4.0F;
    const int fromIndex = static_cast<int>(std::floor(scaledTime)) % 4;
    const int toIndex = (fromIndex + 1) % 4;
    const float linearAmount =
        scaledTime - static_cast<float>(std::floor(scaledTime));
    const float smoothAmount =
        linearAmount * linearAmount * (3.0F - 2.0F * linearAmount);
    const EnvironmentProfile profile =
        lerpProfile(profiles_[static_cast<std::size_t>(fromIndex)],
                    profiles_[static_cast<std::size_t>(toIndex)],
                    smoothAmount);
    EnvironmentState result;
    static_cast<EnvironmentProfile&>(result) = profile;
    result.timeOfDay = timeOfDay_;
    return result;
}

float EnvironmentSystem::timeOfDay() const {
    return timeOfDay_;
}

bool EnvironmentSystem::frozen() const {
    return frozen_;
}

bool EnvironmentSystem::manualOverride() const {
    return manualOverride_;
}

const char* EnvironmentSystem::nearestProfileName() const {
    const int index =
        static_cast<int>(std::floor(timeOfDay_ * 4.0F + 0.5F)) % 4;
    return ProfileNames[static_cast<std::size_t>(index)];
}

EnvironmentLoadResult loadEnvironmentProfiles(std::string_view path) {
    std::ifstream stream{std::string(path)};
    if (!stream) {
        return {
            .profiles = EnvironmentSystem::defaults(),
            .errors = {"failed to open environment profiles: " +
                       std::string(path)},
        };
    }

    try {
        const Json root = Json::parse(stream);
        std::array<EnvironmentProfile, 4> profiles;
        const auto& source = root.at("profiles");
        for (std::size_t index = 0; index < ProfileNames.size(); ++index) {
            profiles[index] = parseProfile(source.at(ProfileNames[index]));
        }
        return {.profiles = profiles, .errors = {}};
    } catch (const std::exception& error) {
        return {
            .profiles = EnvironmentSystem::defaults(),
            .errors = {std::string("invalid environment profiles: ") +
                       error.what()},
        };
    }
}

} // namespace ian
