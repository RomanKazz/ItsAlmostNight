#include "app/App.hpp"

#include "buildings/CoreProgression.hpp"
#include "buildings/BuildingSystem.hpp"
#include "ui/GameUi.hpp"
#include "ui/UiText.hpp"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace ian {
namespace {

constexpr float DesignWidth = 1920.0F;
constexpr float DesignHeight = 1080.0F;
// UiText accepts logical sizes and applies this project-wide multiplier
// internally. Figma measurements are already physical pixels.
constexpr float UiTextScale = 1.68F;

constexpr Color BodyColor{251, 229, 189, 255};
constexpr Color InsetColor{241, 224, 197, 255};
constexpr Color InsetWarmColor{251, 229, 189, 255};
constexpr Color InsetBorderColor{222, 199, 163, 255};
constexpr Color BrownColor{108, 61, 39, 255};
constexpr Color MutedTextColor{213, 197, 179, 255};
constexpr Color GreenTextColor{0, 241, 123, 255};
constexpr Color WarningTextColor{244, 102, 84, 255};

struct MenuTransform {
    float scale{1.0F};
    float offsetX{};
    float offsetY{};

    [[nodiscard]] Rectangle rect(
        float x, float y, float width, float height) const {
        return {
            offsetX + x * scale,
            offsetY + y * scale,
            width * scale,
            height * scale,
        };
    }

