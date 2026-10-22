#include "ui/UiViewport.hpp"
#include "ui/HudRenderer.hpp"

#include "game/Simulation.hpp"
#include "ui/UiText.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace ian {
void drawRunStateOverlay(const SimulationSnapshot& snapshot) {
    // Finished runs use the interactive summary menu in RunMenus.
    if (snapshot.state == RunState::Defeat || snapshot.state == RunState::Victory) return;
    const ScopedUiCanvas canvas;
    if (snapshot.state == RunState::Paused) {
        DrawRectangle(0, 0, uiScreenWidth(), uiScreenHeight(),
                      {0, 0, 0, 150});
        drawCenteredSdfUiText(
            "PAUSED", static_cast<float>(uiScreenHeight() / 2 - 24),
            48.0F, UiWhite);
    } else if (snapshot.state == RunState::StageClear) {
        DrawRectangle(0, 0, uiScreenWidth(), uiScreenHeight(),
                      {4, 7, 10, 218});
        const float centerX =
            static_cast<float>(uiScreenWidth()) * 0.5F;
        const float centerY =
            static_cast<float>(uiScreenHeight()) * 0.5F;
        const float panelWidth = std::min(
            760.0F, static_cast<float>(uiScreenWidth()) - 40.0F);
        const float x = centerX - panelWidth * 0.5F;
        DrawRectangleRounded(
            {x, centerY - 190.0F, panelWidth, 380.0F},
            0.06F, 10, {24, 25, 28, 248});
        DrawRectangleRoundedLinesEx(
            {x, centerY - 190.0F, panelWidth, 380.0F},
            0.06F, 10, 3.0F, {232, 178, 74, 255});
        drawCenteredSdfUiText(
            "FORT DEFENDED", centerY - 145.0F,
            46.0F, {255, 205, 92, 255});
        drawCenteredSdfUiText(
            "STAGE CLEARED  •  BOSS DEFEATED",
            centerY - 78.0F, 19.0F, {229, 219, 190, 255});
        drawCenteredSdfUiText(
            "Your rewards are secured. Bank the run, or test the build",
            centerY - 25.0F, 16.0F, {184, 186, 179, 255});
        drawCenteredSdfUiText(
            "against an endless night that grows stronger every wave.",
            centerY + 3.0F, 16.0F, {184, 186, 179, 255});
        drawCenteredSdfUiText(
            "ENTER / F   FACE THE FINAL NIGHT",
            centerY + 72.0F, 21.0F, {242, 108, 82, 255});
        drawCenteredSdfUiText(
            "B   BANK REWARDS AND END RUN",
            centerY + 118.0F, 18.0F, {142, 207, 147, 255});
    }
}

} // namespace ian
