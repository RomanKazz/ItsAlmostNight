#include "ui/HudRenderer.hpp"

#include "app/UserSettings.hpp"
#include "buildings/BuildingOrientation.hpp"
#include "game/Simulation.hpp"
#include "ui/GameUi.hpp"
#include "ui/HudFormatting.hpp"
#include "ui/HudHotbar.hpp"
#include "ui/UiLabels.hpp"
#include "ui/UiText.hpp"

#include <raylib.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace ian {
namespace {

const char* placementMessage(
    PlacementError error, BuildingType type) {
    switch (error) {
    case PlacementError::None:
        return supportsManualBuildingRotation(type)
            ? "LMB place/drag   RMB cancel   R/Wheel rotate"
            : "LMB place/drag   RMB cancel";
    case PlacementError::CoreAlreadyPlaced:
        return "Core already placed";
    case PlacementError::CoreRequired:
        return "Place Core first";
    case PlacementError::InsufficientResources:
        return "Not enough resources";
    case PlacementError::Occupied:
        return "Space occupied";
    case PlacementError::OutsideCoreArea:
        return "Outside Core area";
    case PlacementError::PlayerOverlap:
        return "Player blocks placement";
    case PlacementError::WorldCollision:
        return "Terrain blocks placement";
    case PlacementError::LimitReached:
        return "Building limit reached";
    case PlacementError::OutOfRange:
        return "Placement is too far";
    case PlacementError::CoreLevelRequired:
        return "Core level II required";
    case PlacementError::SkillRequired:
        return "Unlock in Skill Tree";
    case PlacementError::ResourceBlocked:
        return "Clear tree or stone first";
    }
    return "";
}

Color placementMessageColor(PlacementError error) {
    switch (error) {
    case PlacementError::None:
        return {92, 225, 117, 255};
    case PlacementError::InsufficientResources:
        return {255, 176, 62, 255};
    case PlacementError::OutsideCoreArea:
        return {187, 104, 255, 255};
    case PlacementError::OutOfRange:
        return {94, 172, 255, 255};
    case PlacementError::PlayerOverlap:
        return {255, 222, 82, 255};
    case PlacementError::CoreRequired:
    case PlacementError::CoreLevelRequired:
    case PlacementError::SkillRequired:
    case PlacementError::CoreAlreadyPlaced:
    case PlacementError::LimitReached:
        return {255, 132, 71, 255};
    case PlacementError::Occupied:
    case PlacementError::WorldCollision:
    case PlacementError::ResourceBlocked:
        return {242, 91, 73, 255};
    }
    return {242, 91, 73, 255};
}

const char* attackDirectionName(AttackDirection direction) {
    switch (direction) {
    case AttackDirection::North:
        return "NORTH";
    case AttackDirection::East:
        return "EAST";
    case AttackDirection::South:
        return "SOUTH";
    case AttackDirection::West:
        return "WEST";
    }
    return "";
}

bool hasAttackDirections(const SimulationSnapshot& snapshot) {
    return std::ranges::any_of(
        snapshot.upcomingAttackDirections,
        [](bool active) { return active; });
}

std::string attackDirectionNames(const SimulationSnapshot& snapshot) {
    std::string names;
    for (std::size_t index = 0;
         index < snapshot.upcomingAttackDirections.size(); ++index) {
        if (!snapshot.upcomingAttackDirections[index]) {
            continue;
        }
        if (!names.empty()) {
            names += " + ";
        }
        names += attackDirectionName(static_cast<AttackDirection>(index));
    }
    return names;
}

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

const char* objectiveKindLabel(ObjectiveKind kind) {
    switch (kind) {
    case ObjectiveKind::Milestone:
        return "MILESTONE";
    case ObjectiveKind::Challenge:
        return "CHALLENGE";
    case ObjectiveKind::WorldEvent:
        return "WORLD EVENT";
    }
    return "OBJECTIVE";
}

Color objectiveKindColor(ObjectiveKind kind) {
    switch (kind) {
    case ObjectiveKind::Milestone:
        return {184, 151, 255, 255};
    case ObjectiveKind::Challenge:
        return {255, 190, 73, 255};
    case ObjectiveKind::WorldEvent:
        return {79, 220, 207, 255};
    }
    return {235, 214, 170, 255};
}

UiBarColor objectiveBarColor(ObjectiveKind kind) {
    switch (kind) {
    case ObjectiveKind::Milestone:
        return UiBarColor::Purple;
    case ObjectiveKind::Challenge:
        return UiBarColor::Yellow;
    case ObjectiveKind::WorldEvent:
        return UiBarColor::Green;
    }
    return UiBarColor::Yellow;
}

std::string objectiveInstruction(
    const ObjectiveDefinition& objective) {
    if (!objective.description.empty()) {
        return objective.description;
    }
    switch (objective.metric) {
    case ObjectiveMetric::TreesDestroyed:
        return "Fully chop down tree resource objects";
    case ObjectiveMetric::StonesDestroyed:
        return "Fully break stone resource objects";
    case ObjectiveMetric::CrystalsGathered:
        return "Collect crystal resource units";
    case ObjectiveMetric::TotalResourcesGathered:
        return "Collect wood, stone or crystals";
    case ObjectiveMetric::LargeDepositDepleted:
        return "Fully deplete a large resource deposit";
    case ObjectiveMetric::AllResourceTypesInDay:
        return "Gather wood, stone and crystals in one day";
    case ObjectiveMetric::BareHandsDepletion:
        return "Destroy a tree or stone without a tool";
    case ObjectiveMetric::ResourcesInSixtySeconds:
        return "Gather 50 resources within 60 seconds";
    case ObjectiveMetric::ConsecutiveDepletions:
        return "Destroy 5 resource objects without missing";
    case ObjectiveMetric::NightResourcesGathered:
        return "Gather 30 resources during the night";
    case ObjectiveMetric::FarResourceGathered:
        return "Gather a resource far away from the Core";
    case ObjectiveMetric::EnemiesKilled:
        return "Defeat enemies by any means";
    case ObjectiveMetric::BuildingsPlaced:
        return "Place complete buildings";
    case ObjectiveMetric::ModularPiecesPlaced:
        return "Place platforms, walls, ramps or gates";
    case ObjectiveMetric::BuildingsUpgraded:
        return "Upgrade existing buildings";
    case ObjectiveMetric::StructuresRepaired:
        return "Repair damaged buildings or modular pieces";
    case ObjectiveMetric::WavesCompleted:
        return "Survive enemy waves";
    case ObjectiveMetric::CoinsCollected:
        return "Pick up Coins dropped in the world";
    case ObjectiveMetric::ChestsOpened:
        return "Open loot chests";
    case ObjectiveMetric::LootCollected:
        return "Collect items from chests or props";
    case ObjectiveMetric::PlayerDashes:
        return "Use Dash while moving";
    case ObjectiveMetric::RifleShots:
        return "Fire the rifle";
    case ObjectiveMetric::ElementalHits:
        return "Damage enemies with Ice or Fire Wand";
    case ObjectiveMetric::TrapHits:
        return "Let traps hit enemies";
    case ObjectiveMetric::CannonShots:
        return "Let cannons fire at enemies";
    case ObjectiveMetric::BombsThrown:
        return "Throw bombs";
    case ObjectiveMetric::EarlyWavesStarted:
        return "Start waves early enough to earn a reward";
    case ObjectiveMetric::StructuresFortified:
        return "Use the hammer to fortify structures";
    case ObjectiveMetric::GatesToggled:
        return "Open or close gates";
    case ObjectiveMetric::BuildingsSold:
        return "Sell buildings";
    case ObjectiveMetric::FallsSaved:
        return "Use Rope to survive a lethal fall";
    case ObjectiveMetric::Count:
        break;
    }
    return objective.title;
}

void drawObjectiveKindIcon(
    ObjectiveKind kind, Vector2 center, Color color) {
    if (kind == ObjectiveKind::Milestone) {
        DrawPoly(center, 4, 9.0F, 45.0F, color);
        DrawPolyLines(center, 4, 12.0F, 45.0F,
                      {color.r, color.g, color.b, 110});
    } else if (kind == ObjectiveKind::Challenge) {
        DrawPoly(center, 5, 10.0F, -90.0F, color);
        DrawCircleV(center, 3.2F, {255, 247, 211, 240});
    } else {
        DrawCircleLines(
            static_cast<int>(center.x),
            static_cast<int>(center.y), 10.0F, color);
        DrawCircleLines(
            static_cast<int>(center.x),
            static_cast<int>(center.y), 5.0F,
            {color.r, color.g, color.b, 175});
        DrawCircleV(center, 2.5F, color);
    }
}

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

std::string tutorialText(const SimulationSnapshot& snapshot) {
    if (!snapshot.tutorialObjective) {
        return {};
    }
    switch (*snapshot.tutorialObjective) {
    case TutorialObjective::BareHandsTraining:
        return "OBJECTIVE: Gather by hand - Wood " +
               std::to_string(snapshot.bareHandsWoodGathered) + "/15  Stone " +
               std::to_string(snapshot.bareHandsStoneGathered) + "/10";
    case TutorialObjective::MineWood:
        return "OBJECTIVE: Mine trees - Wood " +
               std::to_string(snapshot.wood) + "/" +
               std::to_string(snapshot.tutorialWoodTarget);
    case TutorialObjective::PlaceCore:
        return "OBJECTIVE: Place Core [1]";
    case TutorialObjective::MineStone:
        return "OBJECTIVE: Mine rocks - Stone " +
               std::to_string(snapshot.stone) + "/" +
               std::to_string(snapshot.tutorialStoneTarget);
    case TutorialObjective::BuildCrystalMine:
        return snapshot.unlockedBuildings[
                   static_cast<std::size_t>(
                       BuildingType::CrystalMine)]
            ? "OBJECTIVE: Build Crystal Mine [4]"
            : "OBJECTIVE: Unlock Crystal Mine [K]";
    case TutorialObjective::PrepareForNight:
        return "OBJECTIVE: Build defenses - [N] starts sunset";
    case TutorialObjective::SurviveFirstWave:
        return "OBJECTIVE: Survive first night";
    }
    return {};
}

void drawBuildingContextCard(
    GameUi& ui, const SimulationSnapshot& snapshot,
    const BuildingInstance& building, const HudViewState& view,
    const Camera3D& camera, const ControlSettings& controls) {
    constexpr float CardWidth = 760.0F;
    constexpr float CardHeight = 480.0F;
    const Vec3 buildingCenter =
        buildingWorldPosition(building);
    const Vector2 buildingScreen = GetWorldToScreen(
        {static_cast<float>(buildingCenter.x), 1.15F,
         static_cast<float>(buildingCenter.z)},
        camera);
    constexpr float HorizontalGap = 150.0F;
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

    drawUiText(buildingDisplayName(building.type), {x + 24.0F, y + 18.0F},
               26.0F, {255, 235, 174, 255});
    drawUiText("LEVEL " + std::to_string(building.level),
               {x + 24.0F, y + 62.0F}, 19.0F, RAYWHITE);

    drawUiText("STATS", {x + 24.0F, y + 105.0F},
               16.0F, {245, 184, 76, 255});

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
        } else if (
            building.type == BuildingType::WoodStorage ||
            building.type == BuildingType::StoneStorage ||
            building.type == BuildingType::CrystalStorage) {
            addOptional(
                "CAPACITY", &BuildingStats::storageCapacity,
                0, "");
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
              Color{255, 191, 65, 255}, RAYWHITE,
              easedProgress)
        : RAYWHITE;
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
            y + 141.0F + static_cast<float>(index) * 31.0F;
        drawUiText(
            row.label, {x + 24.0F, rowY}, 15.0F,
            {205, 197, 184, 255});
        double displayedCurrent = row.current;
        if (animatingUpgrade && row.previous) {
            displayedCurrent =
                *row.previous +
                (row.current - *row.previous) *
                    static_cast<double>(easedProgress);
        }
        drawUiText(
            formatValue(
                displayedCurrent, row.decimals, row.suffix),
            {x + 265.0F, rowY - numberPulse * 0.5F},
            18.0F + numberPulse, currentColor);
        if (row.next) {
            drawUiText(
                ">", {x + 390.0F, rowY}, 18.0F,
                {245, 184, 76, 255});
            drawUiText(
                formatValue(*row.next, row.decimals, row.suffix),
                {x + 435.0F, rowY}, 18.0F,
                {105, 231, 132, 255});
            const double delta = *row.next - row.current;
            if (std::abs(delta) > 0.0001) {
                drawUiText(
                    "+" + formatValue(
                              delta, row.decimals, row.suffix),
                    {x + 565.0F, rowY}, 14.0F,
                    {105, 231, 132, 220});
            }
        }
    }

    const bool upgradeUnlocked =
        building.type == BuildingType::Core ||
        snapshot.coreLevel > building.level;
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
        ? "Q  COPY    F  REPAIR ALL    "
        : "Q  COPY    F  REPAIR    ";
    constexpr float ActionFontSize = 14.0F;
    if (upgradeAffordable) {
        const float glowPulse =
            0.5F +
            0.5F *
                std::sin(
                    static_cast<float>(GetTime()) * 4.2F);
        const float upgradeX =
            x + 24.0F +
            measureUiText(
                actionPrefix, ActionFontSize).x -
            7.0F;
        const Rectangle glowBounds{
            upgradeX, y + 332.0F, 150.0F, 34.0F};
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
            1.5F + glowPulse,
            {
                255, 220, 115,
                static_cast<unsigned char>(
                    std::lround(
                        115.0F + glowPulse * 110.0F)),
            });
    }
    drawUiText(
               actionKeyLabel(controls, ControlAction::Copy) +
                   "  COPY    " +
                   actionKeyLabel(controls, ControlAction::Repair) +
                   (coreActions ? "  REPAIR ALL    " : "  REPAIR    ") +
                   actionKeyLabel(controls, ControlAction::Upgrade) +
                   "  UPGRADE",
               {x + 24.0F, y + 340.0F}, 14.0F,
               {222, 210, 194, 255});
    std::string actions;
    if (building.type != BuildingType::Core) {
        actions = "X  SELL";
    } else {
        actions = "F  REPAIR ALL  " +
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
        actions += "    E  TOGGLE";
    }
    if (!actions.empty()) {
        drawUiText(actions, {x + 24.0F, y + 371.0F},
                   14.0F, {222, 210, 194, 255});
    }

    const ResourceCost repairCost = buildingRepairCost(building);
    if (repairCost.wood > 0 || repairCost.stone > 0 ||
        repairCost.crystals > 0) {
        drawUiText(
            "REPAIR  " + hud_detail::costText(repairCost) + "  +" +
                std::to_string(static_cast<int>(std::ceil(
                    std::max(0.0, building.maxHealth - building.health)))) +
                " HP",
            {x + 24.0F, y + 399.0F}, 13.0F,
            {255, 210, 126, 235});
    } else if (building.health >= building.maxHealth) {
        drawUiText(
            "REPAIR  FULL HEALTH",
            {x + 24.0F, y + 399.0F}, 13.0F,
            {145, 218, 159, 225});
    }

    if (!snapshot.aimedBuildingUpgradeCost) {
        drawUiText("MAX LEVEL", {x + 24.0F, y + 424.0F},
                   19.0F, {245, 184, 76, 255});
        return;
    }

    drawUiText("UPGRADE", {x + 24.0F, y + 427.0F}, 16.0F,
               {245, 184, 76, 255});
    const ResourceCost cost =
        *snapshot.aimedBuildingUpgradeCost;
    float itemX = x + 190.0F;
    const auto drawCost =
        [&ui, &itemX, y](UiResourceIcon icon, int amount,
                         int available) {
            if (amount <= 0) {
                return;
            }
            ui.drawResourceIcon(
                {itemX, y + 413.0F, 48.0F, 48.0F}, icon);
            drawUiText(
                std::to_string(amount),
                {itemX + 54.0F, y + 420.0F}, 18.0F,
                available >= amount
                    ? Color{238, 238, 226, 255}
                    : Color{242, 103, 83, 255});
            itemX += 142.0F;
        };
    drawCost(UiResourceIcon::Wood, cost.wood, snapshot.wood);
    drawCost(UiResourceIcon::Stone, cost.stone, snapshot.stone);
    drawCost(
        UiResourceIcon::Crystal, cost.crystals, snapshot.crystals);
}

