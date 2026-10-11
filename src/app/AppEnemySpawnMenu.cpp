#include "app/App.hpp"
#include "ui/EnemySpawnMenu.hpp"
#include "ui/UiViewport.hpp"

#include <algorithm>

namespace ian {

void App::drawEnemySpawnMenu() {
    const ScopedUiCanvas canvas;
    if (!enemySpawnMenuVisible_) return;

    const auto& snapshot = simulation_.snapshot();
    const int active = static_cast<int>(std::ranges::count_if(snapshot.enemies,
        [](const EnemyInstance& enemy) { return enemy.active; }));
    const EnemySpawnMenuInput input{
        .pointer = uiMousePosition(),
        .click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
        .down = IsMouseButtonDown(MOUSE_BUTTON_LEFT),
        .enter = IsKeyPressed(KEY_ENTER),
        .fastStep = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT),
        .wheel = GetMouseWheelMove(),
    };
    const auto result = drawEnemySpawnCatalog(debugSpawnType_, debugSpawnEliteAffix_,
        debugSpawnCount_, active,
        {static_cast<float>(uiScreenWidth()), static_cast<float>(uiScreenHeight())}, input);
    if (result.summon) {
        pendingSpawnEnemy_ = true;
        audio_.playUiConfirm();
    }
    if (result.close) {
        enemySpawnMenuVisible_ = false;
        audio_.playUiConfirm();
        if (snapshot.state != RunState::Paused) DisableCursor();
    }
}

} // namespace ian