    [[nodiscard]] Vector2 point(float x, float y) const {
        return {offsetX + x * scale, offsetY + y * scale};
    }
};

Color withOpacity(Color color, float opacity) {
    color.a = static_cast<unsigned char>(std::lround(
        static_cast<float>(color.a) *
        std::clamp(opacity, 0.0F, 1.0F)));
    return color;
}

float smooth01(float value) {
    const float t = std::clamp(value, 0.0F, 1.0F);
    return t * t * (3.0F - 2.0F * t);
}

float easeOutBack(float value) {
    const float t = std::clamp(value, 0.0F, 1.0F) - 1.0F;
    constexpr float Overshoot = 1.35F;
    return 1.0F + (Overshoot + 1.0F) * t * t * t +
           Overshoot * t * t;
}

float intervalProgress(float seconds, float start, float end) {
    return smooth01((seconds - start) / std::max(end - start, 0.001F));
}

MenuTransform menuTransform(float animatedScale = 1.0F,
                            float slideY = 0.0F) {
    const float screenWidth = static_cast<float>(GetScreenWidth());
    const float screenHeight = static_cast<float>(GetScreenHeight());
    const float baseScale = std::min(
        screenWidth / DesignWidth, screenHeight / DesignHeight);
    const float scale = baseScale * animatedScale;
    return {
        .scale = scale,
        .offsetX = (screenWidth - DesignWidth * baseScale) * 0.5F +
            DesignWidth * 0.5F * (baseScale - scale),
        .offsetY = (screenHeight - DesignHeight * baseScale) * 0.5F +
            DesignHeight * 0.5F * (baseScale - scale) +
            slideY * baseScale,
    };
}

void drawTexture(Texture2D texture, Rectangle bounds,
                 Color tint = WHITE) {
    if (!IsTextureValid(texture)) return;
    DrawTexturePro(
        texture,
        {0.0F, 0.0F, static_cast<float>(texture.width),
         static_cast<float>(texture.height)},
        bounds, {}, 0.0F, tint);
}

void drawRoundedPanel(Rectangle bounds, float border,
                      float roundness, Color fill,
                      Color borderColor, float opacity = 1.0F) {
    DrawRectangleRounded(
        bounds, roundness, 20, withOpacity(borderColor, opacity));
    const Rectangle inner{
        bounds.x + border,
        bounds.y + border,
        std::max(0.0F, bounds.width - border * 2.0F),
        std::max(0.0F, bounds.height - border * 2.0F),
    };
    DrawRectangleRounded(
        inner, roundness, 20, withOpacity(fill, opacity));
}

void drawMenuText(std::string_view text, const MenuTransform& transform,
                  float x, float y, float size, Color color,
                  float opacity = 1.0F) {
    drawSdfUiText(text, transform.point(x, y),
                  size * transform.scale / UiTextScale,
                  withOpacity(color, opacity));
}

void drawArrow(const MenuTransform& transform, float centerX,
               float centerY, float opacity) {
    const float scale = transform.scale;
    const Vector2 center = transform.point(centerX, centerY);
    const Color outline = withOpacity({83, 47, 22, 255}, opacity);
    const Color fill = withOpacity({255, 190, 28, 255}, opacity);
    // Two convex primitives avoid the self-intersection artifacts produced
    // by a triangle fan over the arrow's concave silhouette.
    DrawRectangleRounded(
        {center.x - 16.0F * scale, center.y - 7.0F * scale,
         18.0F * scale, 14.0F * scale},
        0.35F, 8, outline);
    DrawTriangle(
        {center.x + 17.0F * scale, center.y},
        {center.x - 1.0F * scale, center.y - 14.0F * scale},
        {center.x - 1.0F * scale, center.y + 14.0F * scale},
        outline);
    DrawRectangleRounded(
        {center.x - 12.0F * scale, center.y - 4.0F * scale,
         16.0F * scale, 8.0F * scale},
        0.35F, 8, fill);
    DrawTriangle(
        {center.x + 11.0F * scale, center.y},
        {center.x + 1.0F * scale, center.y - 8.0F * scale},
        {center.x + 1.0F * scale, center.y + 8.0F * scale},
        fill);
}

struct StatRow {
    CoreMenuTexture icon;
    const char* label;
    int current;
    int next;
};

void drawStatRow(const GameUi& ui, const MenuTransform& transform,
                 const StatRow& row, float y, bool hasNextLevel,
                 float reveal, float numberProgress) {
    MenuTransform contentTransform = transform;
    contentTransform.offsetY +=
        (1.0F - reveal) * 10.0F * transform.scale;
    drawTexture(ui.coreMenuTexture(row.icon),
                contentTransform.rect(
                    462.0F, y - 4.0F, 45.0F, 45.0F),
                withOpacity(WHITE, reveal));
    drawMenuText(row.label, contentTransform, 519.0F, y, 40.0F,
                 MutedTextColor, reveal);
    drawMenuText(std::to_string(row.current), contentTransform,
                 768.0F, y, 40.0F, UiWhite, reveal);
    if (!hasNextLevel) {
        drawMenuText("MAX", contentTransform, 917.0F, y, 40.0F,
                     GreenTextColor, reveal);
        return;
    }
    drawArrow(contentTransform, 887.0F, y + 18.0F, reveal);
    const int animatedNext = static_cast<int>(std::lround(
        static_cast<float>(row.current) +
        static_cast<float>(row.next - row.current) * numberProgress));
    const float numberScale =
        0.88F + 0.12F * easeOutBack(numberProgress);
    drawMenuText(std::to_string(animatedNext), contentTransform,
                 934.0F, y - 4.0F + (1.0F - numberScale) * 20.0F,
                 48.0F * numberScale, GreenTextColor, reveal);
}

bool drawActionButton(
    const GameUi& ui, const MenuTransform& transform,
    Rectangle designBounds, CoreMenuTexture actionIcon,
    std::string_view label, CoreMenuTexture costIcon, int cost,
    bool enabled, Color darkFill, Color lightFill,
    float opacity) {
    Rectangle bounds = transform.rect(
        designBounds.x, designBounds.y,
        designBounds.width, designBounds.height);
    const bool hovered = enabled && opacity >= 0.98F &&
        CheckCollisionPointRec(GetMousePosition(), bounds);
    const bool pressed = hovered &&
        IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    const float scale = transform.scale;
    const float pressOffset = pressed ? 3.0F * scale : 0.0F;
    bounds.y += pressOffset;

    DrawRectangleRounded(bounds, 0.18F, 16,
                         withOpacity(
                             enabled ? Color{8, 16, 45, 255}
                                     : Color{48, 46, 43, 255},
                             opacity));
    Rectangle inner{
        bounds.x + 5.0F * scale,
        bounds.y + 5.0F * scale,
        bounds.width - 10.0F * scale,
        bounds.height - 10.0F * scale,
    };
    DrawRectangleRounded(inner, 0.16F, 16,
                         withOpacity(
                             enabled ? darkFill
                                     : Color{110, 103, 91, 255},
                             opacity));
    Rectangle highlight = inner;
    highlight.height = std::max(
        0.0F, highlight.height - 10.0F * scale);
    DrawRectangleRounded(highlight, 0.16F, 16,
                         withOpacity(
                             enabled ? lightFill
                                     : Color{142, 134, 118, 255},
                             opacity));
    if (hovered) {
        DrawRectangleRounded(inner, 0.16F, 16,
                             withOpacity(
                                 {255, 255, 255, 24}, opacity));
    }

    const Color tint = withOpacity(
        enabled ? WHITE : Color{190, 184, 170, 235}, opacity);
    drawTexture(ui.coreMenuTexture(actionIcon),
                {bounds.x + 15.0F * scale,
                 bounds.y + 16.0F * scale,
                 78.0F * scale, 78.0F * scale}, tint);
    drawSdfUiText(label,
                  {bounds.x + 105.0F * scale,
                   bounds.y + 35.0F * scale},
                  46.0F * scale / UiTextScale,
                  withOpacity(
                      enabled ? WHITE : Color{220, 214, 199, 255},
                      opacity));
    drawTexture(ui.coreMenuTexture(costIcon),
                {bounds.x + 386.0F * scale,
                 bounds.y + 31.0F * scale,
                 49.0F * scale, 49.0F * scale}, tint);
    drawSdfUiText(std::to_string(cost),
                  {bounds.x + 434.0F * scale,
                   bounds.y + 35.0F * scale},
                  46.0F * scale / UiTextScale,
                  withOpacity(
                      enabled ? WHITE : WarningTextColor, opacity));

    return hovered && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

struct UnlockCard {
    CoreMenuTexture icon;
    std::string_view label;
};

std::vector<UnlockCard> unlockCardsForLevel(std::uint8_t level) {
    std::vector<UnlockCard> cards;
    const auto addBuilding = [&](BuildingType type,
                                 CoreMenuTexture icon,
                                 std::string_view label) {
        if (buildingRequiredCoreLevel(type) == level) {
            cards.push_back({icon, label});
        }
    };
    addBuilding(BuildingType::Turret,
                CoreMenuTexture::Ballista, "CROSSBOW");
    addBuilding(BuildingType::GunTurret,
                CoreMenuTexture::Turret, "TURRET");
    addBuilding(BuildingType::CrystalMine,
                CoreMenuTexture::Gem, "CRYSTAL MINE");
    addBuilding(BuildingType::Cannon,
                CoreMenuTexture::Cannon, "CANNON");
    addBuilding(BuildingType::SlowTrap,
                CoreMenuTexture::Spikes, "SLOW TRAP");
    addBuilding(BuildingType::LumberMill,
                CoreMenuTexture::Wood, "LUMBER MILL");
    addBuilding(BuildingType::Quarry,
                CoreMenuTexture::Rock, "QUARRY");
    addBuilding(BuildingType::SpikeTrap,
                CoreMenuTexture::Spikes, "SPIKE TRAP");
    addBuilding(BuildingType::Catapult,
                CoreMenuTexture::Cannon, "CATAPULT");

    constexpr std::array Weapons{
        PlayerWeapon::Rifle,
        PlayerWeapon::Bomb,
        PlayerWeapon::IceWand,
        PlayerWeapon::FireWand,
    };
    for (const PlayerWeapon weapon : Weapons) {
        if (playerWeaponRequiredCoreLevel(weapon) != level) {
            continue;
        }
        switch (weapon) {
        case PlayerWeapon::Rifle:
            cards.push_back({CoreMenuTexture::Turret, "RIFLE"});
            break;
        case PlayerWeapon::Bomb:
            cards.push_back({CoreMenuTexture::Upgrade, "BOMBS"});
            break;
        case PlayerWeapon::IceWand:
            cards.push_back({CoreMenuTexture::Gem, "ICE WAND"});
            break;
        case PlayerWeapon::FireWand:
            cards.push_back({CoreMenuTexture::Upgrade, "FIRE WAND"});
            break;
        default:
            break;
        }
    }
    if (level == ModularBuildingRequiredCoreLevel) {
        cards.push_back({CoreMenuTexture::Gear, "MODULAR"});
    }
    if (level > 5U && level <= MaxBuildingLevel) {
        cards.push_back({CoreMenuTexture::Ballista, "MORE DEFENSES"});
        cards.push_back({CoreMenuTexture::Gear, "MORE PRODUCERS"});
        cards.push_back({CoreMenuTexture::Gem, "MORE STORAGE"});
    }
    return cards;
}

Rectangle scaledAroundCenter(Rectangle bounds, float scale) {
    const float width = bounds.width * scale;
    const float height = bounds.height * scale;
    return {
        bounds.x + (bounds.width - width) * 0.5F,
        bounds.y + (bounds.height - height) * 0.5F,
        width, height,
    };
}

void drawUnlockCard(const GameUi& ui, const MenuTransform& transform,
                    float x, float y, float width, float height,
                    const UnlockCard& unlock, float reveal) {
    const float popScale = 0.84F + 0.16F * easeOutBack(reveal);
    const Rectangle card = scaledAroundCenter(
        transform.rect(x, y, width, height), popScale);
    drawRoundedPanel(card, 6.0F * transform.scale, 0.16F,
                     InsetWarmColor, InsetBorderColor, reveal);
    const bool compact = height < 120.0F;
    const float iconSize = compact
        ? std::min(card.height - 14.0F * transform.scale,
                   68.0F * transform.scale)
        : 150.0F * transform.scale * popScale;
    drawTexture(
        ui.coreMenuTexture(unlock.icon),
        {card.x + (compact ? 7.0F : 15.0F) * transform.scale,
         card.y + (compact ? 7.0F : 7.0F) * transform.scale,
         iconSize, iconSize},
        withOpacity(WHITE, reveal));
    const Rectangle pill = compact
        ? Rectangle{
              card.x + 70.0F * transform.scale,
              card.y + 24.0F * transform.scale,
              std::max(0.0F,
                       card.width - 77.0F * transform.scale),
              38.0F * transform.scale}
        : Rectangle{
              card.x + 18.0F * transform.scale,
              card.y + 153.0F * transform.scale * popScale,
              144.0F * transform.scale * popScale,
              35.0F * transform.scale * popScale};
    DrawRectangleRounded(
        pill, 0.8F, 12, withOpacity(BrownColor, reveal));
    const float size = fitUiTextSize(
        unlock.label,
        (compact ? 20.0F : 24.0F) * transform.scale /
            UiTextScale,
        11.0F * transform.scale / UiTextScale,
        pill.width - 12.0F * transform.scale,
        pill.height - 4.0F * transform.scale);
    const Vector2 measured = measureUiText(unlock.label, size);
    drawSdfUiText(unlock.label,
                  {pill.x + (pill.width - measured.x) * 0.5F,
                   pill.y + (pill.height - measured.y) * 0.43F},
                  size, withOpacity(UiWhite, reveal));
}

} // namespace

void App::drawCoreDefenseMenu() {
    if (!coreDefenseMenuVisible_) return;
    const SimulationSnapshot& snapshot = simulation_.snapshot();
    if (!snapshot.coreId) {
        coreDefenseMenuVisible_ = false;
        coreDefenseMenuClosing_ = false;
        coreDefenseMenuEntranceSeconds_ = 0.0;
        return;
    }

    const auto coreIt = std::find_if(
        snapshot.buildings.begin(), snapshot.buildings.end(),
        [&snapshot](const BuildingInstance& building) {
            return building.id == *snapshot.coreId;
        });
    if (coreIt == snapshot.buildings.end()) {
        coreDefenseMenuVisible_ = false;
        coreDefenseMenuClosing_ = false;
        coreDefenseMenuEntranceSeconds_ = 0.0;
        return;
    }
    const BuildingInstance& core = *coreIt;
    if (coreDefenseMenuAnimatedLevel_ != core.level) {
        coreDefenseMenuAnimatedLevel_ = core.level;
    }
    constexpr double AnimationSeconds = 0.40;
    const double frameSeconds = static_cast<double>(
        std::clamp(GetFrameTime(), 0.0F, 0.10F));
    if (coreDefenseMenuClosing_) {
        coreDefenseMenuEntranceSeconds_ = std::max(
            0.0, coreDefenseMenuEntranceSeconds_ - frameSeconds);
        if (coreDefenseMenuEntranceSeconds_ <= 0.0) {
            coreDefenseMenuVisible_ = false;
            coreDefenseMenuClosing_ = false;
            coreDefenseMenuAnimatedLevel_ = 0;
            if (snapshot.state != RunState::Paused) {
                DisableCursor();
            }
            return;
        }
    } else {
        coreDefenseMenuEntranceSeconds_ = std::min(
            coreDefenseMenuEntranceSeconds_ + frameSeconds,
            AnimationSeconds);
    }
    const float entranceSeconds = static_cast<float>(
        coreDefenseMenuEntranceSeconds_);
    const float dimProgress = intervalProgress(
        entranceSeconds, 0.0F, 0.18F);
    const float panelProgress = intervalProgress(
        entranceSeconds, 0.0F, 0.28F);
    const float panelScale = easeOutBack(panelProgress);
    const float panelOpacity = intervalProgress(
        entranceSeconds, 0.0F, 0.12F);
    const float titleReveal = intervalProgress(
        entranceSeconds, 0.02F, 0.14F);
    const float bodyReveal = intervalProgress(
        entranceSeconds, 0.04F, 0.18F);
    const float buttonReveal = intervalProgress(
        entranceSeconds, 0.11F, 0.27F);
    const MenuTransform transform = menuTransform(
        panelScale, (1.0F - panelProgress) * 18.0F);
    const Rectangle menuBounds = transform.rect(
        276.0F, 39.0F, 1284.0F, 972.0F);
    if (!coreDefenseMenuClosing_ &&
        entranceSeconds > 0.05F &&
        IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
        !CheckCollisionPointRec(GetMousePosition(), menuBounds)) {
        coreDefenseMenuClosing_ = true;
        audio_.playUiConfirm();
    }
    const bool hasNextLevel = core.level < MaxBuildingLevel;
    const std::uint8_t nextLevel = hasNextLevel
        ? static_cast<std::uint8_t>(core.level + 1)
        : core.level;

    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                  {4, 8, 10, static_cast<unsigned char>(
                                  std::lround(175.0F * dimProgress))});
    if (panelScale <= 0.001F) {
        return;
    }

