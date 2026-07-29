#include "ui/WorldBillboard.hpp"

#include "ui/UiText.hpp"

#include <raymath.h>

namespace ian {
namespace {

constexpr float WorldSpacing = 0.018F;

} // namespace

void drawWorldBillboardQuad(
    Vector3 center, float width, float height, Color color,
    Vector3 cameraRight, Vector3 cameraUp) {
    const Vector3 horizontal =
        Vector3Scale(cameraRight, width * 0.5F);
    const Vector3 vertical =
        Vector3Scale(cameraUp, height * 0.5F);
    const Vector3 bottomLeft = Vector3Subtract(
        Vector3Subtract(center, horizontal), vertical);
    const Vector3 bottomRight = Vector3Add(
        Vector3Subtract(center, vertical), horizontal);
    const Vector3 topRight = Vector3Add(
        Vector3Add(center, horizontal), vertical);
    const Vector3 topLeft = Vector3Add(
        Vector3Subtract(center, horizontal), vertical);
    DrawTriangle3D(bottomLeft, bottomRight, topRight, color);
    DrawTriangle3D(bottomLeft, topRight, topLeft, color);
}

float measureWorldBillboardText(
    std::string_view text, float worldSize) {
    const Font font = uiFont();
    const float scale =
        worldSize / static_cast<float>(font.baseSize);
    float totalWidth = 0.0F;
    for (const char character : text) {
        const int codepoint =
            static_cast<unsigned char>(character);
        const GlyphInfo glyph = GetGlyphInfo(font, codepoint);
        const Rectangle source =
            GetGlyphAtlasRec(font, codepoint);
        const float advance = static_cast<float>(
            glyph.advanceX > 0
                ? glyph.advanceX
                : static_cast<int>(source.width));
        totalWidth += advance * scale + WorldSpacing;
    }
    if (!text.empty()) {
        totalWidth -= WorldSpacing;
    }
    return totalWidth;
}

void drawWorldBillboardText(
    std::string_view text, Vector3 center, float worldSize,
    const Camera3D& camera, Vector3 cameraRight,
    Vector3 cameraUp, Color color, Color shadowColor) {
    const Font font = uiFont();
    const float scale =
        worldSize / static_cast<float>(font.baseSize);
    float cursor =
        -measureWorldBillboardText(text, worldSize) * 0.5F;
    for (const char character : text) {
        const int codepoint =
            static_cast<unsigned char>(character);
        const GlyphInfo glyph = GetGlyphInfo(font, codepoint);
        const Rectangle source =
            GetGlyphAtlasRec(font, codepoint);
        const float advance = static_cast<float>(
            glyph.advanceX > 0
                ? glyph.advanceX
                : static_cast<int>(source.width));
        const Vector2 glyphSize{
            source.width * scale,
            source.height * scale,
        };
        const float glyphCenterX =
            cursor + static_cast<float>(glyph.offsetX) * scale +
            glyphSize.x * 0.5F;
        const float glyphCenterY =
            (static_cast<float>(font.baseSize) * 0.5F -
             (static_cast<float>(glyph.offsetY) +
              source.height * 0.5F)) *
            scale;
        const Vector3 glyphCenter = Vector3Add(
            Vector3Add(
                center,
                Vector3Scale(cameraRight, glyphCenterX)),
            Vector3Scale(cameraUp, glyphCenterY));
        const Vector3 shadowCenter = Vector3Add(
            Vector3Subtract(
                glyphCenter, Vector3Scale(cameraUp, 0.018F)),
            Vector3Scale(cameraRight, 0.014F));
        DrawBillboardPro(
            camera, font.texture, source, shadowCenter,
            cameraUp, glyphSize,
            {glyphSize.x * 0.5F, glyphSize.y * 0.5F},
            0.0F, shadowColor);
        DrawBillboardPro(
            camera, font.texture, source, glyphCenter,
            cameraUp, glyphSize,
            {glyphSize.x * 0.5F, glyphSize.y * 0.5F},
            0.0F, color);
        cursor += advance * scale + WorldSpacing;
    }
}

void drawWorldBillboardTexture(
    Texture2D texture, Vector3 center, Vector2 worldSize,
    const Camera3D& camera, Vector3 cameraUp, Color tint) {
    if (!IsTextureValid(texture)) {
        return;
    }
    DrawBillboardPro(
        camera, texture,
        {0.0F, 0.0F, static_cast<float>(texture.width),
         static_cast<float>(texture.height)},
        center, cameraUp, worldSize,
        {worldSize.x * 0.5F, worldSize.y * 0.5F},
        0.0F, tint);
}

} // namespace ian
