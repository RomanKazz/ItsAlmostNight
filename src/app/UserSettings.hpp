#pragma once

#include "audio/AudioSystem.hpp"
#include "graphics/GraphicsSettings.hpp"

#include <string_view>

namespace ian {

struct MotionSettings {
    float bobIntensity{1.0F};
    float shakeIntensity{1.0F};
    float landingIntensity{1.0F};
    float swayIntensity{1.0F};

    bool operator==(const MotionSettings&) const = default;
};

struct UserSettings {
    GraphicsSettings graphics;
    AudioSettings audio;
    MotionSettings motion;

    bool operator==(const UserSettings&) const = default;
};

[[nodiscard]] bool loadUserSettings(
    std::string_view path, UserSettings& settings);
[[nodiscard]] bool saveUserSettings(
    std::string_view path, const UserSettings& settings);

void resetDisplaySettings(GraphicsSettings& settings);
void resetColorSettings(GraphicsSettings& settings);
void resetStyleSettings(GraphicsSettings& settings);

} // namespace ian
