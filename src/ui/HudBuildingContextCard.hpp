#pragma once

#include "app/UserSettings.hpp"
#include "game/Simulation.hpp"
#include "ui/HudRenderer.hpp"

#include <raylib.h>

#include <string>

namespace ian::hud_detail {

[[nodiscard]] std::string actionKeyLabel(
    const ControlSettings& controls, ControlAction action);

void drawBuildingContextCard(
    GameUi& ui, const SimulationSnapshot& snapshot,
    const BuildingInstance& building, const HudViewState& view,
    const Camera3D& camera, const ControlSettings& controls);

void drawModularBuildingContextCard(
    const SimulationSnapshot& snapshot, const Camera3D& camera,
    float entranceSeconds);

} // namespace ian::hud_detail