    drawRoundedPanel(transform.rect(407.0F, 375.0F,
                                    1100.0F, 636.0F),
                     10.0F * transform.scale, 0.07F,
                     BodyColor, BLACK, panelOpacity);
    drawTexture(ui_.coreMenuTexture(CoreMenuTexture::Header),
                transform.rect(276.0F, 39.0F, 1284.0F, 428.0F),
                withOpacity(WHITE, panelOpacity));

    drawMenuText("CORE", transform, 695.0F, 198.0F,
                 106.0F, Color{254, 247, 229, 255}, titleReveal);
    const Rectangle levelPill = transform.rect(
        689.0F, 297.0F, 250.0F, 60.0F);
    DrawRectangleRounded(
        levelPill, 0.8F, 16,
        withOpacity(BrownColor, titleReveal));
    drawMenuText("LEVEL " + std::to_string(core.level), transform,
                 723.0F, 307.0F, 48.0F,
                 Color{254, 247, 229, 255}, titleReveal);

    drawRoundedPanel(transform.rect(445.0F, 410.0F,
                                    598.0F, 445.0F),
                     6.0F * transform.scale, 0.08F,
                     InsetColor, InsetBorderColor, bodyReveal);
    drawRoundedPanel(transform.rect(1066.0F, 410.0F,
                                    403.0F, 445.0F),
                     6.0F * transform.scale, 0.08F,
                     InsetColor, InsetBorderColor, bodyReveal);

