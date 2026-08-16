#include "ui/UiText.hpp"
#include "ui/UiCString.hpp"
#include "localization/Localization.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include <string_view>

namespace ian {
namespace {

Font primaryFont{};
Font cyrillicFallbackFont{};
bool initialized{};

constexpr int AsciiFirst = 32;
constexpr int AsciiLast = 126;
constexpr int CyrillicFirst = 0x0400;
constexpr int CyrillicLast = 0x04ff;
constexpr std::array<int, 5> ExtraUiCodepoints{
    0x00b7, // middle dot: ·
    0x2014, // em dash: —
    0x2022, // bullet: •
    0x221e, // infinity: ∞
    0x2713, // check mark: ✓
};
constexpr int GlyphCount =
    (AsciiLast - AsciiFirst + 1) +
    (CyrillicLast - CyrillicFirst + 1) +
    static_cast<int>(ExtraUiCodepoints.size());
constexpr int LatinGlyphCount =
    (AsciiLast - AsciiFirst + 1) +
    static_cast<int>(ExtraUiCodepoints.size());
constexpr float UiTextScale = 1.68F;
constexpr float TwoPi = 6.28318530717958647692F;

void configureFontFiltering(Font& loadedFont) {
    if (!IsFontValid(loadedFont) ||
        !IsTextureValid(loadedFont.texture)) {
        return;
    }
    SetTextureFilter(
        loadedFont.texture, TEXTURE_FILTER_BILINEAR);
}

bool isStandaloneInfinity(std::string_view text) {
    return text == "∞";
}

bool isStandaloneCheck(std::string_view text) {
    return text == "✓";
}

float specialGlyphAdvance(float scaledSize) {
    return scaledSize * 0.9F;
}

void drawInfinityGlyph(Vector2 position, float scaledSize, Color color) {
    constexpr int Segments = 24;
    const Vector2 center{
        position.x + scaledSize * 0.45F,
        position.y + scaledSize * 0.51F};
    const float radiusX = scaledSize * 0.40F;
    const float radiusY = scaledSize * 0.18F;
    const float thickness = std::max(1.5F, scaledSize * 0.085F);
    std::array<Vector2, Segments + 1> points{};
    for (int index = 0; index <= Segments; ++index) {
        const float angle = TwoPi * static_cast<float>(index) /
                            static_cast<float>(Segments);
        points[static_cast<std::size_t>(index)] = {
            center.x + std::sin(angle) * radiusX,
            center.y + std::sin(angle) * std::cos(angle) * radiusY};
    }
    for (int index = 0; index < Segments; ++index) {
        DrawLineEx(
            points[static_cast<std::size_t>(index)],
            points[static_cast<std::size_t>(index + 1)],
            thickness, color);
    }
}

void drawCheckGlyph(Vector2 position, float scaledSize, Color color) {
    const float thickness = std::max(1.5F, scaledSize * 0.10F);
    const Vector2 start{
        position.x + scaledSize * 0.12F,
        position.y + scaledSize * 0.52F};
    const Vector2 middle{
        position.x + scaledSize * 0.37F,
        position.y + scaledSize * 0.75F};
    const Vector2 end{
        position.x + scaledSize * 0.84F,
        position.y + scaledSize * 0.22F};
    DrawLineEx(start, middle, thickness, color);
    DrawLineEx(middle, end, thickness, color);
}

bool drawStandaloneSpecialGlyph(
    std::string_view text, Vector2 position, float scaledSize,
    Color color) {
    if (isStandaloneInfinity(text)) {
        drawInfinityGlyph(position, scaledSize, color);
        return true;
    }
    if (isStandaloneCheck(text)) {
        drawCheckGlyph(position, scaledSize, color);
        return true;
    }
    return false;
}

} // namespace

void initializeUiText() {
    if (initialized) {
        return;
    }
    std::array<int, GlyphCount> codepoints{};
    int index = 0;
    for (int codepoint = AsciiFirst; codepoint <= AsciiLast;
         ++codepoint) {
        codepoints[static_cast<std::size_t>(index++)] = codepoint;
    }
    for (int codepoint = CyrillicFirst;
         codepoint <= CyrillicLast; ++codepoint) {
        codepoints[static_cast<std::size_t>(index++)] = codepoint;
    }
    for (const int codepoint : ExtraUiCodepoints) {
        codepoints[static_cast<std::size_t>(index++)] = codepoint;
    }
    std::array<int, LatinGlyphCount> latinCodepoints{};
    index = 0;
    for (int codepoint = AsciiFirst; codepoint <= AsciiLast;
         ++codepoint) {
        latinCodepoints[static_cast<std::size_t>(index++)] = codepoint;
    }
    for (const int codepoint : ExtraUiCodepoints) {
        latinCodepoints[static_cast<std::size_t>(index++)] = codepoint;
    }
    primaryFont = LoadFontEx(
        "assets/ui/LilitaOne-Regular.ttf", 96,
        latinCodepoints.data(), LatinGlyphCount);
    cyrillicFallbackFont = LoadFontEx(
        "assets/ui/FredokaOneCyrillic-Regular.ttf", 64,
        codepoints.data(), GlyphCount);
    configureFontFiltering(primaryFont);
    configureFontFiltering(cyrillicFallbackFont);
    initialized = IsFontValid(primaryFont) ||
        IsFontValid(cyrillicFallbackFont);
}

void shutdownUiText() {
    if (IsFontValid(primaryFont)) UnloadFont(primaryFont);
    if (IsFontValid(cyrillicFallbackFont)) {
        UnloadFont(cyrillicFallbackFont);
    }
    primaryFont = {};
    cyrillicFallbackFont = {};
    initialized = false;
}

Font uiFont() {
    if (currentLanguage() == Language::English &&
        IsFontValid(primaryFont)) {
        return primaryFont;
    }
    if (IsFontValid(cyrillicFallbackFont)) {
        return cyrillicFallbackFont;
    }
    return IsFontValid(primaryFont) ? primaryFont : GetFontDefault();
}

Vector2 measureUiText(std::string_view text, float fontSize) {
    const float scaledSize = fontSize * UiTextScale;
    if (isStandaloneInfinity(text) || isStandaloneCheck(text)) {
        return {specialGlyphAdvance(scaledSize), scaledSize};
    }
    if (currentLanguage() == Language::English) {
        return withNullTerminatedUiText(
            text, [scaledSize](const char* value) {
                return MeasureTextEx(
                    uiFont(), value, scaledSize,
                    scaledSize * 0.02F);
            });
    }
    const std::string localized = localizeText(text);
    return withNullTerminatedUiText(
        localized, [scaledSize](const char* value) {
            return MeasureTextEx(
                uiFont(), value, scaledSize,
                scaledSize * 0.02F);
        });
}

float fitUiTextSize(
    std::string_view text, float preferredSize,
    float minimumSize, float maximumWidth,
    float maximumHeight) {
    float size = std::max(preferredSize, minimumSize);
    const float safeWidth = std::max(maximumWidth, 1.0F);
    const float safeHeight = std::max(maximumHeight, 1.0F);
    while (size > minimumSize) {
        const Vector2 measured = measureUiText(text, size);
        if (measured.x <= safeWidth && measured.y <= safeHeight) {
            break;
        }
        size = std::max(minimumSize, size - 0.5F);
    }
    return size;
}

void drawUiText(std::string_view text, Vector2 position,
                float fontSize, Color color) {
    const float scaledSize = fontSize * UiTextScale;
    const Color shadow{
        7, 9, 11,
        static_cast<unsigned char>(
            static_cast<unsigned int>(color.a) * 3U / 4U),
    };
    const auto draw = [position, scaledSize, shadow, color](
                          const char* value) {
        if (drawStandaloneSpecialGlyph(
                value, {position.x + 1.5F, position.y + 2.0F},
                scaledSize, shadow)) {
            drawStandaloneSpecialGlyph(value, position, scaledSize, color);
            return;
        }
        DrawTextEx(
            uiFont(), value,
            {position.x + 1.5F, position.y + 2.0F},
            scaledSize, scaledSize * 0.02F, shadow);
        DrawTextEx(
            uiFont(), value, position, scaledSize,
            scaledSize * 0.02F, color);
    };
    if (currentLanguage() == Language::English) {
        withNullTerminatedUiText(text, draw);
    } else {
        const std::string localized = localizeText(text);
        withNullTerminatedUiText(localized, draw);
    }
}

void drawCenteredUiText(std::string_view text, float y,
                        float fontSize, Color color) {
    const Vector2 size = measureUiText(text, fontSize);
    drawUiText(
        text,
        {(static_cast<float>(GetScreenWidth()) - size.x) * 0.5F,
         y},
        fontSize, color);
}

} // namespace ian
