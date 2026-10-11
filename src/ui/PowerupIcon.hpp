#pragma once

#include "economy/PowerupSystem.hpp"
#include <raylib.h>

namespace ian {
// Shared geometric emblems, equally readable in world space and in HUD.
template<class Triangle> void drawPowerupIconGeometry(PowerupType type, Triangle triangle) {
    const auto quad = [&](Vector2 a, Vector2 b, Vector2 c, Vector2 d) {
        triangle(a,b,c); triangle(a,c,d);
    };
    if (type == PowerupType::Fury) {
        triangle({.12F,-.9F},{-.65F,.12F},{.05F,.02F});
        triangle({-.05F,-.08F},{.62F,-.12F},{-.15F,.95F});
    } else if (type == PowerupType::GoldRush) {
        // Three-point cartoon crown.
        quad({-.8F,-.35F},{-.4F,.12F},{0,-.65F},{.4F,.12F});
        triangle({0,-.65F},{.4F,.12F},{.8F,-.35F});
        quad({-.65F,.1F},{.65F,.1F},{.55F,.65F},{-.55F,.65F});
    } else {
        // Horseshoe magnet, broad pole caps.
        quad({-.75F,-.55F},{-.3F,-.55F},{-.3F,.5F},{-.75F,.5F});
        quad({.3F,-.55F},{.75F,-.55F},{.75F,.5F},{.3F,.5F});
        quad({-.75F,.25F},{.75F,.25F},{.6F,.75F},{-.6F,.75F});
    }
}
} // namespace ian
