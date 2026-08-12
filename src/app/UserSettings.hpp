#pragma once

#include "audio/AudioSystem.hpp"
#include "graphics/GraphicsSettings.hpp"
#include "localization/Localization.hpp"

#include <raylib.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace ian {

struct MotionSettings {
    float bobIntensity{1.0F};
    float shakeIntensity{1.0F};
    float landingIntensity{1.0F};
    float swayIntensity{1.0F};

    bool operator==(const MotionSettings&) const = default;
};

enum class ControlAction : std::uint8_t {
    MoveForward,
    MoveLeft,
    MoveBackward,
    MoveRight,
    Jump,
    Sprint,
    Dash,
    Attack,
    ToggleTool,
    Interact,
    Bomb,
    Repair,
    Copy,
    Upgrade,
    Sell,
    UpgradeWeapon,
    BuildMode,
    Pause,
    Skills,
    Map,
    StartWave,
    Restart,
    RevealChest,
    Count,
};

inline constexpr std::size_t ControlActionCount =
    static_cast<std::size_t>(ControlAction::Count);

inline constexpr std::array<int, ControlActionCount>
    DefaultControlKeys{
        KEY_W,          KEY_A,       KEY_S,       KEY_D,
        KEY_SPACE,      KEY_LEFT_SHIFT, KEY_NULL,
        KEY_NULL,       KEY_C,       KEY_E,       KEY_G,
        KEY_F,          KEY_Q,       KEY_U,       KEY_X,
        KEY_V,          KEY_TAB,     KEY_P,       KEY_K,
        KEY_M,          KEY_N,       KEY_R,       KEY_L,
    };

struct ControlSettings {
    float mouseSensitivity{1.0F};
    bool invertMouseY{};
    std::array<int, ControlActionCount> keys{DefaultControlKeys};

    bool operator==(const ControlSettings&) const = default;
};

struct AccessibilitySettings {
    bool showFps{false};
    bool reduceFlashes{};

    bool operator==(const AccessibilitySettings&) const = default;
};

struct UserSettings {
    GraphicsSettings graphics;
    AudioSettings audio;
    MotionSettings motion;
    ControlSettings controls;
    AccessibilitySettings accessibility;
    Language language{Language::English};

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

[[nodiscard]] int controlKey(
    const ControlSettings& settings, ControlAction action);
void setControlKey(
    ControlSettings& settings, ControlAction action, int key);
[[nodiscard]] int defaultControlKey(ControlAction action);
[[nodiscard]] const char* controlActionName(ControlAction action);
[[nodiscard]] std::string keyboardKeyName(int key);

} // namespace ian
