#pragma once

#include "ui/UiMenuStyle.hpp"

#include <cstddef>
#include <string>

namespace ian {

// Shared by the home button and category tabs; claimed rewards disappear.
inline void drawUiNotificationBadge(Vector2 center, float radius, std::size_t count) {
    if (count == 0) return;
    DrawCircleV({center.x, center.y + radius * 0.16F}, radius, ColorAlpha(BLACK, 0.5F));
    DrawCircleV(center, radius, BLACK);
    DrawCircleV(center, radius * 0.80F, {244, 102, 84, 255});
    const std::string label = count > 99 ? "99+" : std::to_string(count);
    menuStyle::text(label,
        {center.x - radius * 0.72F, center.y - radius * 0.67F, radius * 1.44F, radius * 1.34F},
        radius * 1.16F, menuStyle::Cream, UiFontRole::Display, 1);
}

} // namespace ian
