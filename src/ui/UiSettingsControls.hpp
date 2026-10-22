#pragma once

#include "ui/UiMenuStyle.hpp"
#include "ui/UiViewport.hpp"

namespace ian {

// Settings use the same plates and typography as the menu collection screens.
// Keep the existing release-to-activate buttons and hover/drag slider input.
class SettingsMenuUi {
public:
    void drawPanel(Rectangle bounds, unsigned char = 255) const {
        menuStyle::panel(bounds, 1, menuStyle::Frame);
    }
    void drawInsetPanel(Rectangle bounds, unsigned char = 255) const {
        menuStyle::panel(bounds, 1, menuStyle::Inset);
    }
    void drawLabel(Rectangle bounds, std::string_view label, int align = 0) const {
        menuStyle::text(label, bounds, 18, menuStyle::Cream, UiFontRole::MenuBodyBold, align);
    }
    bool drawButton(Rectangle bounds, std::string_view label) const {
        return drawToggleButton(bounds, label, false);
    }
    bool drawToggleButton(Rectangle bounds, std::string_view label, bool active) const {
        const bool hovered = CheckCollisionPointRec(uiMousePosition(), bounds);
        menuStyle::panel(bounds, 1, menuStyle::selectionFill(active, hovered));
        menuStyle::text(label, {bounds.x + 12, bounds.y, bounds.width - 24, bounds.height},
            18, menuStyle::Cream, UiFontRole::Display, 1);
        return hovered && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    }
    bool drawKeyCap(Rectangle bounds, std::string_view label, bool waiting = false) const {
        const bool hovered = CheckCollisionPointRec(uiMousePosition(), bounds);
        menuStyle::panel(bounds, 1, menuStyle::selectionFill(waiting, hovered));
        menuStyle::text(label, {bounds.x + 4, bounds.y, bounds.width - 8, bounds.height},
            14, menuStyle::Cream, UiFontRole::MenuBodyBold, 1);
        return hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    }
    float drawSliderBar(Rectangle bounds, float value, float minimum, float maximum) const {
        if (maximum < minimum) std::swap(minimum, maximum);
        value = std::clamp(value, minimum, maximum);
        const float range = maximum - minimum;
        const Vector2 pointer = uiMousePosition();
        const bool hovered = CheckCollisionPointRec(pointer,
            {bounds.x, bounds.y - 8, bounds.width, bounds.height + 16});
        if (hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT) && bounds.width > 0 && range > 0) {
            value = minimum + range * std::clamp((pointer.x - bounds.x) / bounds.width, 0.0F, 1.0F);
        }
        const float fraction = range > 0 ? (value - minimum) / range : 0;
        menuStyle::progress(bounds, fraction, menuStyle::Gold);
        const float height = std::max(26.0F, bounds.height * 1.45F);
        const float width = 16;
        menuStyle::panel({bounds.x + bounds.width * fraction - width * 0.5F,
            bounds.y + (bounds.height - height) * 0.5F, width, height}, 1,
            hovered ? menuStyle::Selected : menuStyle::Cream);
        return value;
    }
};

inline void drawSettingsText(std::string_view value, Vector2 position, float size, Color color) {
    const ScopedUiFontRole font(UiFontRole::MenuBodyBold);
    const Vector2 measured = measureUiText(value, size);
    menuStyle::text(value, {position.x, position.y, measured.x, measured.y}, size,
        color, UiFontRole::MenuBodyBold);
}

} // namespace ian
