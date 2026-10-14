#include "ui/HudRenderer.hpp"

#include "combat/CombatDisplayUnits.hpp"
#include "game/Simulation.hpp"
#include "ui/UiText.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace ian {
namespace {

std::string compactAmount(int amount) {
    if (amount < 1000) {
        return std::to_string(amount);
    }
    char text[24]{};
    if (amount < 10000) {
        std::snprintf(
            text, sizeof(text), "%.1fk",
            static_cast<double>(amount) / 1000.0);
    } else {
        std::snprintf(
            text, sizeof(text), "%dk", amount / 1000);
    }
    return text;
}

} // namespace

void drawRunStateOverlay(const SimulationSnapshot& snapshot) {
    if (snapshot.state == RunState::Paused) {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                      {0, 0, 0, 150});
        drawCenteredSdfUiText(
            "PAUSED", static_cast<float>(GetScreenHeight() / 2 - 24),
            48.0F, UiWhite);
    } else if (snapshot.state == RunState::StageClear) {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                      {4, 7, 10, 218});
        const float centerX =
            static_cast<float>(GetScreenWidth()) * 0.5F;
        const float centerY =
            static_cast<float>(GetScreenHeight()) * 0.5F;
        const float panelWidth = std::min(
            760.0F, static_cast<float>(GetScreenWidth()) - 40.0F);
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
            "STAGE CLEARED  •  WAVE 12 BOSS DEFEATED",
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
    } else if (snapshot.state == RunState::Defeat ||
               snapshot.state == RunState::Victory) {
        const bool successfulRun = snapshot.stageCleared;
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                      {4, 7, 10, 210});
        const float screenWidth = static_cast<float>(GetScreenWidth());
        const float screenHeight = static_cast<float>(GetScreenHeight());
        const float panelWidth = std::min(760.0F, screenWidth - 40.0F);
        const float panelHeight = std::min(570.0F, screenHeight - 34.0F);
        const float x = (screenWidth - panelWidth) * 0.5F;
        const float y = (screenHeight - panelHeight) * 0.5F;
        DrawRectangleRounded(
            {x, y, panelWidth, panelHeight}, 0.055F, 10,
            {24, 25, 28, 246});
        DrawRectangleRoundedLinesEx(
            {x, y, panelWidth, panelHeight}, 0.055F, 10, 3.0F,
            successfulRun ? Color{188, 145, 65, 255}
                          : Color{142, 96, 65, 255});

        drawCenteredSdfUiText(
            successfulRun ? "FORT DEFENDED" : "CORE DESTROYED",
            y + 24.0F, 43.0F,
            successfulRun ? Color{255, 205, 92, 255}
                          : Color{242, 91, 73, 255});
        char survival[96]{};
        const int elapsed = std::max(
            0, static_cast<int>(snapshot.elapsedSeconds));
        std::snprintf(
            survival, sizeof(survival),
            "SURVIVED  %02d:%02d   •   WAVES CLEARED  %d",
            elapsed / 60, elapsed % 60,
            snapshot.runStatistics.wavesSurvived);
        drawCenteredSdfUiText(
            survival, y + 82.0F, 19.0F,
            {224, 205, 170, 255});

        const float cardGap = 16.0F;
        const float cardX = x + 28.0F;
        const float cardY = y + 124.0F;
        const float cardWidth = (panelWidth - 72.0F) * 0.5F;
        const auto drawSummaryCard = [&](float left, std::string_view label,
                                         std::string value, Color color) {
            DrawRectangleRounded(
                {left, cardY, cardWidth, 91.0F}, 0.12F, 7,
                {38, 38, 40, 245});
            drawSdfUiText(label, {left + 18.0F, cardY + 12.0F}, 14.0F,
                       {169, 162, 151, 255});
            drawSdfUiText(value, {left + 18.0F, cardY + 37.0F}, 31.0F,
                       color);
        };
        const double totalDamage =
            snapshot.runStatistics.playerDamageDealt +
            snapshot.runStatistics.defenseDamageDealt;
        drawSummaryCard(
            cardX, "ENEMIES DEFEATED",
            compactAmount(snapshot.runStatistics.enemiesDefeated),
            {255, 177, 84, 255});
        drawSummaryCard(
            cardX + cardWidth + cardGap, "TOTAL DAMAGE",
            compactAmount(combatDisplayValue(totalDamage)),
            {239, 116, 88, 255});

        const float detailsY = cardY + 118.0F;
        const float leftX = x + 46.0F;
        const float rightX = x + panelWidth * 0.54F;
        const auto drawStat = [&](float left, float top,
                                  std::string_view label,
                                  std::string value, Color valueColor) {
            drawSdfUiText(label, {left, top}, 15.0F,
                       {155, 151, 145, 255});
            const Vector2 valueSize = measureUiText(value, 18.0F);
            drawSdfUiText(
                value,
                {left + panelWidth * 0.39F - valueSize.x, top - 2.0F},
                18.0F, valueColor);
        };
        drawStat(leftX, detailsY, "PLAYER DAMAGE",
                 compactAmount(combatDisplayValue(
                     snapshot.runStatistics.playerDamageDealt)),
                 {242, 204, 119, 255});
        drawStat(leftX, detailsY + 38.0F, "DEFENSE DAMAGE",
                 compactAmount(combatDisplayValue(
                     snapshot.runStatistics.defenseDamageDealt)),
                 {115, 199, 239, 255});
        drawStat(leftX, detailsY + 76.0F, "STRUCTURES BUILT",
                 std::to_string(snapshot.runStatistics.structuresBuilt),
                 {145, 217, 151, 255});
        drawStat(leftX, detailsY + 114.0F, "STRUCTURES LOST",
                 std::to_string(snapshot.runStatistics.structuresLost),
                 {239, 125, 105, 255});

        drawStat(rightX, detailsY, "WOOD ACQUIRED",
                 compactAmount(snapshot.runStatistics.woodAcquired),
                 {218, 164, 93, 255});
        drawStat(rightX, detailsY + 38.0F, "STONE ACQUIRED",
                 compactAmount(snapshot.runStatistics.stoneAcquired),
                 {185, 194, 204, 255});
        drawStat(rightX, detailsY + 76.0F, "CRYSTALS ACQUIRED",
                 compactAmount(snapshot.runStatistics.crystalsAcquired),
                 {186, 121, 239, 255});
        drawStat(rightX, detailsY + 114.0F, "COINS COLLECTED",
                 compactAmount(snapshot.runStatistics.coinsCollected),
                 {255, 198, 65, 255});

        DrawLineEx(
            {x + 36.0F, y + panelHeight - 72.0F},
            {x + panelWidth - 36.0F, y + panelHeight - 72.0F},
            2.0F, {85, 70, 58, 190});
        drawCenteredSdfUiText(
            "R  RESTART     •     ESC  MENU",
            y + panelHeight - 47.0F, 18.0F,
            {238, 230, 210, 255});
    }
}

} // namespace ian
