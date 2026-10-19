#include "ui/WorldBillboard.hpp"

#include "ui/UiText.hpp"

#include <raymath.h>

#include <algorithm>
#include <array>
#include <cmath>

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

void drawWorldBillboardRoundedRectangle(
    Vector3 center, float width, float height, float radius,
    Color color, Vector3 cameraRight, Vector3 cameraUp) {
    if (width <= 0.0F || height <= 0.0F) {
        return;
    }
    radius = std::clamp(
        radius, 0.0F, std::min(width, height) * 0.5F);
    if (radius <= 0.0001F) {
        drawWorldBillboardQuad(
            center, width, height, color, cameraRight, cameraUp);
        return;
    }

    constexpr int CornerSegments = 5;
    constexpr int PointCount = CornerSegments * 4;
    std::array<Vector3, PointCount> perimeter{};
    const float halfWidth = width * 0.5F;
    const float halfHeight = height * 0.5F;
    constexpr std::array<Vector2, 4> CornerCenters{{
        {-1.0F, -1.0F}, {1.0F, -1.0F},
        {1.0F, 1.0F}, {-1.0F, 1.0F},
    }};
    constexpr std::array<float, 4> StartAngles{{
        PI, PI * 1.5F, 0.0F, PI * 0.5F,
    }};
    std::size_t pointIndex = 0;
    for (std::size_t corner = 0; corner < 4; ++corner) {
        const float cornerX = CornerCenters[corner].x *
            (halfWidth - radius);
        const float cornerY = CornerCenters[corner].y *
            (halfHeight - radius);
        for (int segment = 0; segment < CornerSegments; ++segment) {
            const float angle = StartAngles[corner] +
                static_cast<float>(segment) *
                    (PI * 0.5F) /
                    static_cast<float>(CornerSegments - 1);
            const float x = cornerX + std::cos(angle) * radius;
            const float y = cornerY + std::sin(angle) * radius;
            perimeter[pointIndex++] = Vector3Add(
                Vector3Add(
                    center, Vector3Scale(cameraRight, x)),
                Vector3Scale(cameraUp, y));
        }
    }
    for (std::size_t index = 0; index < perimeter.size(); ++index) {
        DrawTriangle3D(
            center,
            perimeter[index],
            perimeter[(index + 1U) % perimeter.size()],
            color);
    }
}

float measureWorldBillboardText(
    std::string_view text, float worldSize) {
    const Font font = uiFont();
    const float scale =
        worldSize / static_cast<float>(font.baseSize);
    float totalWidth = 0.0F;
    for (std::size_t offset = 0; offset < text.size();) {
        int byteCount = 0;
        const int codepoint = GetCodepointNext(text.data() + offset, &byteCount);
        offset += static_cast<std::size_t>(std::max(byteCount, 1));
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
    for (std::size_t offset = 0; offset < text.size();) {
        int byteCount = 0;
        const int codepoint = GetCodepointNext(text.data() + offset, &byteCount);
        offset += static_cast<std::size_t>(std::max(byteCount, 1));
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

void drawWorldBillboardOutlinedText(
    std::string_view text, Vector3 center, float worldSize,
    const Camera3D& camera, Vector3 cameraRight,
    Vector3 cameraUp, Color color, Color outlineColor,
    Color shadowColor, float rotationDegrees) {
    const Font font = uiFont();
    const float scale =
        worldSize / static_cast<float>(font.baseSize);
    // Rotate the text's entire camera-facing plane around the word center.
    // Rotating each glyph in place leaves the baseline horizontal.
    const float rotation = rotationDegrees * DEG2RAD;
    const float cosine = std::cos(rotation);
    const float sine = std::sin(rotation);
    const Vector3 textRight = Vector3Add(
        Vector3Scale(cameraRight, cosine),
        Vector3Scale(cameraUp, sine));
    const Vector3 textUp = Vector3Subtract(
        Vector3Scale(cameraUp, cosine),
        Vector3Scale(cameraRight, sine));
    const float outlineOffset = worldSize * 0.047F;
    const float shadowOffset = worldSize * 0.065F;
    constexpr Vector2 OutlineDirections[]{
        {-1.0F, 0.0F}, {1.0F, 0.0F},
        {0.0F, -1.0F}, {0.0F, 1.0F},
        {-0.7F, -0.7F}, {0.7F, -0.7F},
        {-0.7F, 0.7F}, {0.7F, 0.7F},
    };
    float cursor =
        -measureWorldBillboardText(text, worldSize) * 0.5F;
    for (std::size_t offset = 0; offset < text.size();) {
        int byteCount = 0;
        const int codepoint = GetCodepointNext(text.data() + offset, &byteCount);
        offset += static_cast<std::size_t>(std::max(byteCount, 1));
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
        const Vector3 glyphCenter = Vector3Add(
            Vector3Add(
                center,
                Vector3Scale(
                    textRight,
                    cursor + static_cast<float>(glyph.offsetX) * scale +
                        glyphSize.x * 0.5F)),
            Vector3Scale(
                textUp,
                (static_cast<float>(font.baseSize) * 0.5F -
                 (static_cast<float>(glyph.offsetY) +
                  source.height * 0.5F)) * scale));
        const Vector2 glyphOrigin{
            glyphSize.x * 0.5F, glyphSize.y * 0.5F};
        const Vector3 shadowCenter = Vector3Add(
            Vector3Add(
                glyphCenter,
                Vector3Scale(textRight, shadowOffset)),
            Vector3Scale(textUp, -shadowOffset));
        DrawBillboardPro(
            camera, font.texture, source, shadowCenter,
            cameraUp, glyphSize, glyphOrigin,
            rotationDegrees, shadowColor);
        for (const Vector2 direction : OutlineDirections) {
            const Vector3 outlineCenter = Vector3Add(
                Vector3Add(
                    glyphCenter,
                    Vector3Scale(
                        textRight,
                        direction.x * outlineOffset)),
                Vector3Scale(
                    textUp,
                    direction.y * outlineOffset));
            DrawBillboardPro(
                camera, font.texture, source, outlineCenter,
                cameraUp, glyphSize, glyphOrigin,
                rotationDegrees, outlineColor);
        }
        DrawBillboardPro(
            camera, font.texture, source, glyphCenter,
            cameraUp, glyphSize, glyphOrigin,
            rotationDegrees, color);
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
