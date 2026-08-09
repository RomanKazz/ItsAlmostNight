#include "app/App.hpp"

#include "ui/UiText.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <string>

namespace ian {
namespace {

[[nodiscard]] const char* enemyTypeName(EnemyType type) {
    switch (type) {
    case EnemyType::Basic:
        return "BASIC";
    case EnemyType::Fast:
        return "FAST";
    case EnemyType::Heavy:
        return "HEAVY";
    case EnemyType::Boss:
        return "BOSS";
    case EnemyType::Ranged:
        return "RANGED";
    case EnemyType::Sapper:
        return "SAPPER";
    case EnemyType::Flying:
        return "FLYING";
    case EnemyType::Splitter:
        return "SPLITTER";
    case EnemyType::Splitling:
        return "SPLITLING";
    }
    return "BASIC";
}

[[nodiscard]] EnemyType adjacentEnemyType(
    EnemyType type, int direction) {
    constexpr int TypeCount = 9;
    int index = static_cast<int>(type);
    index = (index + direction + TypeCount) % TypeCount;
    return static_cast<EnemyType>(index);
}

} // namespace

void App::drawEnemySpawnMenu() {
    if (!enemySpawnMenuVisible_) {
        return;
    }

    constexpr float Width = 620.0F;
    constexpr float Height = 510.0F;
    constexpr float Padding = 34.0F;
    constexpr float ButtonHeight = 66.0F;
    const float x =
        (static_cast<float>(GetScreenWidth()) - Width) * 0.5F;
    const float y =
        (static_cast<float>(GetScreenHeight()) - Height) * 0.5F;
    const float contentWidth = Width - Padding * 2.0F;

    ui_.drawPanel({x, y, Width, Height}, 250);
    ui_.drawInsetPanel(
        {x + Padding, y + 28.0F, contentWidth, 72.0F}, 245);
    ui_.drawLabel(
        {x + Padding, y + 35.0F, contentWidth, 56.0F},
        "SUMMON ENEMIES", 1);

    drawUiText(
        "ENEMY TYPE", {x + Padding, y + 126.0F},
        17.0F, {245, 220, 174, 255});
    constexpr float ArrowWidth = 76.0F;
    const float typeY = y + 164.0F;
    if (ui_.drawButton(
            {x + Padding, typeY, ArrowWidth, ButtonHeight},
            "<")) {
        debugSpawnType_ =
            adjacentEnemyType(debugSpawnType_, -1);
    }
    ui_.drawInsetPanel(
        {x + Padding + ArrowWidth + 12.0F, typeY,
         contentWidth - ArrowWidth * 2.0F - 24.0F,
         ButtonHeight},
        240);
    ui_.drawLabel(
        {x + Padding + ArrowWidth + 12.0F, typeY,
         contentWidth - ArrowWidth * 2.0F - 24.0F,
         ButtonHeight},
        enemyTypeName(debugSpawnType_), 1);
    if (ui_.drawButton(
            {x + Width - Padding - ArrowWidth, typeY,
             ArrowWidth, ButtonHeight},
            ">")) {
        debugSpawnType_ =
            adjacentEnemyType(debugSpawnType_, 1);
    }

    drawUiText(
        TextFormat("COUNT: %d", debugSpawnCount_),
        {x + Padding, y + 258.0F}, 18.0F,
        {245, 220, 174, 255});
    ui_.drawInsetPanel(
        {x + Padding, y + 300.0F, contentWidth, 48.0F}, 235);
    const float selectedCount = ui_.drawSliderBar(
        {x + Padding + 12.0F, y + 310.0F,
         contentWidth - 24.0F, 28.0F},
        static_cast<float>(debugSpawnCount_), 1.0F, 1000.0F);
    debugSpawnCount_ = std::clamp(
        static_cast<int>(std::lround(selectedCount)), 1, 1000);

    const float actionY = y + 382.0F;
    if (ui_.drawButton(
            {x + Padding, actionY,
             contentWidth * 0.66F - 7.0F, ButtonHeight},
            "SUMMON")) {
        pendingSpawnEnemy_ = true;
        audio_.playUiConfirm();
    }
    if (ui_.drawButton(
            {x + Padding + contentWidth * 0.66F + 7.0F,
             actionY, contentWidth * 0.34F - 7.0F,
             ButtonHeight},
            "CLOSE [B]")) {
        enemySpawnMenuVisible_ = false;
        if (simulation_.snapshot().state != RunState::Paused) {
            DisableCursor();
        }
    }
}

} // namespace ian
