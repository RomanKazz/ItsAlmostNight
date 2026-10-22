#pragma once

#include "game/Simulation.hpp"
#include "presentation/PresentationTypes.hpp"

#include <optional>

namespace ian {

[[nodiscard]] inline std::optional<TutorialObjective>
presentedTutorialObjective(
    std::optional<TutorialObjective> objective, ActionMode actionMode) {
    if (actionMode == ActionMode::Equipment && objective &&
        (*objective == TutorialObjective::PlaceCore ||
         *objective == TutorialObjective::PrepareForNight)) {
        return TutorialObjective::OpenBuildMode;
    }
    return objective;
}

} // namespace ian
