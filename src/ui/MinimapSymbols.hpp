#pragma once

#include "ui/MinimapMarkers.hpp"
#include <raylib.h>
#include <rlgl.h>

namespace ian {
// A shared cartographic alphabet: resources are small marks, destinations badges.
inline Color minimapSymbolColor(MapIcon icon) {
    switch (icon) {
    case MapIcon::Wood: return {170, 207, 130, 255};
    case MapIcon::Stone: return {190, 202, 204, 255};
    case MapIcon::Crystal: return {211, 146, 239, 255};
    case MapIcon::Chest: return {255, 209, 101, 255};
    case MapIcon::Supplies: return {220, 173, 116, 255};
    case MapIcon::Challenge: case MapIcon::Boss: return {255, 130, 104, 255};
    case MapIcon::Base: return {111, 215, 232, 255};
    case MapIcon::PlayerShrine: return {243, 159, 197, 255};
    case MapIcon::BuildingShrine: return {111, 215, 232, 255};
    case MapIcon::Industry: return {224, 190, 136, 255};
    default: return {222, 223, 194, 255};
    }
}
inline void drawMinimapSymbol(MapIcon icon, Vector2 center, float size,
                             bool badge = false, float rotation = 0,
                             bool highlighted = false) {
    const Color color = minimapSymbolColor(icon);
    if (badge) {
        DrawCircleV({center.x, center.y + size * .06F}, size * .54F, {12, 19, 17, 160});
        DrawCircleV(center, size * .54F, highlighted ? Color{255, 209, 101, 255} : Color{14, 23, 21, 255});
        DrawCircleV(center, size * .45F, {43, 56, 47, 255});
    }
    rlPushMatrix();
    rlTranslatef(center.x, center.y, 0);
    rlRotatef(rotation, 0, 0, 1);
    rlScalef(size, size, 1);
    const auto line = [&](Vector2 a, Vector2 b) { DrawLineEx(a, b, .085F, color); };
    constexpr Color inset{43, 56, 47, 255};
    switch (icon) {
    case MapIcon::Wood:
        DrawTriangle({0, -.38F}, {-.31F, .20F}, {.31F, .20F}, color);
        DrawRectangleRec({-.05F, .18F, .10F, .18F}, color);
        break;
    case MapIcon::Stone: case MapIcon::Boulders:
        DrawPoly({0, 0}, 5, .34F, -18, color);
        break;
    case MapIcon::Crystal:
        DrawTriangle({0, -.38F}, {-.27F, -.05F}, {0, .36F}, color);
        DrawTriangle({0, -.38F}, {0, .36F}, {.27F, -.05F}, color);
        break;
    case MapIcon::Chest:
        DrawRectangleRounded({-.32F, -.23F, .64F, .48F}, .18F, 3, color);
        DrawLineEx({-.29F, -.02F}, {.29F, -.02F}, .055F, inset);
        DrawRectangleRec({-.055F, -.08F, .11F, .18F}, color);
        break;
    case MapIcon::Supplies:
        DrawRectangleRounded({-.24F, -.33F, .48F, .66F}, .3F, 3, color);
        DrawLineEx({-.23F, -.16F}, {.23F, -.16F}, .045F, inset);
        DrawLineEx({-.23F, .16F}, {.23F, .16F}, .045F, inset);
        break;
    case MapIcon::Base:
        line({-.27F, -.02F}, {0, -.29F}); line({0, -.29F}, {.27F, -.02F});
        line({-.20F, -.02F}, {-.20F, .25F}); line({.20F, -.02F}, {.20F, .25F});
        line({-.20F, .25F}, {.20F, .25F}); line({0, .25F}, {0, .08F});
        break;
    case MapIcon::Boss:
        DrawCircleV({0, -.07F}, .27F, color);
        DrawRectangleRec({-.16F, .06F, .32F, .21F}, color);
        DrawCircleV({-.10F, -.07F}, .065F, inset);
        DrawCircleV({.10F, -.07F}, .065F, inset);
        break;
    case MapIcon::Challenge:
        DrawTriangle({.12F, -.34F}, {-.27F, .07F}, {.06F, .07F}, color);
        DrawTriangle({-.12F, .34F}, {.27F, -.07F}, {-.06F, -.07F}, color);
        break;
    case MapIcon::PlayerShrine: case MapIcon::BuildingShrine:
        DrawPoly({0, 0}, 4, .34F, 0, color);
        DrawCircleV({0, 0}, .115F, inset);
        break;
    case MapIcon::Industry:
        line({-.23F, .28F}, {.22F, -.21F});
        line({-.18F, -.29F}, {.27F, -.14F});
        break;
    default:
        line({-.27F, .25F}, {-.27F, -.23F});
        line({.27F, .25F}, {.27F, -.23F});
        line({-.27F, -.15F}, {.27F, -.15F});
        line({-.27F, .25F}, {-.10F, .25F});
        line({.27F, .25F}, {.10F, .25F});
        break;
    }
    rlPopMatrix();
}
} // namespace ian
