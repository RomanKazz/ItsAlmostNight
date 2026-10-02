#pragma once

#include "game/Simulation.hpp"
#include "ui/UiText.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>

namespace ian {

// Shared card/HUD glyph: type color identifies the stat; rarity stays on the card.
inline void drawRunHammerIcon(Rectangle bounds, const RunHammerDefinition& hammer, float opacity = 1.0F) {
    const float size = std::min(bounds.width, bounds.height);
    const float x = bounds.x + (bounds.width - size) * 0.5F;
    const float y = bounds.y + (bounds.height - size) * 0.5F;
    const auto color = [opacity](Color value) {
        value.a = static_cast<unsigned char>(std::lround(value.a * std::clamp(opacity, 0.0F, 1.0F)));
        return value;
    };
    const auto rect = [&](float rx, float ry, float rw, float rh, Color fill) {
        DrawRectangleRounded({x + rx * size, y + ry * size, rw * size, rh * size}, 0.18F, 4, color(fill));
    };
    const Color accent{static_cast<unsigned char>(hammer.colorRgb >> 16U),
        static_cast<unsigned char>(hammer.colorRgb >> 8U), static_cast<unsigned char>(hammer.colorRgb), 255};
    rect(0.405F, 0.31F, 0.19F, 0.62F, {23, 22, 22, 255});
    rect(0.435F, 0.34F, 0.13F, 0.56F, {146, 82, 44, 255});
    rect(0.44F, 0.36F, 0.045F, 0.49F, {226, 165, 100, 255});
    rect(0.105F, 0.12F, 0.79F, 0.36F, {23, 22, 22, 255});
    rect(0.135F, 0.15F, 0.73F, 0.30F, accent);
    rect(0.15F, 0.165F, 0.70F, 0.065F, {255, 247, 220, 125});
    rect(0.15F, 0.365F, 0.70F, 0.065F, {20, 25, 36, 100});
    rect(0.39F, 0.64F, 0.22F, 0.13F, {23, 22, 22, 255});
    rect(0.415F, 0.665F, 0.17F, 0.08F, {232, 191, 130, 255});
    // Marks disappear at HUD sizes; larger card glyphs retain a readable symbol.
    if (size >= 70.0F) {
        const ScopedUiFontRole role(UiFontRole::BodyBold);
        // These short glyph marks stay neutral across languages, like a stat symbol.
        const std::string mark{hammer.mark};
        const Font font = uiFont();
        float fontSize = size * 0.12F;
        auto measured = MeasureTextEx(font, mark.c_str(), fontSize, fontSize * 0.02F);
        if (measured.x > size * 0.59F) {
            fontSize *= size * 0.59F / measured.x;
            measured = MeasureTextEx(font, mark.c_str(), fontSize, fontSize * 0.02F);
        }
        DrawTextEx(font, mark.c_str(), {x + size * 0.5F - measured.x * 0.5F,
            y + size * 0.30F - measured.y * 0.5F}, fontSize, fontSize * 0.02F,
            color({25, 27, 27, 255}));
    }
}

inline void drawRunHammerInventory(const SimulationSnapshot& snapshot, Rectangle bounds,
    float opacity = 1.0F, std::optional<Vector2> pointer = std::nullopt) {
    std::size_t count = 0;
    for (const auto& hammer : RunHammerDefinitions)
        if (snapshot.runUpgradeStacks[runUpgradeIndex(hammer.effect)] > 0) ++count;
    if (count == 0) return;
    // Keep level badges legible as the inventory grows; additional items wrap.
    const std::size_t columns = std::min(count, std::size_t{13});
    const float cellSize = std::min(bounds.height, bounds.width / static_cast<float>(columns));
    const Vector2 mouse = pointer.value_or(GetMousePosition());
    const RunHammerDefinition* hovered = nullptr;
    std::size_t position = 0;
    const auto fade = [opacity](Color color) {
        color.a = static_cast<unsigned char>(std::lround(color.a * std::clamp(opacity, 0.0F, 1.0F)));
        return color;
    };
    for (const auto& hammer : RunHammerDefinitions) {
        const int level = snapshot.runUpgradeStacks[runUpgradeIndex(hammer.effect)];
        if (level <= 0) continue;
        const Rectangle cell{bounds.x + static_cast<float>(position % columns) * cellSize,
            bounds.y + static_cast<float>(position / columns) * (cellSize + 3.0F),
            cellSize - 3.0F, cellSize};
        ++position;
        DrawRectangleRounded(cell, 0.18F, 4, fade({24, 29, 27, 160}));
        drawRunHammerIcon({cell.x + 2.0F, cell.y + 1.0F, cell.width - 4.0F, cell.height - 6.0F}, hammer, opacity);
        const std::string levelText = std::to_string(level);
        const float fontSize = cellSize * 0.19F;
        const auto measured = measureUiText(levelText, fontSize);
        drawStyledSdfUiText(levelText, {cell.x + cell.width - measured.x - 2.0F,
            cell.y + cell.height - measured.y - 1.0F}, fontSize,
            {.fillColor = fade(UiWhite), .outlineColor = fade({20, 22, 20, 255}), .outlinePixels = 1.5F});
        if ((pointer || !IsCursorHidden()) && CheckCollisionPointRec(mouse, cell)) hovered = &hammer;
    }
    if (hovered) {
        const std::string level = "LVL " + std::to_string(snapshot.runUpgradeStacks[runUpgradeIndex(hovered->effect)]);
        const float width = std::min(300.0F, static_cast<float>(GetScreenWidth()) - 24.0F);
        const float left = std::clamp(mouse.x, 12.0F, static_cast<float>(GetScreenWidth()) - width - 12.0F);
        const Rectangle tooltip{left, bounds.y + static_cast<float>((count + columns - 1) / columns) * (cellSize + 3.0F) + 6.0F, width, 62.0F};
        DrawRectangleRounded(tooltip, 0.12F, 4, fade({24, 29, 27, 245}));
        const float fontSize = fitUiTextSize(hovered->name, 17.0F, 11.0F, width - 20.0F);
        drawUiText(hovered->name, {left + 10.0F, tooltip.y + 8.0F}, fontSize, fade(UiWhite));
        const ScopedUiFontRole body(UiFontRole::Body);
        drawUiText(level, {left + 10.0F, tooltip.y + 36.0F}, 14.0F, fade({197, 207, 190, 255}));
    }
}

} // namespace ian
