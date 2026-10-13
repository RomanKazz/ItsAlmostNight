#pragma once
#include "ui/UiItemTooltip.hpp"

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
    const std::size_t count = static_cast<std::size_t>(runTomeCount(snapshot.runUpgradeStacks));
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
        const int level = runTomeLevel(hammer.effect, snapshot.runUpgradeStacks);
        if (level <= 0 || retiredRunUpgrade(hammer.effect)) continue;
        const Rectangle cell{bounds.x + static_cast<float>(position % columns) * cellSize,
            bounds.y + static_cast<float>(position / columns) * cellSize,
            cellSize - 3.0F, cellSize - 3.0F};
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
            bounds.y + static_cast<float>(position / columns) * cellSize, cellSize - 3.0F, cellSize - 3.0F};
        DrawRectangleRounded(cell, .18F, 4, fade({24, 29, 27, 120}));
        const float fontSize = cellSize * .40F;
        const Vector2 plusSize = measureUiText("+", fontSize);
        drawUiText("+", {cell.x + (cell.width - plusSize.x) * .5F,
            cell.y + (cell.height - plusSize.y) * .5F},
            fontSize, fade({190, 202, 197, 115}));
    }
    if (hovered) drawItemTooltip(hovered->name,
        "LVL "+std::to_string(runTomeLevel(hovered->effect,snapshot.runUpgradeStacks))+" · "+
            localizeText(runUpgradeDefinition(hovered->effect).description),
        mouse,{static_cast<float>(uiScreenWidth()),static_cast<float>(uiScreenHeight())},.85F,opacity);
}

} // namespace ian
