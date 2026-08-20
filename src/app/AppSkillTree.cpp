#include "app/App.hpp"

#include <raylib.h>

namespace ian {

void App::setSkillTreeVisible(bool visible) {
    if (visible == skillTree_.isOpen()) {
        return;
    }
    if (visible) {
        const RunState state = simulation_.snapshot().state;
        skillTreePausedSimulation_ =
            state != RunState::MainMenu &&
            state != RunState::Paused &&
            state != RunState::Defeat;
        if (skillTreePausedSimulation_) {
            simulation_.togglePause();
            fixedStep_.reset();
        }
        enemySpawnMenuVisible_ = false;
        itemGrantMenuVisible_ = false;
        coreDefenseMenuVisible_ = false;
        skillTree_.open();
        EnableCursor();
        return;
    }

    skillTree_.close();
    if (skillTreePausedSimulation_ &&
        simulation_.snapshot().state == RunState::Paused) {
        simulation_.togglePause();
        fixedStep_.reset();
    }
    skillTreePausedSimulation_ = false;
    const RunState state = simulation_.snapshot().state;
    if (state == RunState::MainMenu || state == RunState::Paused) {
        EnableCursor();
    } else {
        DisableCursor();
    }
}

} // namespace ian
