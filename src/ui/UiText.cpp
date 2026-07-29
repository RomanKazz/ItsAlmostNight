#include "ui/UiText.hpp"

#include <array>
#include <cstddef>
#include <string>

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
    const std::string owned{text};
    const float scaledSize = fontSize * UiTextScale;
    return MeasureTextEx(uiFont(), owned.c_str(), scaledSize,
                         scaledSize * 0.02F);
}

void drawUiText(std::string_view text, Vector2 position,
                float fontSize, Color color) {
    const std::string owned{text};
    const float scaledSize = fontSize * UiTextScale;
    DrawTextEx(uiFont(), owned.c_str(), position, scaledSize,
               scaledSize * 0.02F, color);
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
