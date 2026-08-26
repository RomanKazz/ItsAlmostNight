#include "app/App.hpp"

#include "ui/UiText.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace ian {

void App::drawObjectiveDebugMenu(const SimulationSnapshot& snapshot) {
    if (!objectiveDebugMenuVisible_) return;

    const float width = std::min(
        1120.0F, static_cast<float>(GetScreenWidth()) - 48.0F);
    const float height = std::min(
        700.0F, static_cast<float>(GetScreenHeight()) - 48.0F);
    const float x = (static_cast<float>(GetScreenWidth()) - width) * 0.5F;
    const float y = (static_cast<float>(GetScreenHeight()) - height) * 0.5F;
    ui_.drawPanel({x, y, width, height}, 252);
    drawCenteredUiText("OBJECTIVE DEBUG [SHIFT+F9]", y + 14.0F, 24.0F,
                       {255, 224, 151, 255});

    constexpr float ColumnGap = 14.0F;
    const float columnWidth = (width - 40.0F - ColumnGap * 2.0F) / 3.0F;
    const ObjectiveKind kinds[]{ObjectiveKind::Milestone,
                                ObjectiveKind::Challenge,
                                ObjectiveKind::WorldEvent};
    for (int column = 0; column < 3; ++column) {
        const float columnX = x + 20.0F +
            static_cast<float>(column) * (columnWidth + ColumnGap);
        ui_.drawInsetPanel(
            {columnX, y + 55.0F, columnWidth, height - 75.0F}, 235);
        ui_.drawLabel(
            {columnX + 8.0F, y + 63.0F, columnWidth - 16.0F, 30.0F},
            objectiveKindName(kinds[column]), 1);
        float rowY = y + 100.0F;
        for (const ObjectiveStatus& status : snapshot.objectives) {
            if (status.definition.kind != kinds[column]) continue;
            const Color color = status.completed
                ? Color{117, 222, 139, 255}
                : status.active ? Color{239, 226, 194, 255}
                                : Color{130, 127, 122, 210};
            const std::string marker = status.completed ? "[DONE] "
                : status.active ? "[ACTIVE] " : "[INACTIVE] ";
            drawUiText(marker + status.definition.title,
                       {columnX + 10.0F, rowY}, 11.0F, color);
            const std::string progress =
                std::to_string(static_cast<int>(std::floor(status.progress))) +
                " / " +
                std::to_string(static_cast<int>(std::ceil(
                    status.definition.target))) +
                "   +" + std::to_string(static_cast<int>(std::lround(
                    status.definition.insightReward))) + " XP";
            drawUiText(progress, {columnX + 10.0F, rowY + 13.0F}, 9.0F,
                       status.active ? Color{182, 171, 158, 235}
                                     : Color{105, 103, 100, 190});
            rowY += 28.0F;
            if (rowY > y + height - 32.0F) break;
        }
    }
}

} // namespace ian
