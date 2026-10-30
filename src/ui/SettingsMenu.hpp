#pragma once

#include "app/UserSettings.hpp"
#include <raylib.h>

#include <optional>
#include <string>

namespace ian {

enum class SettingsTab { Video, Audio, Controls, Game };

struct SettingsMenuState {
    SettingsTab tab{SettingsTab::Video};
    float scroll{};
    int focusedRow{-1};
    int draggingRow{-1};
    bool resetConfirmation{};
    bool progressResetConfirmation{};
    bool progressResetSelected{};
    std::string bindingError;
};

struct SettingsMenuInput {
    Vector2 pointer{};
    float wheel{};
    bool click{};
    bool pointerDown{};
    int tabStep{};
    int rowStep{};
    int valueStep{};
    bool activate{};
    bool back{};
    bool reset{};
    bool mainMenu{};
    std::optional<ControlAction> pendingBinding;
};

struct SettingsMenuResult {
    bool close{};
    bool chooseBackground{};
    bool credits{};
    bool resetProgress{};
    std::optional<ControlAction> rebind;
};

struct SettingsMenuLayout {
    Rectangle panel;
    Rectangle content;
    Rectangle tabs[4];
    Rectangle close;
    Rectangle reset;
};

[[nodiscard]] SettingsMenuLayout settingsMenuLayout(Vector2 screen);
[[nodiscard]] SettingsMenuResult drawSettingsMenu(
    UserSettings& settings, SettingsMenuState& state, Vector2 screen,
    const SettingsMenuInput& input);

// Empty on success. Ordinary controls cannot capture fixed UI/debug shortcuts
// or silently replace another gameplay binding. Backspace restores mouse input
// for Attack and the default key for the other actions.
[[nodiscard]] std::string rebindPlayerControl(
    ControlSettings& controls, ControlAction action, int key, Language language);

} // namespace ian
