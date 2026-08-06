#pragma once

#include "audio/AudioSystem.hpp"
#include "graphics/GraphicsSettings.hpp"

#include <optional>
#include <string_view>

namespace ian {

struct MotionSettings {
    float bobIntensity{1.0F};
    float shakeIntensity{1.0F};
    float landingIntensity{1.0F};
    float swayIntensity{1.0F};

    bool operator==(const MotionSettings&) const = default;
};

struct ControlSettings {
    float mouseSensitivity{1.0F};
    bool invertMouseY{};

    bool operator==(const ControlSettings&) const = default;
};

struct AccessibilitySettings {
    bool showFps{true};
    bool reduceFlashes{};

    bool operator==(const AccessibilitySettings&) const = default;
};

struct UserSettings {
    GraphicsSettings graphics;
    AudioSettings audio;
    MotionSettings motion;
    ControlSettings controls;
    AccessibilitySettings accessibility;

    bool operator==(const UserSettings&) const = default;
};

[[nodiscard]] bool loadUserSettings(
    std::string_view path, UserSettings& settings);
[[nodiscard]] bool saveUserSettings(
    std::string_view path, const UserSettings& settings);

void resetDisplaySettings(GraphicsSettings& settings);
void resetColorSettings(GraphicsSettings& settings);
void resetStyleSettings(GraphicsSettings& settings);
void applyGraphicsPreset(
    GraphicsSettings& settings, GraphicsQuality preset);
[[nodiscard]] std::optional<GraphicsQuality>
detectGraphicsPreset(const GraphicsSettings& settings);

} // namespace ian