std::string phaseClock(double seconds) {
    const int total = std::max(0, static_cast<int>(std::ceil(seconds)));
    char buffer[16]{};
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d", total / 60,
                  total % 60);
    return buffer;
}

} // namespace

using namespace hud_detail;

void drawHud(GameUi& ui, const SimulationSnapshot& snapshot,
             const HudViewState& view, const Camera3D& camera,
             const ControlSettings& controls) {
    const float woodY = -view.woodResourceBounce;
    const float stoneY = -view.stoneResourceBounce;
    const float crystalY = -view.crystalResourceBounce;
    const float coinY = -view.coinResourceBounce;
    constexpr float ResourcePanelX = 12.0F;
    constexpr float ResourcePanelY = 12.0F;
    constexpr float ResourceCellWidth = 145.0F;
    constexpr float ResourcePanelWidth =
        ResourceCellWidth * 4.0F + 16.0F;
    ui.drawPanel(
        {ResourcePanelX, ResourcePanelY,
         ResourcePanelWidth, 56.0F}, 170);
    const auto drawResourceCell =
        [&ui](std::size_t index, UiResourceIcon icon,
              std::string value, float bounce, Color color) {
            const float x = 20.0F +
                static_cast<float>(index) * ResourceCellWidth;
            ui.drawResourceIcon(
                {x, 21.0F + bounce, 34.0F, 34.0F}, icon);
            drawUiText(
                value, {x + 40.0F, 24.0F + bounce},
                17.0F, color);
        };
    drawResourceCell(
        0U, UiResourceIcon::Wood,
        snapshot.unlimitedResources
            ? "∞"
            : compactAmount(snapshot.wood) + "/" +
                  compactAmount(snapshot.woodCapacity),
        woodY, RAYWHITE);
    drawResourceCell(
        1U, UiResourceIcon::Stone,
        snapshot.unlimitedResources
            ? "∞"
            : compactAmount(snapshot.stone) + "/" +
                  compactAmount(snapshot.stoneCapacity),
        stoneY, RAYWHITE);
    drawResourceCell(
        2U, UiResourceIcon::Crystal,
        snapshot.unlimitedResources
            ? "∞"
            : compactAmount(snapshot.crystals) + "/" +
                  compactAmount(snapshot.crystalCapacity),
        crystalY,
        snapshot.crystalStorageFull
            ? Color{255, 174, 112, 255}
            : RAYWHITE);
    if (snapshot.crystalStorageFull) {
        drawUiText(
            "FULL",
            {20.0F + 2.0F * ResourceCellWidth + 40.0F,
             45.0F},
            8.0F, {255, 153, 92, 245});
    }
    drawResourceCell(
        3U, UiResourceIcon::Coin,
        snapshot.unlimitedResources
            ? "∞"
            : compactAmount(snapshot.coins),
        coinY, {255, 236, 152, 255});
    drawUiText(
        "L FIND CHEST " +
            std::to_string(snapshot.chestRevealCoinCost),
        {ResourcePanelX + ResourcePanelWidth - 142.0F,
         ResourcePanelY + 40.0F},
        8.0F, {220, 207, 173, 205});
    const auto drawResourcePulse =
        [](Rectangle bounds, float remaining) {
            if (remaining <= 0.0F) {
                return;
            }
            const float progress =
                1.0F - std::clamp(remaining, 0.0F, 1.0F);
            const float pulse =
                std::sin(progress * PI);
            const auto alpha = static_cast<unsigned char>(
                std::lround(pulse * 245.0F));
            DrawRectangleLinesEx(
                bounds, 3.0F + pulse * 2.0F,
                {255, 236, 160, alpha});
        };
    const std::array<float, 4> resourcePulses{
        view.woodResourcePulse, view.stoneResourcePulse,
        view.crystalResourcePulse, view.coinResourcePulse};
    for (std::size_t index = 0;
         index < resourcePulses.size(); ++index) {
        drawResourcePulse(
            {16.0F + static_cast<float>(index) * ResourceCellWidth,
             16.0F, ResourceCellWidth - 4.0F, 48.0F},
            resourcePulses[index]);
    }

    const float healthY = static_cast<float>(GetScreenHeight()) -
        (view.showCoreHealth ? 132.0F : 76.0F);
    const double healthFraction =
        snapshot.playerHealth / snapshot.playerMaxHealth;
    drawUiText(
        "YOU  " +
            std::to_string(static_cast<int>(snapshot.playerHealth)) +
            " / " +
            std::to_string(static_cast<int>(snapshot.playerMaxHealth)),
        {18.0F, healthY}, 15.0F,
        healthFraction > 0.3 ? Color{176, 225, 179, 255}
                             : Color{240, 116, 98, 255});
    const bool hasRecoverableArmor =
        snapshot.playerMaxRecoverableArmor > 0.0;
    if (hasRecoverableArmor) {
        std::string armorText =
            "ARMOR " +
            std::to_string(static_cast<int>(std::ceil(
                snapshot.playerRecoverableArmor))) +
            "/" +
            std::to_string(static_cast<int>(std::ceil(
                snapshot.playerMaxRecoverableArmor)));
        if (snapshot.playerRecoverableArmor <
                snapshot.playerMaxRecoverableArmor &&
            snapshot.playerArmorRechargeDelayRemaining > 0.05) {
            armorText += "  " + std::to_string(
                static_cast<int>(std::ceil(
                    snapshot.playerArmorRechargeDelayRemaining))) + "s";
        }
        drawUiText(
            armorText, {154.0F, healthY + 2.0F}, 9.0F,
            snapshot.playerArmorRechargeDelayRemaining > 0.05
                ? Color{127, 172, 190, 235}
                : Color{139, 225, 250, 255});
    }
    const Rectangle healthBarBounds{
        18.0F, healthY + 27.0F, 260.0F, 16.0F};
    ui.drawProgressBar(
        healthBarBounds,
        static_cast<float>(healthFraction),
        healthFraction > 0.3 ? UiBarColor::Green : UiBarColor::Red);
    if (hasRecoverableArmor) {
        // Armor is a protective coating on the health bar rather than a
        // second standalone resource bar.
        ui.drawProgressBar(
            {healthBarBounds.x, healthBarBounds.y,
             healthBarBounds.width, 8.0F},
            static_cast<float>(
                snapshot.playerRecoverableArmor /
                snapshot.playerMaxRecoverableArmor),
            UiBarColor::Blue);
    }

    if (view.showCoreHealth && snapshot.coreMaxHealth > 0.0) {
        drawUiText(
            "CORE L" + std::to_string(snapshot.coreLevel) + "  " +
                std::to_string(static_cast<int>(snapshot.coreHealth)) +
                " / " +
                std::to_string(static_cast<int>(snapshot.coreMaxHealth)),
            {18.0F, healthY + 55.0F}, 15.0F,
            {232, 196, 123, 255});
        ui.drawProgressBar(
            {18.0F, healthY + 82.0F, 260.0F, 16.0F},
            static_cast<float>(snapshot.coreHealth /
                               snapshot.coreMaxHealth),
            UiBarColor::Yellow);
    }
    if (snapshot.unlimitedResources) {
        drawUiText("∞", {256.0F, healthY - 2.0F}, 20.0F,
                   {111, 220, 151, 255});
    }

    int objectiveCount = 0;
    for (int index : snapshot.recommendedObjectives) {
        if (index >= 0 &&
            static_cast<std::size_t>(index) < snapshot.objectives.size())
            ++objectiveCount;
    }
    if (!view.mapOverlayOpen &&
        objectiveCount > 0 &&
        view.informationExpansion > 0.65F) {
        constexpr float ObjectiveX = 12.0F;
        constexpr float ObjectiveY = 374.0F;
        constexpr float ObjectiveWidth = 530.0F;
        constexpr float HeaderHeight = 48.0F;
        constexpr float CardHeight = 88.0F;
        constexpr float CardGap = 7.0F;
        const float objectiveHeight = HeaderHeight + 9.0F +
            static_cast<float>(objectiveCount) * CardHeight +
            static_cast<float>(objectiveCount - 1) * CardGap;
        ui.drawPanel(
            {ObjectiveX, ObjectiveY, ObjectiveWidth,
             objectiveHeight}, 224);
        DrawCircleV(
            {ObjectiveX + 24.0F, ObjectiveY + 23.0F},
            11.0F, {102, 78, 48, 230});
        DrawPoly(
            {ObjectiveX + 24.0F, ObjectiveY + 23.0F},
            4, 6.5F, 45.0F, {255, 215, 105, 255});
        drawUiText(
            "OBJECTIVE TRACKER",
            {ObjectiveX + 43.0F, ObjectiveY + 9.0F},
            14.0F, {255, 225, 157, 255});
        drawUiText(
            "Complete tasks to earn Insight",
            {ObjectiveX + 43.0F, ObjectiveY + 27.0F},
            10.5F, {190, 178, 157, 225});
        const std::string activeCount =
            "TRACKED  " + std::to_string(objectiveCount);
        const float activeWidth =
            measureUiText(activeCount, 10.5F).x;
        drawUiText(
            activeCount,
            {ObjectiveX + ObjectiveWidth - activeWidth - 15.0F,
             ObjectiveY + 17.0F},
            10.5F, {232, 205, 139, 230});
        int row = 0;
        for (int rawIndex : snapshot.recommendedObjectives) {
            if (rawIndex < 0 ||
                static_cast<std::size_t>(rawIndex) >= snapshot.objectives.size())
                continue;
            const ObjectiveStatus& objective =
                snapshot.objectives[static_cast<std::size_t>(rawIndex)];
            const float rowY = ObjectiveY + HeaderHeight +
                static_cast<float>(row) * (CardHeight + CardGap);
            const Rectangle card{
                ObjectiveX + 9.0F, rowY,
                ObjectiveWidth - 18.0F, CardHeight};
            const Color kindColor =
                objectiveKindColor(objective.definition.kind);
            const bool pulsing =
                view.objectivePulse > 0.0 &&
                view.objectivePulseId == objective.definition.id;
            if (pulsing) {
                const float pulse = static_cast<float>(
                    std::sin(view.objectivePulse * PI));
                DrawRectangleRounded(
                    {card.x - 4.0F - pulse * 2.0F,
                     card.y - 4.0F - pulse * 2.0F,
                     card.width + 8.0F + pulse * 4.0F,
                     card.height + 8.0F + pulse * 4.0F},
                    0.11F, 8,
                    {kindColor.r, kindColor.g, kindColor.b,
                     static_cast<unsigned char>(70.0F * pulse)});
            }
            ui.drawInsetPanel(card, 238);
            DrawRectangleRoundedLinesEx(
                card, 0.09F, 8, pulsing ? 2.2F : 1.2F,
                {kindColor.r, kindColor.g, kindColor.b,
                 static_cast<unsigned char>(pulsing ? 235 : 115)});
            DrawRectangleRounded(
                {card.x + 3.0F, card.y + 7.0F,
                 4.0F, card.height - 14.0F},
                0.8F, 4, kindColor);
            drawObjectiveKindIcon(
                objective.definition.kind,
                {card.x + 25.0F, card.y + 27.0F},
                kindColor);

            const std::string eyebrow =
                std::string(objectiveKindLabel(
                    objective.definition.kind)) +
                "  •  " + objective.definition.title;
            const float eyebrowSize = fitUiTextSize(
                eyebrow, 11.0F, 9.0F,
                card.width - 165.0F, 17.0F);
            drawUiText(
                eyebrow,
                {card.x + 44.0F, card.y + 8.0F},
                eyebrowSize, kindColor);
            const std::string reward = "+" +
                std::to_string(static_cast<int>(std::lround(
                    objective.definition.insightReward))) +
                " INSIGHT";
            const float rewardWidth =
                measureUiText(reward, 10.0F).x;
            drawUiText(
                reward,
                {card.x + card.width - rewardWidth - 12.0F,
                 card.y + 9.0F},
                10.0F, {226, 207, 255, 245});

            const std::string instruction =
                objectiveInstruction(objective.definition);
            const float instructionSize = fitUiTextSize(
                instruction, 14.0F, 10.5F,
                card.width - 142.0F, 22.0F);
            drawUiText(
                instruction,
                {card.x + 44.0F, card.y + 31.0F},
                instructionSize, {247, 238, 215, 255});

            const int current = static_cast<int>(
                std::floor(std::min(
                    objective.progress,
                    objective.definition.target)));
            const int target = static_cast<int>(
                std::ceil(objective.definition.target));
            const std::string progress =
                std::to_string(current) + " / " +
                std::to_string(target);
            const float progressWidth =
                measureUiText(progress, 13.0F).x;
            drawUiText(
                progress,
                {card.x + card.width - progressWidth - 12.0F,
                 card.y + 33.0F},
                13.0F, {255, 239, 189, 255});
            const float fraction = objective.definition.target > 0.0
                ? std::clamp(static_cast<float>(
                      objective.progress /
                      objective.definition.target), 0.0F, 1.0F)
                : 0.0F;
            const Rectangle progressBar{
                card.x + 44.0F, card.y + 61.0F,
                card.width - 58.0F, 14.0F};
            ui.drawProgressBar(
                progressBar, fraction,
                objectiveBarColor(objective.definition.kind));
            if (fraction > 0.01F) {
                const float tipX = progressBar.x +
                    progressBar.width * fraction;
                const float shimmer = 0.55F + 0.45F *
                    std::sin(static_cast<float>(
                        snapshot.elapsedSeconds * 7.0) +
                        static_cast<float>(row));
                DrawCircleV(
                    {tipX, progressBar.y + progressBar.height * 0.5F},
                    pulsing ? 6.0F : 3.2F,
                    {kindColor.r, kindColor.g, kindColor.b,
                     static_cast<unsigned char>(150.0F +
                         shimmer * 95.0F)});
            }
            ++row;
        }
    } else if (!view.mapOverlayOpen &&
               objectiveCount > 0) {
        const float screenMinimum = static_cast<float>(
            std::min(GetScreenWidth(), GetScreenHeight()));
        const float compactMapSize = std::clamp(
            screenMinimum * 0.18F, 132.0F, 176.0F);
        constexpr float TrackerWidth = 344.0F;
        constexpr float RowHeight = 43.0F;
        const float trackerX = static_cast<float>(GetScreenWidth()) -
            TrackerWidth - 12.0F;
        const float trackerY = view.minimapHidden
            ? 18.0F
            : 30.0F + compactMapSize;
        drawUiText(
            "OBJECTIVES",
            {trackerX + 7.0F, trackerY},
            12.5F, {226, 208, 170, 225});
        int row = 0;
        for (int rawIndex : snapshot.recommendedObjectives) {
            if (rawIndex < 0 ||
                static_cast<std::size_t>(rawIndex) >=
                    snapshot.objectives.size()) {
                continue;
            }
            const ObjectiveStatus& objective =
                snapshot.objectives[
                    static_cast<std::size_t>(rawIndex)];
            const float rowY = trackerY + 22.0F +
                static_cast<float>(row) * RowHeight;
            const Color color = objectiveKindColor(
                objective.definition.kind);
            const bool pulsing =
                view.objectivePulse > 0.0 &&
                view.objectivePulseId == objective.definition.id;
            DrawRectangleRounded(
                {trackerX, rowY, TrackerWidth, RowHeight - 3.0F},
                0.18F, 5,
                {20, 18, 16,
                 static_cast<unsigned char>(pulsing ? 218 : 152)});
            drawObjectiveKindIcon(
                objective.definition.kind,
                {trackerX + 16.0F, rowY + 18.0F}, color);
            const std::string instruction =
                objectiveInstruction(objective.definition);
            const float instructionSize = fitUiTextSize(
                instruction, 12.5F, 9.5F, 210.0F, 18.0F);
            drawUiText(
                instruction,
                {trackerX + 32.0F, rowY + 6.0F},
                instructionSize, {242, 234, 214, 250});
            const int current = static_cast<int>(std::floor(
                std::min(objective.progress,
                         objective.definition.target)));
            const int target = static_cast<int>(std::ceil(
                objective.definition.target));
            const std::string progress =
                std::to_string(current) + "/" +
                std::to_string(target);
            const std::string reward = "+" +
                std::to_string(static_cast<int>(std::lround(
                    objective.definition.insightReward)));
            const float progressWidth =
                measureUiText(progress, 12.0F).x;
            const float rewardWidth =
                measureUiText(reward, 10.0F).x;
            drawUiText(
                progress,
                {trackerX + TrackerWidth - progressWidth - 9.0F,
                 rowY + 4.0F},
                12.0F, {255, 235, 176, 255});
            drawUiText(
                reward,
                {trackerX + TrackerWidth - rewardWidth - 22.0F,
                 rowY + 22.0F},
                10.0F, {195, 174, 245, 235});
            DrawPoly(
                {trackerX + TrackerWidth - 10.0F, rowY + 29.0F},
                4, 4.0F, 45.0F, {195, 174, 245, 235});
            const float fraction = objective.definition.target > 0.0
                ? std::clamp(static_cast<float>(
                      objective.progress /
                      objective.definition.target), 0.0F, 1.0F)
                : 0.0F;
            DrawRectangleRounded(
                {trackerX + 32.0F, rowY + 29.0F,
                 210.0F, 5.0F},
                0.8F, 3, {47, 41, 35, 210});
            DrawRectangleRounded(
                {trackerX + 32.0F, rowY + 29.0F,
                 210.0F * fraction, 5.0F},
                0.8F, 3, color);
            ++row;
        }
    }

    if (!view.mapOverlayOpen) {
        drawLootInventory(
            ui, snapshot, false,
            view.showCoreHealth);
    }

    bool chestCompassVisible = false;
    if (snapshot.nearestChestPosition &&
        snapshot.nearestChestDistance > 0.0) {
        constexpr std::array<std::string_view, 8> Directions{
            "N", "NE", "E", "SE", "S", "SW", "W", "NW"};
        constexpr double Octant = PI * 0.25;
        const double deltaX =
            snapshot.nearestChestPosition->x -
            snapshot.playerPosition.x;
        const double deltaZ =
            snapshot.nearestChestPosition->z -
            snapshot.playerPosition.z;
        double relative = std::atan2(deltaX, -deltaZ) -
            snapshot.playerYaw;
        while (relative <= -PI) relative += 2.0 * PI;
        while (relative > PI) relative -= 2.0 * PI;
        int directionIndex = static_cast<int>(std::lround(
            relative / Octant));
        directionIndex = (directionIndex % 8 + 8) % 8;
        const std::string compassText =
            "CHEST " + std::string(Directions[
                static_cast<std::size_t>(directionIndex)]) +
            "  " + std::to_string(static_cast<int>(
                std::lround(snapshot.nearestChestDistance))) + "m";
        const float compassWidth =
            std::max(188.0F,
                     measureUiText(compassText, 13.0F).x + 30.0F);
        ui.drawPanel({static_cast<float>(GetScreenWidth()) * 0.5F -
                          compassWidth * 0.5F,
                      82.0F, compassWidth, 31.0F}, 188);
        drawCenteredUiText(
            compassText, 89.0F, 13.0F,
            {255, 215, 116, 255});
        chestCompassVisible = true;
    }

    std::string phaseText;
    Color phaseColor{245, 184, 76, 255};
    if (snapshot.state == RunState::BuildPhase) {
        phaseText = "TO SUNSET  " +
            phaseClock(snapshot.phaseTimeRemaining);
        if (snapshot.earlyWaveBonus > 0) {
            phaseText += "  •  N: +" +
                std::to_string(snapshot.earlyWaveBonus) +
                " CRYSTALS";
            if (snapshot.earlyWaveCoinBonus > 0) {
                phaseText += "  +" +
                    std::to_string(
                        snapshot.earlyWaveCoinBonus) +
                    " COINS";
            }
            if (snapshot.earlyWaveInsightBonus > 0) {
                phaseText += "  +" +
                    std::to_string(
                        snapshot.earlyWaveInsightBonus) +
                    " INSIGHT";
            }
        }
    } else if (snapshot.state == RunState::Sunset) {
        phaseText = "TWILIGHT  •  BUILD / REPAIR  •  WAVE " +
            std::to_string(snapshot.wave + 1) + " IN " +
            phaseClock(snapshot.phaseTimeRemaining);
        phaseColor = {255, 146, 79, 255};
    } else if (snapshot.state == RunState::Wave) {
        phaseText = "WAVE " + std::to_string(snapshot.wave) +
            "     " +
            std::to_string(snapshot.activeEnemyCount +
                           snapshot.pendingEnemyCount) +
            " ENEMIES";
        phaseColor = {242, 108, 82, 255};
        const bool bossCharging = std::any_of(
            snapshot.enemies.begin(), snapshot.enemies.end(),
            [](const EnemyInstance& enemy) {
                return enemy.active &&
                       enemy.state == EnemyState::BossRamWindup;
            });
        if (bossCharging) {
            phaseText += "  •  BOSS RAM";
        }
    } else if (snapshot.state == RunState::WaveComplete) {
        phaseText = "DAWN  •  DAY IN " +
            phaseClock(snapshot.phaseTimeRemaining);
        phaseColor = {255, 194, 92, 255};
    }
    if (!phaseText.empty()) {
        constexpr float PhaseFontSize = 18.0F;
        const float textWidth = measureUiText(
            phaseText, PhaseFontSize).x;
        const float centerX = static_cast<float>(GetScreenWidth()) * 0.5F;
        DrawRectangleRounded(
            {centerX - textWidth * 0.5F - 20.0F,
             11.0F, textWidth + 40.0F, 42.0F},
            0.34F, 7, {25, 20, 18, 196});
        drawCenteredUiText(
            phaseText, 20.0F, PhaseFontSize, phaseColor);
    }

    if (snapshot.state == RunState::Sunset &&
        hasAttackDirections(snapshot) &&
        snapshot.phaseDuration > 0.0) {
        const double elapsed =
            snapshot.phaseDuration - snapshot.phaseTimeRemaining;
        if (elapsed < 3.2) {
            const float fade = std::clamp(
                static_cast<float>((3.2 - elapsed) / 0.65),
                0.0F, 1.0F);
            const std::string warning =
                "ATTACK FROM " + attackDirectionNames(snapshot);
            const float warningWidth =
                measureUiText(warning, 32.0F).x;
            DrawRectangleRounded(
                {static_cast<float>(GetScreenWidth()) * 0.5F -
                     warningWidth * 0.5F - 28.0F,
                 104.0F, warningWidth + 56.0F, 66.0F},
                0.22F, 8,
                {32, 10, 9,
                 static_cast<unsigned char>(fade * 190.0F)});
            drawCenteredUiText(
                warning, 113.0F, 32.0F,
                {255, 134, 88,
                 static_cast<unsigned char>(fade * 255.0F)});
        }
    }

    const std::string objective = view.mapOverlayOpen
        ? std::string{}
        : tutorialText(snapshot);
    if (!objective.empty()) {
        const bool attackWarningVisible =
            snapshot.state == RunState::Sunset &&
            hasAttackDirections(snapshot) &&
            snapshot.phaseDuration > 0.0 &&
            snapshot.phaseDuration - snapshot.phaseTimeRemaining < 3.2;
        constexpr float ObjectiveFontSize = 18.0F;
        const float objectiveY = attackWarningVisible
            ? 184.0F
            : chestCompassVisible ? 125.0F : 91.0F;
        const float objectiveWidth = measureUiText(
            objective, ObjectiveFontSize).x;
        const float centerX = static_cast<float>(GetScreenWidth()) * 0.5F;
        DrawRectangleRounded(
            {centerX - objectiveWidth * 0.5F - 16.0F,
             objectiveY - 6.0F, objectiveWidth + 32.0F, 34.0F},
            0.28F, 6, {24, 21, 17, 166});
        drawCenteredUiText(
            objective, objectiveY, ObjectiveFontSize,
            {255, 224, 146, 255});
    }

    const bool buildModeActive =
        view.actionMode == ActionMode::Buildings ||
        view.actionMode == ActionMode::Modular;
    drawCompactInsight(ui, snapshot, view, buildModeActive);
    if (!view.hideBottomHints && buildModeActive) {
        drawBuildHotbar(ui, snapshot, view);
    } else if (!view.hideBottomHints) {
        drawWeaponHotbar(ui, snapshot, view);
    }

    const float hitProgress =
        view.crosshairHitRemaining > 0.0 &&
                view.crosshairHitDuration > 0.0
            ? std::clamp(
                  static_cast<float>(
                      1.0 -
                      view.crosshairHitRemaining /
                          view.crosshairHitDuration),
                  0.0F, 1.0F)
            : 0.0F;
    const float hitPulse =
        view.crosshairHitRemaining > 0.0
            ? std::sin(hitProgress * PI)
            : 0.0F;
    const float invalidShake =
        view.invalidActionRemaining > 0.0
            ? std::sin(
                  static_cast<float>(GetTime()) * 92.0F) *
                  3.0F *
                  static_cast<float>(
                      std::min(
                          view.invalidActionRemaining / 0.22,
                          1.0))
            : 0.0F;
    const Vector2 crosshairCenter{
        static_cast<float>(GetScreenWidth()) * 0.5F +
            invalidShake,
        static_cast<float>(GetScreenHeight()) * 0.5F,
    };
    if (view.repairSweepActive) {
        drawCenteredUiText(
            "REPAIR MODE", crosshairCenter.y + 42.0F,
            17.0F, {104, 232, 163, 255});
    }
    const Color crosshairColor =
        view.invalidActionRemaining > 0.0
            ? Color{255, 76, 62, 255}
            : view.crosshairHitRemaining > 0.0 &&
                      view.crosshairHitCritical
                  ? Color{255, 215, 62, 255}
                  : RAYWHITE;
    const float gap =
        5.0F - hitPulse * 2.8F +
        std::clamp(view.weaponRecoilAmount, 0.0F, 1.0F) *
            5.5F;
    constexpr float ArmLength = 8.0F;
    DrawLineEx(
        {crosshairCenter.x - gap - ArmLength,
         crosshairCenter.y},
        {crosshairCenter.x - gap, crosshairCenter.y},
        2.2F, crosshairColor);
    DrawLineEx(
        {crosshairCenter.x + gap, crosshairCenter.y},
        {crosshairCenter.x + gap + ArmLength,
         crosshairCenter.y},
        2.2F, crosshairColor);
    DrawLineEx(
        {crosshairCenter.x, crosshairCenter.y - gap - ArmLength},
        {crosshairCenter.x, crosshairCenter.y - gap},
        2.2F, crosshairColor);
    DrawLineEx(
        {crosshairCenter.x, crosshairCenter.y + gap},
        {crosshairCenter.x,
         crosshairCenter.y + gap + ArmLength},
        2.2F, crosshairColor);
    if (view.crosshairHitRemaining > 0.0) {
        const float markerRadius =
            12.0F - hitPulse * 4.0F;
        const float markerHalf = 3.6F + hitPulse * 1.6F;
        const auto markerAlpha =
            static_cast<unsigned char>(
                std::lround(
                    (1.0F - hitProgress) * 255.0F));
        const Color markerColor{
            crosshairColor.r, crosshairColor.g,
            crosshairColor.b, markerAlpha};
        for (const Vector2 sign :
             std::array<Vector2, 4>{{
                 {-1.0F, -1.0F},
                 {1.0F, -1.0F},
                 {-1.0F, 1.0F},
                 {1.0F, 1.0F},
             }}) {
            const Vector2 outer{
                crosshairCenter.x +
                    sign.x * markerRadius,
                crosshairCenter.y +
                    sign.y * markerRadius,
            };
            const Vector2 inner{
                crosshairCenter.x +
                    sign.x *
                        (markerRadius - markerHalf),
                crosshairCenter.y +
                    sign.y *
                        (markerRadius - markerHalf),
            };
            DrawLineEx(outer, inner, 2.6F, markerColor);
        }
    }
    if (snapshot.selectedWeapon == PlayerWeapon::Rifle &&
        snapshot.rifleReloading &&
        snapshot.rifleReloadDuration > 0.0) {
        const float reloadProgress = std::clamp(
            static_cast<float>(
                1.0 -
                snapshot.rifleReloadRemaining /
                    snapshot.rifleReloadDuration),
            0.0F, 1.0F);
        DrawRing(
            crosshairCenter, 17.0F, 20.0F,
            -90.0F, 270.0F, 48,
            {35, 31, 27, 205});
        DrawRing(
            crosshairCenter, 17.0F, 20.0F,
            -90.0F,
            -90.0F + reloadProgress * 360.0F,
            48, {255, 205, 92, 245});
    }
    if (snapshot.dashUnlocked && snapshot.dashCooldownDuration > 0.0) {
        const float ready = std::clamp(
            static_cast<float>(
                1.0 - snapshot.dashCooldownRemaining /
                          snapshot.dashCooldownDuration),
            0.0F, 1.0F);
        const Color dashColor = snapshot.dashing
            ? Color{185, 242, 255, 255}
            : ready >= 0.999F
                  ? Color{91, 201, 244, 235}
                  : Color{77, 126, 151, 190};
        DrawRing(
            crosshairCenter, 25.0F, 27.0F,
            -90.0F, 270.0F, 48,
            {22, 35, 43, 170});
        DrawRing(
            crosshairCenter, 25.0F, 27.0F,
            -90.0F, -90.0F + ready * 360.0F,
            48, dashColor);
    }

    const float indicatorRadius =
        static_cast<float>(
            std::min(GetScreenWidth(), GetScreenHeight())) *
        0.31F;
    const Vector2 screenCenter{
        static_cast<float>(GetScreenWidth()) * 0.5F,
        static_cast<float>(GetScreenHeight()) * 0.5F,
    };
    for (const DamageIndicator& indicator :
         view.damageIndicators) {
        const float outwardX =
            static_cast<float>(std::sin(indicator.relativeAngle));
        const float outwardY =
            static_cast<float>(-std::cos(indicator.relativeAngle));
        const Vector2 center{
            screenCenter.x + outwardX * indicatorRadius,
            screenCenter.y + outwardY * indicatorRadius,
        };
        const Vector2 perpendicular{-outwardY, outwardX};
        const Vector2 tip{center.x + outwardX * 13.0F,
                          center.y + outwardY * 13.0F};
        const Vector2 baseLeft{
            center.x - outwardX * 9.0F +
                perpendicular.x * 9.0F,
            center.y - outwardY * 9.0F +
                perpendicular.y * 9.0F,
        };
        const Vector2 baseRight{
            center.x - outwardX * 9.0F -
                perpendicular.x * 9.0F,
            center.y - outwardY * 9.0F -
                perpendicular.y * 9.0F,
        };
        const double fade =
            indicator.remaining / indicator.duration;
        const auto alpha = static_cast<unsigned char>(
            220.0 * std::clamp(fade, 0.0, 1.0));
        const Color color =
            indicator.severe ? Color{255, 92, 42, alpha}
                             : Color{235, 62, 62, alpha};
        DrawTriangle(tip, baseLeft, baseRight, color);
    }

    if (view.statusMessageRemaining > 0.0 &&
        !view.statusMessage.empty()) {
        const std::string status{view.statusMessage};
        constexpr float StatusFontSize = 22.0F;
        const float statusWidth = measureUiText(
            status, StatusFontSize).x;
        const float statusY = static_cast<float>(
            GetScreenHeight() / 2 + 78);
        const float centerX = static_cast<float>(GetScreenWidth()) * 0.5F;
        DrawRectangleRounded(
            {centerX - statusWidth * 0.5F - 18.0F,
             statusY - 7.0F, statusWidth + 36.0F, 40.0F},
            0.3F, 7, {24, 20, 16, 198});
        drawCenteredUiText(
            status, statusY, StatusFontSize,
            {255, 194, 92, 255});
    }

    if (snapshot.buildingPreview) {
        const BuildingPreview& preview = *snapshot.buildingPreview;
        const ResourceCost cost = preview.placement.cost;
        const std::string buildText =
            std::string(buildingDisplayName(preview.type)) +
            "  W:" + std::to_string(cost.wood) +
            " S:" + std::to_string(cost.stone) +
            " C:" + std::to_string(cost.crystals);
        const Vec3 previewCenter = buildingWorldPosition(
            preview.type, preview.gridPosition);
        const Vector2 anchor = GetWorldToScreen(
            {static_cast<float>(previewCenter.x), 2.35F,
             static_cast<float>(previewCenter.z)},
            camera);
        constexpr float BuildFontSize = 18.0F;
        constexpr float MessageFontSize = 16.0F;
        const std::string placementText = placementMessage(
            preview.placement.error, preview.type);
        const float PanelWidth = std::clamp(
            std::max(measureUiText(buildText, BuildFontSize).x,
                     measureUiText(placementText, MessageFontSize).x) +
                44.0F,
            420.0F, 680.0F);
        constexpr float PanelHeight = 116.0F;
        const float panelX = std::clamp(
            anchor.x - PanelWidth * 0.5F, 18.0F,
            std::max(
                18.0F,
                static_cast<float>(GetScreenWidth()) -
                    PanelWidth - 18.0F));
        const float panelY = std::clamp(
            anchor.y - PanelHeight - 34.0F, 18.0F,
            std::max(
                18.0F,
                static_cast<float>(GetScreenHeight()) -
                    PanelHeight - 18.0F));
        ui.drawPanel(
            {panelX, panelY, PanelWidth, PanelHeight}, 236);
        drawUiText(
            buildText, {panelX + 18.0F, panelY + 14.0F},
            BuildFontSize, RAYWHITE);
        drawUiText(
            placementText,
            {panelX + 18.0F, panelY + 62.0F}, MessageFontSize,
            placementMessageColor(
                preview.placement.error));
    } else if (
        snapshot.aimedBuilding &&
        view.showBuildingContextCard) {
        const auto aimed = std::find_if(
            snapshot.buildings.begin(), snapshot.buildings.end(),
            [&snapshot](const BuildingInstance& building) {
                return building.id == *snapshot.aimedBuilding;
            });
        if (aimed != snapshot.buildings.end()) {
            drawBuildingContextCard(
                ui, snapshot, *aimed, view, camera, controls);
        }
    }

}

