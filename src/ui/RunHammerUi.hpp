#pragma once

#include "ui/UiViewport.hpp"

#include "game/Simulation.hpp"
#include "ui/UiText.hpp"
#include "ui/GameUi.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>

namespace ian {

// HUD and offers resolve the same asset, rather than drawing a second icon family.
inline void drawRunHammerIcon(const GameUi& ui, Rectangle bounds,
    const RunTomeDefinition& hammer, float opacity = 1.0F) {
    const Texture2D texture = ui.runUpgradeTexture(hammer.effect);
    if (!IsTextureValid(texture)) return;
    const float size = std::min(bounds.width, bounds.height);
    DrawTexturePro(texture, {0, 0, static_cast<float>(texture.width), static_cast<float>(texture.height)},
        {bounds.x + (bounds.width - size) * 0.5F, bounds.y + (bounds.height - size) * 0.5F, size, size},
        {}, 0, {255, 255, 255, static_cast<unsigned char>(255 * std::clamp(opacity, 0.0F, 1.0F))});
}

inline void drawRunHammerInventory(const GameUi& ui, const SimulationSnapshot& snapshot, Rectangle bounds,
    float opacity = 1.0F, std::optional<Vector2> pointer = std::nullopt) {
    std::size_t count = 0;
    for (const auto& hammer : RunTomeDefinitions)
        if (snapshot.runUpgradeStacks[runUpgradeIndex(hammer.effect)] > 0) ++count;
    const std::size_t slots = std::max(count, static_cast<std::size_t>(snapshot.tomeSlotLimit));
    // Keep level badges legible as the inventory grows; additional items wrap.
    const std::size_t columns = std::min(slots, std::size_t{13});
    const float cellSize = std::min(bounds.height, bounds.width / static_cast<float>(columns));
    const Vector2 mouse = pointer.value_or(uiMousePosition());
    const RunTomeDefinition* hovered = nullptr;
    std::size_t position = 0;
    const auto fade = [opacity](Color color) {
        color.a = static_cast<unsigned char>(std::lround(color.a * std::clamp(opacity, 0.0F, 1.0F)));
        return color;
    };
    for (const auto& hammer : RunTomeDefinitions) {
        const int level = snapshot.runUpgradeStacks[runUpgradeIndex(hammer.effect)];
        if (level <= 0) continue;
        const Rectangle cell{bounds.x + static_cast<float>(position % columns) * cellSize,
            bounds.y + static_cast<float>(position / columns) * (cellSize + 3.0F),
            cellSize - 3.0F, cellSize};
        ++position;
        DrawRectangleRounded(cell, 0.18F, 4, fade({24, 29, 27, 160}));
        drawRunHammerIcon(ui, {cell.x + 2.0F, cell.y + 1.0F, cell.width - 4.0F, cell.height - 6.0F}, hammer, opacity);
        const std::string levelText = std::to_string(level);
        const float fontSize = cellSize * 0.19F;
        const auto measured = measureUiText(levelText, fontSize);
        drawStyledSdfUiText(levelText, {cell.x + cell.width - measured.x - 2.0F,
            cell.y + cell.height - measured.y - 1.0F}, fontSize,
            {.fillColor = fade(UiWhite), .outlineColor = fade({20, 22, 20, 255}), .outlinePixels = 1.5F});
        if ((pointer || !IsCursorHidden()) && CheckCollisionPointRec(mouse, cell)) hovered = &hammer;
    }
    for (; position < slots; ++position) {
        const Rectangle cell{bounds.x + static_cast<float>(position % columns) * cellSize,
            bounds.y + static_cast<float>(position / columns) * (cellSize + 3.0F), cellSize - 3.0F, cellSize};
        DrawRectangleRounded(cell, .18F, 4, fade({24, 29, 27, 120}));
        drawUiText("+", {cell.x + cell.width * .35F, cell.y + cell.height * .24F},
            cellSize * .40F, fade({190, 202, 197, 115}));
    }
    const std::string capacity = std::to_string(count) + "/" + std::to_string(snapshot.tomeSlotLimit);
    drawStyledSdfUiText(capacity, {bounds.x + static_cast<float>(columns) * cellSize + 4, bounds.y + 12},
        16, {.fillColor = fade(UiWhite), .outlineColor = fade({20, 22, 20, 255}), .outlinePixels = 1.5F});
    if (hovered) {
        const std::string level = "LVL " + std::to_string(snapshot.runUpgradeStacks[runUpgradeIndex(hovered->effect)]);
        const float width = std::min(300.0F, static_cast<float>(uiScreenWidth()) - 24.0F);
        const float left = std::clamp(mouse.x, 12.0F, static_cast<float>(uiScreenWidth()) - width - 12.0F);
        const Rectangle tooltip{left, bounds.y + static_cast<float>((count + columns - 1) / columns) * (cellSize + 3.0F) + 6.0F, width, 62.0F};
        DrawRectangleRounded(tooltip, 0.12F, 4, fade({24, 29, 27, 245}));
        const float fontSize = fitUiTextSize(hovered->name, 17.0F, 11.0F, width - 20.0F);
        drawUiText(hovered->name, {left + 10.0F, tooltip.y + 8.0F}, fontSize, fade(UiWhite));
        const ScopedUiFontRole body(UiFontRole::Body);
        drawUiText(level, {left + 10.0F, tooltip.y + 36.0F}, 14.0F, fade({197, 207, 190, 255}));
    }
}

} // namespace ian
