#pragma once

#include <raylib.h>
#include <rlgl.h>

namespace ian {

// raylib's textured fan stretches the font atlas's white patch across every
// triangle. On long, thin triangles MSAA can sample outside that patch and
// bring glyph pixels into solid fills. A constant UV on the dedicated white
// texture keeps fills uniform while retaining antialiasing and alpha blending.
inline void drawSolidTriangleFan(const Vector2* points, int pointCount, Color color) {
    if (pointCount < 3 || color.a == 0) return;
    rlCheckRenderBatchLimit((pointCount - 2) * 3);
    rlSetTexture(rlGetTextureIdDefault());
    rlBegin(RL_TRIANGLES);
    rlNormal3f(0, 0, 1);
    rlColor4ub(color.r, color.g, color.b, color.a);
    rlTexCoord2f(0.5F, 0.5F);
    for (int index = 1; index < pointCount - 1; ++index) {
        rlVertex2f(points[0].x, points[0].y);
        rlVertex2f(points[index].x, points[index].y);
        rlVertex2f(points[index + 1].x, points[index + 1].y);
    }
    rlEnd();
    rlSetTexture(0);
}

} // namespace ian