    const double healthStep = hasNextLevel
        ? core.maxHealth *
              ((1.0 + 0.15 * static_cast<double>(core.level)) /
               (1.0 + 0.15 * static_cast<double>(core.level - 1)))
        : core.maxHealth;
    const std::array<StatRow, 6> rows{
        StatRow{CoreMenuTexture::Heart, "HEALTH",
                static_cast<int>(std::lround(core.maxHealth)),
                static_cast<int>(std::lround(healthStep))},
        StatRow{CoreMenuTexture::Shield, "DEFENSES",
                buildingLimitForCoreLevel(BuildingType::Turret,
                                          core.level),
                buildingLimitForCoreLevel(BuildingType::Turret,
                                          nextLevel)},
        StatRow{CoreMenuTexture::Gear, "PRODUCERS",
                buildingLimitForCoreLevel(BuildingType::LumberMill,
                                          core.level),
                buildingLimitForCoreLevel(BuildingType::LumberMill,
                                          nextLevel)},
        StatRow{CoreMenuTexture::Wood, "WOOD",
                coreResourceCapacity(ResourceCapacityType::Wood,
                                     core.level),
                coreResourceCapacity(ResourceCapacityType::Wood,
                                     nextLevel)},
        StatRow{CoreMenuTexture::Rock, "STONE",
                coreResourceCapacity(ResourceCapacityType::Stone,
                                     core.level),
                coreResourceCapacity(ResourceCapacityType::Stone,
                                     nextLevel)},
        StatRow{CoreMenuTexture::Gem, "CRYSTAL",
                coreResourceCapacity(ResourceCapacityType::Crystal,
                                     core.level),
                coreResourceCapacity(ResourceCapacityType::Crystal,
                                     nextLevel)},
    };
    constexpr std::array RowY{
        451.0F, 516.0F, 582.0F, 647.0F, 712.0F, 777.0F};
    for (std::size_t index = 0; index < rows.size(); ++index) {
        const float rowReveal = intervalProgress(
            entranceSeconds,
            0.07F + static_cast<float>(index) * 0.015F,
            0.20F + static_cast<float>(index) * 0.015F);
        const float numberProgress = intervalProgress(
            entranceSeconds,
            0.10F + static_cast<float>(index) * 0.018F,
            0.30F + static_cast<float>(index) * 0.018F);
        drawStatRow(ui_, transform, rows[index], RowY[index],
                    hasNextLevel, rowReveal, numberProgress);
    }

