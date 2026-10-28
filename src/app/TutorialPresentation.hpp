#pragma once

#include "game/Simulation.hpp"
#include "presentation/PresentationTypes.hpp"
#include "presentation/TutorialHint.hpp"

#include <optional>

namespace ian {

[[nodiscard]] inline std::optional<TutorialObjective>
presentedTutorialObjective(
    std::optional<TutorialObjective> objective, ActionMode actionMode,
    int runsPlayed = 1) {
    if (runsPlayed > 1) return std::nullopt;
    if (actionMode == ActionMode::Equipment && objective &&
        (*objective == TutorialObjective::PlaceCore ||
         *objective == TutorialObjective::PrepareForNight)) {
        return TutorialObjective::OpenBuildMode;
    }
    return objective;
}

[[nodiscard]] inline bool tutorialHintCompleted(
    std::optional<TutorialObjective> objective,
    const SimulationSnapshot& snapshot, ActionMode actionMode) {
    if (!objective) return false;
    switch (*objective) {
    case TutorialObjective::MineWood:
        return snapshot.wood >= snapshot.tutorialWoodTarget || snapshot.coreMaxHealth > 0;
    case TutorialObjective::OpenBuildMode:
        return actionMode == ActionMode::Buildings;
    case TutorialObjective::PlaceCore:
        return snapshot.coreMaxHealth > 0;
    case TutorialObjective::PrepareForNight:
        return std::ranges::any_of(snapshot.buildings, [](const BuildingInstance& building) {
            return buildingLimitCategory(building.type) == BuildingLimitCategory::Structure;
        });
    case TutorialObjective::SurviveFirstWave:
        return snapshot.wave > 1 || snapshot.state == RunState::WaveComplete;
    }
    return false;
}

} // namespace ian
