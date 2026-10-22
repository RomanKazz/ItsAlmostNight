#pragma once
#include "ui/UiItemTooltip.hpp"

#include "ui/RunHammerUi.hpp"
#include "localization/Localization.hpp"

namespace ian {

inline void drawRunDefenseInventory(const GameUi& ui, const SimulationSnapshot& snapshot,
    Rectangle bounds, float opacity = 1.0F, std::optional<Vector2> pointer = std::nullopt) {
    const std::size_t slots = std::max(static_cast<std::size_t>(std::max(1, snapshot.buildingSlotLimit)), snapshot.buildingArsenalCount);
    const float pitch = std::min(bounds.height, bounds.width / static_cast<float>(slots));
    const auto fade = [opacity](Color color) {
        color.a = static_cast<unsigned char>(std::lround(color.a * std::clamp(opacity, 0.0F, 1.0F)));
        return color;
    };
    const Vector2 mouse = pointer.value_or(uiMousePosition());
    std::optional<RunUpgradeEffect> hovered;
    std::size_t position = 0;
    const auto* character = playerClassDefinition(snapshot.playerClass);
    const auto starter = character ? character->startingBuilding : BuildingType::GunTurret;
    const auto drawType = [&](BuildingType type) {
        const auto effect = *buildingRunUpgrade(type);
        const int level = snapshot.runUpgradeStacks[runUpgradeIndex(effect)];
        if (level <= 0) return;
        const Rectangle cell{bounds.x + static_cast<float>(position++) * pitch, bounds.y, pitch - 3, pitch - 3};
        DrawRectangleRounded(cell, .18F, 4, fade({24, 29, 27, 160}));
        const auto texture = ui.buildingTexture(type);
        if (IsTextureValid(texture)) DrawTexturePro(texture,
            {0, 0, static_cast<float>(texture.width), static_cast<float>(texture.height)},
            {cell.x + 2, cell.y + 1, cell.width - 4, cell.height - 6}, {}, 0, fade(WHITE));
        const std::string badge = std::to_string(level);
        const float size = pitch * .19F;
        const auto measured = measureUiText(badge, size);
        drawStyledSdfUiText(badge, {cell.x + cell.width - measured.x - 2,
            cell.y + cell.height - measured.y - 1}, size,
            {.fillColor = fade(UiWhite), .outlineColor = fade({20, 22, 20, 255}), .outlinePixels = 1.5F});
        if ((pointer || !IsCursorHidden()) && CheckCollisionPointRec(mouse, cell)) hovered = effect;
    };
    drawType(starter);
    for (const auto type : RunBuildingTypes)
        if (type != starter && type != BuildingType::Wall && runCombatBuilding(type)) drawType(type);
    for (; position < slots; ++position) {
        const Rectangle cell{bounds.x + static_cast<float>(position) * pitch, bounds.y, pitch - 3, pitch - 3};
        DrawRectangleRounded(cell, .18F, 4, fade({24, 29, 27, 120}));
        const float size = pitch * .40F;
        const auto measured = measureUiText("+", size);
        drawUiText("+", {cell.x + (cell.width - measured.x) * .5F,
            cell.y + (cell.height - measured.y) * .5F}, size, fade({190, 202, 197, 115}));
    }
    if (hovered) drawItemTooltip(runUpgradeDefinition(*hovered).name,
        "LVL "+std::to_string(snapshot.runUpgradeStacks[runUpgradeIndex(*hovered)])+" · "+
            localizeText(runUpgradeDefinition(*hovered).description),
        mouse,{static_cast<float>(uiScreenWidth()),static_cast<float>(uiScreenHeight())},.85F,opacity);
}

} // namespace ian
