#pragma once

#include "ui/UiSolidGeometry.hpp"

#include <algorithm>
#include <array>

namespace ian {

inline void drawChamferedFill(Rectangle bounds, float cut, Color color) {
    if (bounds.width <= 0 || bounds.height <= 0) return;
    cut = std::clamp(cut, 0.0F, std::min(bounds.width, bounds.height) * 0.5F);
    const float right = bounds.x + bounds.width;
    const float bottom = bounds.y + bounds.height;
    std::array<Vector2, 10> points{{
        {bounds.x + bounds.width * 0.5F, bounds.y + bounds.height * 0.5F},
        {bounds.x + cut, bounds.y}, {bounds.x, bounds.y + cut},
        {bounds.x, bottom - cut}, {bounds.x + cut, bottom},
        {right - cut, bottom}, {right, bottom - cut},
        {right, bounds.y + cut}, {right - cut, bounds.y},
        {bounds.x + cut, bounds.y},
    }};
    drawSolidTriangleFan(points.data(), static_cast<int>(points.size()), color);
}

// Dimensions change only the straight spans. Stroke, 45-degree chamfers and
// the inner highlight depend on uniform UI scale, never on plate aspect ratio.
inline void drawUiChamferedPlate(Rectangle bounds, float scale, Color fill,
                                int bevelContrast = 42, float opacity = 1.0F) {
    opacity = std::clamp(opacity, 0.0F, 1.0F);
    const auto faded = [opacity](Color color) {
        color.a = static_cast<unsigned char>(static_cast<float>(color.a) * opacity + 0.5F);
        return color;
    };
    const float stroke = 5.0F * scale;
    const float cut = 18.0F * scale;
    drawChamferedFill(bounds, cut, faded(BLACK));
    const Rectangle inner{bounds.x + stroke, bounds.y + stroke,
        bounds.width - 2 * stroke, bounds.height - 2 * stroke};
    // The diagonal stroke has the same perpendicular thickness as the edges.
    const float innerCut = cut - stroke * (2.0F - 1.41421356237F);
    const Color highlight{
        static_cast<unsigned char>(std::clamp(static_cast<int>(fill.r) + bevelContrast, 0, 255)),
        static_cast<unsigned char>(std::clamp(static_cast<int>(fill.g) + bevelContrast, 0, 255)),
        static_cast<unsigned char>(std::clamp(static_cast<int>(fill.b) + bevelContrast, 0, 255)), fill.a};
    drawChamferedFill(inner, innerCut, faded(highlight));
    const float bevel = 3.0F * scale;
    drawChamferedFill({inner.x, inner.y + bevel, inner.width, inner.height - bevel},
        innerCut, faded(fill));
}

} // namespace ian
