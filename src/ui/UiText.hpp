#pragma once

#include <raylib.h>

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

namespace ian {

inline constexpr Color UiWhite{244, 240, 223, 255};

struct SdfUiTextStyle {
    Color fillColor{UiWhite};
    Color outlineColor{48, 50, 56, 255};
    Color shadowColor{0, 0, 0, 0};
    Vector2 shadowOffsetPixels{};
    float outlinePixels{2.0F};
    float softness{0.75F};
};

struct SdfUiTextRun {
    std::string_view text;
    Vector2 position{};
    float fontSize{};
    std::optional<Color> fillColor;
};

struct UiTextPerformanceStats {
    double milliseconds{};
    std::size_t submissions{};
    std::size_t characters{};
};

void beginUiTextPerformanceFrame() noexcept;
[[nodiscard]] UiTextPerformanceStats uiTextPerformanceStats() noexcept;
void beginSdfUiTextBatch();
void endSdfUiTextBatch();

void initializeUiText();
void shutdownUiText();
[[nodiscard]] Font uiFont();
[[nodiscard]] Vector2 measureUiText(std::string_view text,
                                    float fontSize);
[[nodiscard]] float fitUiTextSize(
    std::string_view text, float preferredSize,
    float minimumSize, float maximumWidth,
    float maximumHeight = 100000.0F);
void drawUiText(std::string_view text, Vector2 position,
                float fontSize, Color color);
void drawStyledSdfUiText(std::string_view text, Vector2 position,
                         float fontSize,
                         const SdfUiTextStyle& style);
void drawRotatedStyledUiText(
    std::string_view text, Vector2 center,
    float fontSize, float rotationDegrees,
    const SdfUiTextStyle& style);
void drawStyledSdfUiTextRuns(
    std::span<const SdfUiTextRun> runs,
    const SdfUiTextStyle& style);
void drawSdfUiText(std::string_view text, Vector2 position,
                   float fontSize, Color fillColor);
void drawCenteredUiText(std::string_view text, float y,
                        float fontSize, Color color);
void drawCenteredStyledSdfUiText(
    std::string_view text, float y, float fontSize,
    const SdfUiTextStyle& style);
void drawCenteredSdfUiText(std::string_view text, float y,
                           float fontSize, Color fillColor);

} // namespace ian
