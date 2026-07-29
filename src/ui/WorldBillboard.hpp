#pragma once

#include <raylib.h>

#include <string_view>

namespace ian {

void drawWorldBillboardQuad(
    Vector3 center, float width, float height, Color color,
    Vector3 cameraRight, Vector3 cameraUp);

[[nodiscard]] float measureWorldBillboardText(
    std::string_view text, float worldSize);

void drawWorldBillboardText(
    std::string_view text, Vector3 center, float worldSize,
    const Camera3D& camera, Vector3 cameraRight,
    Vector3 cameraUp, Color color,
    Color shadowColor = {24, 18, 14, 230});

void drawWorldBillboardTexture(
    Texture2D texture, Vector3 center, Vector2 worldSize,
    const Camera3D& camera, Vector3 cameraUp,
    Color tint = WHITE);

} // namespace ian
