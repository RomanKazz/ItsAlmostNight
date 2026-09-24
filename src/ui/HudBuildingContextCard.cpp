#include "ui/HudBuildingContextCard.hpp"

#include "buildings/BuildingOrientation.hpp"
#include "buildings/CoreProgression.hpp"
#include "ui/GameUi.hpp"
#include "ui/HudFormatting.hpp"
#include "ui/UiLabels.hpp"
#include "ui/UiText.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace ian::hud_detail {

std::string actionKeyLabel(
    const ControlSettings& controls, ControlAction action) {
    const int key = controlKey(controls, action);
    if (action == ControlAction::Attack) {
        if (key == KEY_NULL) {
            return "LMB";
        }
        return "LMB / " + keyboardKeyName(key);
    }
    if (action == ControlAction::Dash && key == KEY_NULL) {
        return "RMB";
    }
    return keyboardKeyName(key);
}


void drawBuildingContextCard(
    GameUi& ui, const SimulationSnapshot& snapshot,
    const BuildingInstance& building, const HudViewState& view,
    const Camera3D& camera, const ControlSettings& controls) {
    const Vec3 buildingCenter =
        buildingWorldPosition(building);
    const float dx = static_cast<float>(buildingCenter.x) -
        camera.position.x;
    const float dz = static_cast<float>(buildingCenter.z) -
        camera.position.z;
    const float distance = std::sqrt(dx * dx + dz * dz);
    const float cardScale = std::clamp(
        3.0F / std::max(distance, 0.1F), 0.68F, 1.0F);
    const float CardWidth = 760.0F * cardScale;
    const float CardHeight = 520.0F * cardScale;
    const Vector2 buildingScreen = GetWorldToScreen(
        {static_cast<float>(buildingCenter.x), 1.15F,
         static_cast<float>(buildingCenter.z)},
        camera);
    const float HorizontalGap = 150.0F * cardScale;
    float x = buildingScreen.x + HorizontalGap;
    if (x + CardWidth >
        static_cast<float>(GetScreenWidth()) - 24.0F) {
        x = buildingScreen.x - CardWidth - HorizontalGap;
    }
    x = std::clamp(
        x, 24.0F,
        std::max(24.0F,
                 static_cast<float>(GetScreenWidth()) -
                     CardWidth - 24.0F));
    const float y = std::clamp(
        buildingScreen.y - CardHeight * 0.5F, 24.0F,
        std::max(24.0F,
                 static_cast<float>(GetScreenHeight()) -
                     CardHeight - 24.0F));
    ui.drawPanel({x, y, CardWidth, CardHeight}, 242);

    const auto point = [x, y, cardScale](float localX, float localY) {
        return Vector2{x + localX * cardScale,
                       y + localY * cardScale};
    };
    const auto font = [cardScale](float size) {
        return size * cardScale;
    };

    drawSdfUiText(buildingDisplayName(building.type), point(24.0F, 18.0F),
               font(26.0F), {255, 235, 174, 255});
    drawSdfUiText("LEVEL " + std::to_string(building.level),
               point(24.0F, 62.0F), font(19.0F), UiWhite);

    drawSdfUiText("STATS", point(24.0F, 105.0F),
               font(16.0F), {245, 184, 76, 255});

    struct StatRow {
        const char* label;
        double current;
        std::optional<double> previous;
        std::optional<double> next;
        int decimals;
        const char* suffix;
    };
    std::vector<StatRow> rows;
    rows.reserve(6);
    if (snapshot.aimedBuildingStats) {
        const BuildingStatComparison& comparison =
            *snapshot.aimedBuildingStats;
        const BuildingStats& current = comparison.current;
        const BuildingStats* previous =
            comparison.previous
                ? &*comparison.previous
                : nullptr;
        const BuildingStats* next =
            comparison.next ? &*comparison.next : nullptr;
        rows.push_back({
            "HEALTH", current.maxHealth,
            previous
                ? std::optional<double>(previous->maxHealth)
                : std::nullopt,
            next ? std::optional<double>(next->maxHealth)
                 : std::nullopt,
            0, "",
        });
        const auto addOptional =
            [&rows, &comparison, previous, next](
                const char* label,
                std::optional<double> BuildingStats::* member,
                int decimals, const char* suffix) {
                const auto currentValue =
                    comparison.current.*member;
                if (!currentValue) {
                    return;
                }
                rows.push_back({
                    label, *currentValue,
                    previous ? previous->*member : std::nullopt,
                    next ? next->*member : std::nullopt,
                    decimals, suffix,
                });
            };
        if (building.type == BuildingType::Core) {
            addOptional(
                "DEFENSE LIMIT",
                &BuildingStats::defenseBuildingLimit, 0, "");
            addOptional(
                "EACH PRODUCER",
                &BuildingStats::producerPerTypeLimit, 0, "");
            addOptional(
                "WOOD CAPACITY",
                &BuildingStats::woodCapacity, 0, "");
            addOptional(
                "STONE CAPACITY",
                &BuildingStats::stoneCapacity, 0, "");
            addOptional(
                "CRYSTAL CAPACITY",
                &BuildingStats::crystalCapacity, 0, "");
        } else if (building.type == BuildingType::Turret ||
            building.type == BuildingType::GunTurret ||
            building.type == BuildingType::Cannon ||
            building.type == BuildingType::Catapult) {
            addOptional(
                "DAMAGE", &BuildingStats::attackDamage, 0, "");
            addOptional(
                "RANGE", &BuildingStats::attackRange, 1, "m");
            addOptional(
                "ARC", &BuildingStats::attackArcDegrees, 0, " deg");
            addOptional(
                "ATTACK RATE",
                &BuildingStats::attacksPerSecond, 2, "/s");
            if (building.type == BuildingType::Turret) {
                addOptional(
                    "PIERCING",
                    &BuildingStats::piercingCount, 0, "");
            }
            if (building.type == BuildingType::Cannon ||
                building.type == BuildingType::Catapult) {
                addOptional(
                    "BLAST RADIUS",
                    &BuildingStats::effectRadius, 1, "m");
            }
        } else if (
            building.type == BuildingType::CrystalMine ||
            building.type == BuildingType::LumberMill ||
            building.type == BuildingType::Quarry) {
            addOptional(
                building.type == BuildingType::CrystalMine
                    ? "CRYSTALS / CYCLE"
                    : building.type ==
                              BuildingType::LumberMill
                          ? "WOOD / CYCLE"
                          : "STONE / CYCLE",
                &BuildingStats::productionPerCycle, 0, "");
            addOptional(
                "CYCLE TIME",
                &BuildingStats::productionInterval, 1, "s");
        } else if (building.type == BuildingType::SlowTrap) {
            addOptional(
                "SLOW", &BuildingStats::slowPercent, 0, "%");
            addOptional(
                "RADIUS", &BuildingStats::effectRadius, 1, "m");
            addOptional(
                "DURATION",
                &BuildingStats::effectDuration, 1, "s");
            addOptional(
                "COOLDOWN", &BuildingStats::cooldown, 1, "s");
        } else if (building.type == BuildingType::SpikeTrap) {
            addOptional(
                "DAMAGE", &BuildingStats::attackDamage, 0, "");
            addOptional(
                "RADIUS", &BuildingStats::effectRadius, 2, "m");
            addOptional(
                "COOLDOWN", &BuildingStats::cooldown, 2, "s");
        }
    } else {
        rows.push_back({
            "HEALTH", building.maxHealth,
            std::nullopt, std::nullopt, 0, "",
        });
    }

    const bool animatingUpgrade =
        view.buildingStatsUpgradeEntity &&
        *view.buildingStatsUpgradeEntity == building.id &&
        view.buildingStatsUpgradeRemaining > 0.0 &&
        view.buildingStatsUpgradeDuration > 0.0;
    const float rawProgress = animatingUpgrade
        ? std::clamp(
              static_cast<float>(
                  1.0 -
                  view.buildingStatsUpgradeRemaining /
                      view.buildingStatsUpgradeDuration),
              0.0F, 1.0F)
        : 1.0F;
    const float easedProgress =
        1.0F - std::pow(1.0F - rawProgress, 3.0F);
    const float numberPulse = animatingUpgrade
        ? std::sin(rawProgress * PI) * 3.0F
        : 0.0F;
    const Color currentColor = animatingUpgrade
        ? ColorLerp(
              Color{255, 191, 65, 255}, UiWhite,
              easedProgress)
        : UiWhite;
    const auto formatValue =
        [](double value, int decimals,
           const char* suffix) {
            char buffer[48]{};
            std::snprintf(
                buffer, sizeof(buffer),
                decimals == 0 ? "%.0f%s"
                              : decimals == 1 ? "%.1f%s"
                                              : "%.2f%s",
                value, suffix);
            return std::string(buffer);
        };
    for (std::size_t index = 0;
         index < rows.size() && index < 6; ++index) {
        const StatRow& row = rows[index];
        const float rowY =
            141.0F + static_cast<float>(index) * 31.0F;
        drawSdfUiText(
            row.label, point(24.0F, rowY), font(15.0F),
            {205, 197, 184, 255});
        double displayedCurrent = row.current;
        if (animatingUpgrade && row.previous) {
            displayedCurrent =
                *row.previous +
                (row.current - *row.previous) *
                    static_cast<double>(easedProgress);
        }
        drawSdfUiText(
            formatValue(
                displayedCurrent, row.decimals, row.suffix),
            point(265.0F, rowY - numberPulse * 0.5F),
            font(18.0F + numberPulse), currentColor);
        if (row.next) {
            drawSdfUiText(
                ">", point(390.0F, rowY), font(18.0F),
                {245, 184, 76, 255});
            drawSdfUiText(
                formatValue(*row.next, row.decimals, row.suffix),
                point(435.0F, rowY), font(18.0F),
                {105, 231, 132, 255});
            const double delta = *row.next - row.current;
            if (std::abs(delta) > 0.0001) {
                drawSdfUiText(
                    "+" + formatValue(
                              delta, row.decimals, row.suffix),
                    point(565.0F, rowY), font(14.0F),
                    {105, 231, 132, 220});
            }
        }
    }

    const bool globalBlueprint =
        BuildingSystem::usesGlobalBlueprint(building.type);
    const bool upgradeUnlocked =
        !globalBlueprint &&
        (building.type == BuildingType::Core ||
         snapshot.coreLevel > building.level);
    const bool upgradeAffordable =
        snapshot.aimedBuildingUpgradeCost &&
        upgradeUnlocked &&
        (snapshot.unlimitedResources ||
         (snapshot.wood >=
              snapshot.aimedBuildingUpgradeCost->wood &&
          snapshot.stone >=
              snapshot.aimedBuildingUpgradeCost->stone &&
          snapshot.crystals >=
              snapshot.aimedBuildingUpgradeCost->crystals));
    const bool coreActions = building.type == BuildingType::Core;
    const std::string actionPrefix = coreActions
        ? "F  REPAIR ALL    "
        : "Q  COPY    F  REPAIR    ";
    const float ActionFontSize = font(14.0F);
    if (upgradeAffordable) {
        const float glowPulse =
            0.5F +
            0.5F *
                std::sin(
                    static_cast<float>(GetTime()) * 4.2F);
        const float upgradeX =
            x + 24.0F * cardScale +
            measureUiText(
                actionPrefix, ActionFontSize).x -
            7.0F * cardScale;
        const Rectangle glowBounds{
            upgradeX, y + 332.0F * cardScale,
            150.0F * cardScale, 34.0F * cardScale};
        DrawRectangleRounded(
            glowBounds, 0.24F, 8,
            {
                245, 184, 76,
                static_cast<unsigned char>(
                    std::lround(
                        30.0F + glowPulse * 52.0F)),
            });
        DrawRectangleRoundedLinesEx(
            glowBounds, 0.24F, 8,
            (1.5F + glowPulse) * cardScale,
            {
                255, 220, 115,
                static_cast<unsigned char>(
                    std::lround(
                        115.0F + glowPulse * 110.0F)),
            });
    }
    std::string primaryActions;
    if (!coreActions) {
        primaryActions =
            actionKeyLabel(controls, ControlAction::Copy) +
            "  COPY    ";
    }
    primaryActions +=
        actionKeyLabel(controls, ControlAction::Repair) +
        (coreActions ? "  REPAIR ALL" : "  REPAIR");
    if (!globalBlueprint) {
        primaryActions += "    " +
            actionKeyLabel(controls, ControlAction::Upgrade) +
            "  UPGRADE";
    }
    drawSdfUiText(
               primaryActions,
               point(24.0F, 340.0F), font(14.0F),
               {222, 210, 194, 255});
    std::string actions;
    if (building.type != BuildingType::Core) {
        actions = "X  SELL";
        if (isDirectionalDefense(building.type)) {
            actions += "    R  ROTATE";
        }
    } else {
        actions = actionKeyLabel(controls, ControlAction::Interact) +
            "  MANAGE DEFENSES    F  REPAIR ALL  " +
            std::to_string(snapshot.repairAllCoinCost) +
            " COINS";
        if (snapshot.unlockedWeapons[
                static_cast<std::size_t>(PlayerWeapon::Bomb)]) {
            actions += "    V  +" +
                std::to_string(snapshot.bombPurchaseAmount) +
                " BOMBS  " +
                std::to_string(snapshot.bombPurchaseCoinCost) +
                " COINS";
        }
    }
    if (building.type == BuildingType::Gate) {
        actions += "    " +
            actionKeyLabel(controls, ControlAction::Interact) +
            "  TOGGLE";
    } else if (building.type != BuildingType::Core) {
        actions += "    " +
            actionKeyLabel(controls, ControlAction::Interact) +
            "  CLOSE";
    }
    if (!actions.empty()) {
        drawSdfUiText(actions, point(24.0F, 371.0F),
                   font(14.0F), {222, 210, 194, 255});
    }

    const ResourceCost repairCost = buildingRepairCost(building);
    if (repairCost.wood > 0 || repairCost.stone > 0 ||
        repairCost.crystals > 0) {
        drawSdfUiText(
            "REPAIR  " + hud_detail::costText(repairCost) + "  +" +
                std::to_string(static_cast<int>(std::ceil(
                    std::max(0.0, building.maxHealth - building.health)))) +
                " HP",
            point(24.0F, 399.0F), font(13.0F),
            {255, 210, 126, 235});
    } else if (building.health >= building.maxHealth) {
        drawSdfUiText(
            "REPAIR  FULL HEALTH",
            point(24.0F, 399.0F), font(13.0F),
            {145, 218, 159, 225});
    }

    if (globalBlueprint) {
        drawSdfUiText(
            "GLOBAL BLUEPRINT LEVEL " +
                std::to_string(building.level) +
                "  -  MANAGE AT CORE",
            point(24.0F, 424.0F), font(17.0F),
            {245, 184, 76, 255});
        return;
    }
    if (!snapshot.aimedBuildingUpgradeCost) {
        drawSdfUiText("MAX LEVEL", point(24.0F, 424.0F),
                   font(19.0F), {245, 184, 76, 255});
        return;
    }

    drawSdfUiText("UPGRADE", point(24.0F, 427.0F), font(16.0F),
               {245, 184, 76, 255});
    const ResourceCost cost =
        *snapshot.aimedBuildingUpgradeCost;
    float itemX = x + 190.0F * cardScale;
    const auto drawCost =
        [&ui, &itemX, y, cardScale](
            UiResourceIcon icon, int amount, int available) {
            if (amount <= 0) {
                return;
            }
            ui.drawResourceIcon(
                {itemX, y + 413.0F * cardScale,
                 48.0F * cardScale, 48.0F * cardScale}, icon);
            drawSdfUiText(
                std::to_string(amount),
                {itemX + 54.0F * cardScale,
                 y + 420.0F * cardScale},
                18.0F * cardScale,
                available >= amount
                    ? Color{238, 238, 226, 255}
                    : Color{242, 103, 83, 255});
            itemX += 142.0F * cardScale;
        };
    drawCost(UiResourceIcon::Wood, cost.wood, snapshot.wood);
    drawCost(UiResourceIcon::Stone, cost.stone, snapshot.stone);
    drawCost(
        UiResourceIcon::Crystal, cost.crystals, snapshot.crystals);
    if (building.type == BuildingType::Core) {
        const std::string_view unlocks = coreLevelUnlockSummary(
            static_cast<int>(building.level) + 1);
        if (!unlocks.empty()) {
            drawSdfUiText(unlocks, point(24.0F, 470.0F), font(13.0F),
                       {105, 231, 132, 245});
        }
    }
}


} // namespace ian::hud_detail
