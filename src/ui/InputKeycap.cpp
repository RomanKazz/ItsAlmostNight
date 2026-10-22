#include "ui/InputKeycap.hpp"

#include "ui/GameUi.hpp"
#include "ui/UiHintStyle.hpp"

#include <algorithm>

namespace ian {

std::string InputKeycap::label(
    const ControlSettings& controls, ControlAction action) {
    const int key = controlKey(controls, action);
    if (action == ControlAction::Attack && key == KEY_NULL) {
        return "LMB";
    }
    return keyboardKeyName(key);
}

bool InputKeycap::held(
    const ControlSettings& controls, ControlAction action) {
    if (action == ControlAction::Attack &&
        controlKey(controls, action) == KEY_NULL) {
        return IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    }
    const int key = controlKey(controls, action);
    return key != KEY_NULL && IsKeyDown(key);
}

Vector2 InputKeycap::size(std::string_view value, float height) {
    const float safeHeight = std::max(height, 28.0F);
    const ScopedUiFontRole role(UiFontRole::Display);
    return {std::max(safeHeight,measureUiText(value,safeHeight*.5F).x+safeHeight*.4F),safeHeight};
}

void InputKeycap::draw(const GameUi& ui, Rectangle bounds,
                       std::string_view value, bool pressed,
                       unsigned char alpha) {
    static_cast<void>(ui);
    hintStyle::key(bounds,value,pressed,static_cast<float>(alpha)/255);
}

} // namespace ian
