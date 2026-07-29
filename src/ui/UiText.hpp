#pragma once

#include <raylib.h>

#include <string_view>

namespace ian {

void initializeUiText();
void shutdownUiText();
[[nodiscard]] Font uiFont();
[[nodiscard]] Vector2 measureUiText(std::string_view text,
                                    float fontSize);
void drawUiText(std::string_view text, Vector2 position,
                float fontSize, Color color);
void drawCenteredUiText(std::string_view text, float y,
                        float fontSize, Color color);

} // namespace ian