    const Rectangle unlockPill = transform.rect(
        1086.0F, 400.0F, 364.0F, 49.0F);
    DrawRectangleRounded(
        unlockPill, 0.8F, 16,
        withOpacity(BrownColor, bodyReveal));
    drawMenuText(hasNextLevel
                     ? "LVL " + std::to_string(nextLevel) +
                           (nextLevel > 5U ? " EXPANDS" : " UNLOCKS")
                     : "MAX LEVEL",
                 transform, hasNextLevel ? 1122.0F : 1166.0F,
                 407.0F, 40.0F,
                 Color{254, 247, 229, 255}, bodyReveal);

    if (hasNextLevel) {
        const std::vector<UnlockCard> unlocks =
            unlockCardsForLevel(nextLevel);
        const bool compact = unlocks.size() > 4U;
        const float cardHeight = compact ? 84.0F : 175.0F;
        const float rowStep = compact ? 95.0F : 195.0F;
        for (std::size_t index = 0; index < unlocks.size(); ++index) {
            const bool centeredLast =
                index + 1U == unlocks.size() &&
                unlocks.size() % 2U != 0U;
            const float x = centeredLast
                ? 1176.0F
                : (index % 2U == 0U ? 1082.0F : 1270.0F);
            const float y = 459.0F +
                static_cast<float>(index / 2U) * rowStep;
            const float reveal = intervalProgress(
                entranceSeconds,
                0.10F + static_cast<float>(index) * 0.018F,
                0.24F + static_cast<float>(index) * 0.018F);
            drawUnlockCard(
                ui_, transform, x, y, 180.0F, cardHeight,
                unlocks[index], reveal);
        }
    }

