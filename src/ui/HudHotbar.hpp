#pragma once

#include "ui/HudRenderer.hpp"

namespace ian::hud_detail {

void drawBuildHotbar(
    GameUi& ui, const SimulationSnapshot& snapshot,
    const HudViewState& view, float transitionOffset = 0.0F);
void drawLootInventory(
    GameUi& ui, const SimulationSnapshot& snapshot,
    bool expanded, bool coreVisible);
void drawClassAbilityStatus(const SimulationSnapshot& snapshot);
void drawCompactInsight(
    GameUi& ui,
    const SimulationSnapshot& snapshot,
    const HudViewState& view);
void drawWeaponHotbar(
    GameUi& ui, const SimulationSnapshot& snapshot,
    const HudViewState& view, float transitionOffset = 0.0F);

} // namespace ian::hud_detail
