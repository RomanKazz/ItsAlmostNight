#pragma once

#include "ui/UiSolidGeometry.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace ian {

inline void drawSolidRoundedRect(
    Rectangle bounds, float roundness, Color color) {
    if (bounds.width <= 0.0F || bounds.height <= 0.0F ||
        color.a == 0) {
        return;
    }
    const float radius = std::min(bounds.width, bounds.height) *
        std::clamp(roundness, 0.0F, 1.0F) * 0.5F;
    if (radius <= 0.0F) {
        const std::array<Vector2, 4> points{{
            {bounds.x, bounds.y}, {bounds.x, bounds.y + bounds.height},
            {bounds.x + bounds.width, bounds.y + bounds.height},
            {bounds.x + bounds.width, bounds.y},
        }};
        drawSolidTriangleFan(points.data(), static_cast<int>(points.size()), color);
        return;
    }

    // A single convex fan keeps animated translucency uniform across the
    // pill. Overlapping rectangles and circles darken where they intersect.
    constexpr int SegmentsPerCorner = 8;
    constexpr float Pi = 3.14159265358979323846F;
    std::array<Vector2, 2 + 4 * (SegmentsPerCorner + 1)> points{};
    points[0] = {bounds.x + bounds.width * 0.5F,
                 bounds.y + bounds.height * 0.5F};
    std::size_t count = 1;
    const auto appendCorner = [&](float centerX, float centerY,
                                  float firstAngle) {
        for (int step = 0; step <= SegmentsPerCorner; ++step) {
            const float angle = firstAngle -
                (Pi * 0.5F * static_cast<float>(step)) /
                    static_cast<float>(SegmentsPerCorner);
            points[count++] = {
                centerX + std::cos(angle) * radius,
                centerY + std::sin(angle) * radius};
        }
    };
    appendCorner(bounds.x + radius,
                 bounds.y + radius, -Pi * 0.5F);
    appendCorner(bounds.x + radius,
                 bounds.y + bounds.height - radius, Pi);
    appendCorner(bounds.x + bounds.width - radius,
                 bounds.y + bounds.height - radius, Pi * 0.5F);
    appendCorner(bounds.x + bounds.width - radius,
                 bounds.y + radius, 0.0F);
    points[count] = points[1];
    drawSolidTriangleFan(points.data(), static_cast<int>(count + 1), color);
}

} // namespace ian