    const ResourceCost upgradeCost = buildingUpgradeCost(core);
    const int coreUpgradeCost = upgradeCost.crystals;
    const bool canUpgrade = hasNextLevel &&
        (snapshot.unlimitedResources ||
         snapshot.crystals >= coreUpgradeCost);
    const bool hasDamagedBuilding =
        std::ranges::any_of(
            snapshot.buildings,
            [](const BuildingInstance& building) {
                return building.health < building.maxHealth;
            }) ||
        std::ranges::any_of(
            snapshot.platformFrames,
            [](const PlatformFrameInstance& building) {
                return building.health < building.maxHealth;
            }) ||
        std::ranges::any_of(
            snapshot.modularWalls,
            [](const WallInstance& building) {
                return building.health < building.maxHealth;
            }) ||
        std::ranges::any_of(
            snapshot.ramps,
            [](const RampInstance& building) {
                return building.health < building.maxHealth;
            });
    const bool canRepair = hasDamagedBuilding &&
        (snapshot.unlimitedResources ||
         snapshot.coins >= snapshot.repairAllCoinCost);

    const bool repairClicked = drawActionButton(
            ui_, transform,
            {445.0F, 866.0F, 504.0F, 119.0F},
            CoreMenuTexture::Wrench, "REPAIR ALL",
            CoreMenuTexture::Coin, snapshot.repairAllCoinCost,
            canRepair, Color{29, 80, 208, 255},
            Color{51, 146, 242, 255}, buttonReveal);
    if (!coreDefenseMenuClosing_ && repairClicked) {
        pendingRepairAllBuildings_ = true;
        audio_.playUiConfirm();
    }
    const bool upgradeClicked = drawActionButton(
            ui_, transform,
            {968.0F, 866.0F, 504.0F, 119.0F},
            CoreMenuTexture::Upgrade, "UPGRADE",
            CoreMenuTexture::Gem, coreUpgradeCost,
            canUpgrade, Color{16, 163, 70, 255},
            Color{64, 206, 90, 255}, buttonReveal);
    if (!coreDefenseMenuClosing_ && upgradeClicked) {
        pendingBuildingUpgrade_ = UpgradeBuildingCommand{core.id};
        audio_.playUiConfirm();
    }
}

} // namespace ian