void drawRunStateOverlay(const SimulationSnapshot& snapshot) {
    if (snapshot.state == RunState::Paused) {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                      {0, 0, 0, 150});
        drawCenteredUiText(
            "PAUSED", static_cast<float>(GetScreenHeight() / 2 - 24),
            48.0F, RAYWHITE);
    } else if (snapshot.state == RunState::Defeat) {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                      {0, 0, 0, 170});
        drawCenteredUiText(
            "CORE DESTROYED",
            static_cast<float>(GetScreenHeight() / 2 - 48), 48.0F,
            {235, 92, 72, 255});
        drawCenteredUiText(
            "R: restart", static_cast<float>(GetScreenHeight() / 2 + 24),
            24.0F, RAYWHITE);
    } else if (snapshot.playerRespawning) {
        constexpr int PanelWidth = 460;
        constexpr int PanelHeight = 150;
        const int panelX =
            (GetScreenWidth() - PanelWidth) / 2;
        const int panelY =
            (GetScreenHeight() - PanelHeight) / 2;
        DrawRectangleRounded(
            {static_cast<float>(panelX),
             static_cast<float>(panelY),
             static_cast<float>(PanelWidth),
             static_cast<float>(PanelHeight)},
            0.12F, 8, {12, 14, 18, 224});

        char respawnText[64]{};
        std::snprintf(
            respawnText, sizeof(respawnText),
            "RESPAWN IN %.1f",
            snapshot.playerRespawnTimeRemaining);
        drawCenteredUiText(
            respawnText,
            static_cast<float>(panelY + 30), 34.0F,
            RAYWHITE);

        char lossText[128]{};
        std::snprintf(
            lossText, sizeof(lossText),
            "Lost  Wood %d   Stone %d   Crystals %d",
            snapshot.deathLostWood,
            snapshot.deathLostStone,
            snapshot.deathLostCrystals);
        drawCenteredUiText(
            lossText,
            static_cast<float>(panelY + 91), 19.0F,
            {235, 148, 112, 255});
    }
}

} // namespace ian
