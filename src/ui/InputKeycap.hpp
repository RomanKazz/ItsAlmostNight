#pragma once

#include "app/UserSettings.hpp"

#include <raylib.h>

#include <string>
#include <string_view>

namespace ian {

class GameUi;

class InputKeycap {
  public:
    [[nodiscard]] static std::string label(
        const ControlSettings& controls, ControlAction action);
    [[nodiscard]] static bool held(
        const ControlSettings& controls, ControlAction action);
    [[nodiscard]] static Vector2 size(std::string_view label,
                                      float height = 36.0F);
    static void draw(const GameUi& ui, Rectangle bounds,
                     std::string_view label, bool pressed = false,
                     unsigned char alpha = 255);
};

} // namespace ian
