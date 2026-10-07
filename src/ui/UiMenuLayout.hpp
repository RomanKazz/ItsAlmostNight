#pragma once

#include <raylib.h>

#include <algorithm>

namespace ian {

// Center fixed-size menus inside the current UI canvas. This is separate
// from UiViewport: HUD edge anchors still use the full aspect-ratio canvas.
struct UiMenuLayout {
    float scale;
    Vector2 origin;

    static UiMenuLayout fromViewport(Vector2 viewport) {
        const float scale = std::max(0.1F,
            std::min(viewport.x / 1920.0F, viewport.y / 1080.0F));
        return {scale, {(viewport.x - 1920.0F * scale) * 0.5F,
                        (viewport.y - 1080.0F * scale) * 0.5F}};
    }
    Rectangle rect(float x, float y, float width, float height) const {
        return {origin.x + x * scale, origin.y + y * scale,
                width * scale, height * scale};
    }
};

} // namespace ian
