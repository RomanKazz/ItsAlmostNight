#include "app/App.hpp"

#include "ui/UiLabels.hpp"
#include "ui/UiText.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace ian {
namespace {

std::string statValue(double value, int decimals,
                      const char* suffix = "") {
    char buffer[64]{};
    std::snprintf(buffer, sizeof(buffer),
                  decimals == 0 ? "%.0f%s" : "%.1f%s",
                  value, suffix);
    return buffer;
}

void drawCost(GameUi& ui, float x, float y,
              UiResourceIcon icon, int amount, int available) {
    if (amount <= 0) return;
    ui.drawResourceIcon({x, y, 40.0F, 40.0F}, icon);
    drawUiText(std::to_string(amount), {x + 45.0F, y + 6.0F},
               17.0F,
               available >= amount
                   ? Color{244, 239, 220, 255}
                   : Color{244, 102, 84, 255});
}

} // namespace

void App::drawCoreDefenseMenu() {
    if (!coreDefenseMenuVisible_) return;
    const SimulationSnapshot& snapshot = simulation_.snapshot();
    if (!snapshot.coreId) {
        coreDefenseMenuVisible_ = false;
        return;
    }

    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                  {4, 8, 10, 175});
    constexpr float Width = 930.0F;
    constexpr float Height = 650.0F;
    constexpr float Padding = 32.0F;
    const float x =
        (static_cast<float>(GetScreenWidth()) - Width) * 0.5F;
    const float y =
        (static_cast<float>(GetScreenHeight()) - Height) * 0.5F;
    ui_.drawPanel({x, y, Width, Height}, 252);
    ui_.drawInsetPanel({x + Padding, y + 24.0F,
                        Width - Padding * 2.0F, 76.0F}, 245);
    ui_.drawLabel({x + Padding, y + 27.0F,
                   Width - Padding * 2.0F, 48.0F},
                  "CORE DEFENSE MANAGEMENT", 1);
    drawUiText("One upgrade modernizes every existing defense of that type",
               {x + Padding + 12.0F, y + 75.0F}, 14.0F,
               {210, 202, 184, 235});

    constexpr float TabGap = 12.0F;
    const float tabWidth =
        (Width - Padding * 2.0F - TabGap * 3.0F) / 4.0F;
    for (std::size_t index = 0;
         index < snapshot.defenseBlueprints.size(); ++index) {
        const DefenseBlueprintStatus& status =
            snapshot.defenseBlueprints[index];
        const float tabX = x + Padding +
            static_cast<float>(index) * (tabWidth + TabGap);
        std::string label(buildingDisplayName(status.type));
        label += status.unlocked
            ? "  L" + std::to_string(status.level)
            : "  LOCKED";
        if (ui_.drawToggleButton(
                {tabX, y + 118.0F, tabWidth, 62.0F}, label,
                selectedDefenseBlueprint_ == index)) {
            selectedDefenseBlueprint_ = index;
            audio_.playUiConfirm();
        }
    }
    selectedDefenseBlueprint_ = std::min(
        selectedDefenseBlueprint_,
        snapshot.defenseBlueprints.size() - 1U);
    const DefenseBlueprintStatus& selected =
        snapshot.defenseBlueprints[selectedDefenseBlueprint_];

    ui_.drawInsetPanel({x + Padding, y + 202.0F,
                        Width - Padding * 2.0F, 326.0F}, 238);
    drawUiText(buildingDisplayName(selected.type),
               {x + 54.0F, y + 224.0F}, 28.0F,
               {255, 226, 154, 255});
    drawUiText("BLUEPRINT LEVEL " + std::to_string(selected.level),
               {x + 54.0F, y + 266.0F}, 18.0F, RAYWHITE);
    drawUiText("BUILT: " +
                   std::to_string(selected.existingBuildingCount),
               {x + 310.0F, y + 266.0F}, 18.0F,
               {205, 197, 184, 255});

    struct Row {
        const char* name;
        double current;
        std::optional<double> next;
        int decimals;
        const char* suffix;
    };
    std::array<Row, 6> rows{};
    std::size_t rowCount = 0;
    const BuildingStats& current = selected.stats.current;
    const BuildingStats* next = selected.stats.next
        ? &*selected.stats.next : nullptr;
    const auto add = [&](const char* name, double value,
                         std::optional<double> nextValue,
                         int decimals, const char* suffix) {
        if (rowCount < rows.size()) {
            rows[rowCount++] = {name, value, nextValue, decimals, suffix};
        }
    };
    add("HEALTH", current.maxHealth,
        next ? std::optional<double>{next->maxHealth} : std::nullopt,
        0, "");
    const auto addOptional = [&](const char* name,
                                 std::optional<double> value,
                                 std::optional<double> nextValue,
                                 int decimals, const char* suffix) {
        if (value) add(name, *value, nextValue, decimals, suffix);
    };
    addOptional("DAMAGE", current.attackDamage,
                next ? next->attackDamage : std::nullopt, 0, "");
    addOptional("RANGE", current.attackRange,
                next ? next->attackRange : std::nullopt, 1, "m");
    addOptional("ATTACK RATE", current.attacksPerSecond,
                next ? next->attacksPerSecond : std::nullopt, 1, "/s");
    addOptional("ARC", current.attackArcDegrees,
                next ? next->attackArcDegrees : std::nullopt, 0, " deg");
    addOptional(current.piercingCount ? "PIERCING" : "BLAST RADIUS",
                current.piercingCount
                    ? current.piercingCount : current.effectRadius,
                next ? (current.piercingCount
                            ? next->piercingCount : next->effectRadius)
                     : std::nullopt,
                current.piercingCount ? 0 : 1,
                current.piercingCount ? "" : "m");

    for (std::size_t index = 0; index < rowCount; ++index) {
        const Row& row = rows[index];
        const float rowY = y + 310.0F +
            static_cast<float>(index) * 32.0F;
        drawUiText(row.name, {x + 54.0F, rowY}, 15.0F,
                   {203, 195, 179, 255});
        drawUiText(statValue(row.current, row.decimals, row.suffix),
                   {x + 255.0F, rowY}, 17.0F, RAYWHITE);
        if (row.next) {
            drawUiText(">", {x + 390.0F, rowY}, 17.0F,
                       {245, 184, 76, 255});
            drawUiText(statValue(*row.next, row.decimals, row.suffix),
                       {x + 430.0F, rowY}, 17.0F,
                       {105, 231, 132, 255});
        }
    }

    const float sideX = x + 620.0F;
    drawUiText("TOTAL COST", {sideX, y + 232.0F}, 16.0F,
               {245, 184, 76, 255});
    drawCost(ui_, sideX, y + 270.0F, UiResourceIcon::Wood,
             selected.upgradeCost.wood, snapshot.wood);
    drawCost(ui_, sideX, y + 316.0F, UiResourceIcon::Stone,
             selected.upgradeCost.stone, snapshot.stone);
    drawCost(ui_, sideX, y + 362.0F, UiResourceIcon::Crystal,
             selected.upgradeCost.crystals, snapshot.crystals);
    drawUiText("Includes research + retrofit of " +
                   std::to_string(selected.existingBuildingCount) +
                   " built",
               {sideX, y + 414.0F}, 13.0F,
               {195, 188, 175, 230});

    std::string reason;
    if (!selected.unlocked) reason = "UPGRADE CORE TO UNLOCK";
    else if (snapshot.state == RunState::Wave)
        reason = "UNAVAILABLE DURING THE NIGHT";
    else if (selected.upgradeError == UpgradeError::MaxLevel)
        reason = "MAXIMUM BLUEPRINT LEVEL";
    else if (selected.upgradeError == UpgradeError::CoreLevelRequired)
        reason = "UPGRADE CORE FIRST";
    else if (selected.upgradeError == UpgradeError::InsufficientResources)
        reason = "NOT ENOUGH RESOURCES";

    const bool canUpgrade = selected.unlocked &&
        selected.upgradeError == UpgradeError::None;
    const std::string upgradeLabel = canUpgrade
        ? "UPGRADE ALL TO LEVEL " +
              std::to_string(selected.level + 1)
        : reason;
    if (ui_.drawButton({x + Padding, y + 550.0F,
                        Width - Padding * 2.0F - 190.0F, 66.0F},
                       upgradeLabel) && canUpgrade) {
        pendingBuildingBlueprintUpgrade_ =
            UpgradeBuildingBlueprintCommand{selected.type};
        audio_.playUiConfirm();
    }
    if (ui_.drawButton({x + Width - Padding - 172.0F, y + 550.0F,
                        172.0F, 66.0F}, "CLOSE")) {
        coreDefenseMenuVisible_ = false;
        if (snapshot.state != RunState::Paused) DisableCursor();
        audio_.playUiConfirm();
    }
}

} // namespace ian
