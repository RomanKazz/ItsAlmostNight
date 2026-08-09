#include "ui/InputKeycap.hpp"

#include "ui/GameUi.hpp"

#include <algorithm>

namespace ian {

std::string InputKeycap::label(
    const ControlSettings& controls, ControlAction action) {
    const int key = controlKey(controls, action);
    if (action == ControlAction::Attack && key == KEY_NULL) {
        return "LMB";
    }
    if (action == ControlAction::Dash && key == KEY_NULL) {
        return "RMB";
    }
    return keyboardKeyName(key);
}

bool InputKeycap::held(
    const ControlSettings& controls, ControlAction action) {
    if (action == ControlAction::Attack &&
        controlKey(controls, action) == KEY_NULL) {
        return IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    }
    if (action == ControlAction::Dash &&
        controlKey(controls, action) == KEY_NULL) {
        return IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    }
    const int key = controlKey(controls, action);
    return key != KEY_NULL && IsKeyDown(key);
}

Vector2 InputKeycap::size(std::string_view value, float height) {
    const float safeHeight = std::max(height, 28.0F);
    return value == "LMB" || value == "RMB"
               ? Vector2{std::max(44.0F, safeHeight * 1.25F), safeHeight}
               : Vector2{safeHeight, safeHeight};
}

void InputKeycap::draw(const GameUi& ui, Rectangle bounds,
                       std::string_view value, bool pressed,
                       unsigned char alpha) {
    static_cast<void>(ui.drawKeyCap(bounds, value, pressed, alpha));
}

} // namespace ian
