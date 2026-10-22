#pragma once

#include "ui/UiChamferedPlate.hpp"
#include "ui/UiRoundedRect.hpp"
#include "ui/UiText.hpp"

#include <algorithm>
#include <string_view>

namespace ian::menuStyle {

inline constexpr Color Cream{254, 247, 229, 255};
inline constexpr Color Gold{255, 204, 0, 255};
inline constexpr Color Green{87, 226, 74, 255};
inline constexpr Color Muted{174, 181, 180, 255};
inline constexpr Color Frame{44, 54, 47, 255};
inline constexpr Color Inset{28, 35, 30, 255};
inline constexpr Color AlternateRow{34, 42, 36, 255};
inline constexpr Color Card{70, 80, 73, 255}; // #465049: muted forest grey.
inline constexpr Color Hover{84, 95, 87, 255};
inline constexpr Color Selected{250, 193, 16, 255};
inline constexpr Color Disabled{42, 50, 44, 255};
inline constexpr int BevelContrast = 22;
inline constexpr int SelectedBevelContrast = 42;

inline int bevelContrast(Color fill) {
    const auto matches = [fill](Color color) {
        return fill.r == color.r && fill.g == color.g && fill.b == color.b;
    };
    return matches(Selected) || matches(Gold) ? SelectedBevelContrast : BevelContrast;
}

inline void panel(Rectangle bounds, float scale, Color fill = Frame, float opacity = 1.0F) {
    drawUiChamferedPlate(bounds, scale, fill, bevelContrast(fill), opacity);
}

inline Color selectionFill(bool selected, bool hovered, bool locked = false) {
    return selected ? Selected : hovered ? Hover : locked ? Disabled : Card;
}

// Menu labels share the home screen's outlined display/body faces. Preserve
// regular body text for descriptions and fit localized strings to their slots.
inline void text(std::string_view value, Rectangle bounds, float size, Color color = Cream,
                 UiFontRole role = UiFontRole::Body, int align = 0) {
    if (role == UiFontRole::BodyBold) role = UiFontRole::MenuBodyBold;
    const ScopedUiFontRole font(role);
    const float fitted = fitUiTextSize(value, size, size * 0.60F, bounds.width, bounds.height);
    const Vector2 measured = measureUiText(value, fitted);
    const float x = align == 1 ? bounds.x + (bounds.width - measured.x) * 0.5F
        : align == 2 ? bounds.x + bounds.width - measured.x : bounds.x;
    SdfUiTextStyle style{};
    style.fillColor = color;
    style.outlineColor = {18, 18, 15, 255};
    style.outlinePixels = fitted * (role == UiFontRole::Display ? 0.12F
        : role == UiFontRole::MenuBodyBold ? 0.09F : 0.04F);
    style.shadowColor = BLACK;
    style.shadowOffsetPixels = {0, fitted * 0.1F};
    style.softness = 0.6F;
    drawStyledSdfUiText(value,
        {x, bounds.y + (bounds.height - measured.y) * 0.5F}, fitted, style);
}

inline bool button(Rectangle bounds, float scale, std::string_view label, float fontSize,
                   Vector2 pointer, bool click, bool enabled = true) {
    const bool hovered = enabled && CheckCollisionPointRec(pointer, bounds);
    panel(bounds, scale, !enabled ? Disabled : hovered ? Selected : Card);
    text(label, {bounds.x + 8 * scale, bounds.y,
        bounds.width - 16 * scale, bounds.height}, fontSize * scale,
        enabled ? Cream : Muted, UiFontRole::Display, 1);
    return hovered && click;
}

inline bool closeButton(Rectangle bounds, float scale, Vector2 pointer, bool click) {
    return button(bounds, scale, "X", 20, pointer, click);
}

inline void progress(Rectangle bounds, double fraction, Color fill = Green) {
    drawSolidRoundedRect(bounds, 0.8F, BLACK);
    const float stroke = bounds.height * 0.15F;
    const Rectangle inner{bounds.x + stroke, bounds.y + stroke,
        bounds.width - stroke * 2, bounds.height - stroke * 2};
    drawSolidRoundedRect(inner, 0.8F, {174, 179, 173, 255});
    if (fraction > 0) drawSolidRoundedRect(
        {inner.x, inner.y, inner.width * static_cast<float>(std::clamp(fraction, 0.0, 1.0)), inner.height},
        0.8F, fill);
}

} // namespace ian::menuStyle
