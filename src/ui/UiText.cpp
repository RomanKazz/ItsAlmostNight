#include "ui/UiText.hpp"
#include "ui/UiCString.hpp"

#include <array>
#include <cstddef>

namespace ian {
namespace {

Font font{};
bool initialized{};

constexpr int AsciiFirst = 32;
constexpr int AsciiLast = 126;
constexpr int CyrillicFirst = 0x0400;
constexpr int CyrillicLast = 0x04ff;
constexpr int GlyphCount =
    (AsciiLast - AsciiFirst + 1) +
    (CyrillicLast - CyrillicFirst + 1);
constexpr float UiTextScale = 1.68F;

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
    font = LoadFontEx(
        "assets/ui/FredokaOneCyrillic-Regular.ttf", 64,
        codepoints.data(), GlyphCount);
    initialized = IsFontValid(font);
}

void shutdownUiText() {
    if (initialized) {
        UnloadFont(font);
    }
    font = {};
    initialized = false;
}

Font uiFont() {
    return initialized ? font : GetFontDefault();
}

Vector2 measureUiText(std::string_view text, float fontSize) {
    const float scaledSize = fontSize * UiTextScale;
    return withNullTerminatedUiText(
        text, [scaledSize](const char* value) {
            return MeasureTextEx(
                uiFont(), value, scaledSize,
                scaledSize * 0.02F);
        });
}

void drawUiText(std::string_view text, Vector2 position,
                float fontSize, Color color) {
    const float scaledSize = fontSize * UiTextScale;
    const Color shadow{
        7, 9, 11,
        static_cast<unsigned char>(
            static_cast<unsigned int>(color.a) * 3U / 4U),
    };
    withNullTerminatedUiText(
        text, [position, scaledSize, shadow, color](
                  const char* value) {
            DrawTextEx(
                uiFont(), value,
                {position.x + 1.5F, position.y + 2.0F},
                scaledSize, scaledSize * 0.02F, shadow);
            DrawTextEx(
                uiFont(), value, position, scaledSize,
                scaledSize * 0.02F, color);
        });
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
