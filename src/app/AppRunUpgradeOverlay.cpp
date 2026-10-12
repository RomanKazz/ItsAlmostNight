#include "app/AppRunUpgradeOverlay.hpp"

#include "ui/UiText.hpp"
#include "ui/RunHammerUi.hpp"
#include "ui/RewardBackdrop.hpp"
#include "ui/UiSolidGeometry.hpp"
#include "ui/UiViewport.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

namespace ian::app_detail {
namespace {

constexpr float CardWidth = 360.0F;
constexpr float CardHeight = 548.0F;
constexpr float CardGap = 64.0F;
constexpr float UpgradeUiScale = 0.80F;
constexpr Color CardBrown{0x6E, 0x37, 0x21, 255};
constexpr Color CardBlack{0, 0, 0, 255};
constexpr Color CardShadowBlack{0, 0, 0, 64};
constexpr Color CardInset{0x34, 0x22, 0x15, 255};
constexpr Color CardInk{20, 21, 20, 255};
constexpr Color CardValueCream{0xF9, 0xE8, 0xCD, 255};
constexpr Color LevelText{0xBF, 0xAD, 0x9D, 255};
constexpr Color LevelOutline{0x3A, 0x25, 0x14, 255};
constexpr float UiTextRasterScale = 1.68F;

enum class UpgradeColorClass { Blue, Red, Yellow, Green, Purple };

struct UpgradePalette {
    Color border;
    Color borderShadow;
    Color badge;
    Color accent;
};

constexpr UpgradePalette upgradePalette(UpgradeColorClass colorClass) {
    switch (colorClass) {
    case UpgradeColorClass::Blue:
        return {
            {0x36, 0xE3, 0xF6, 255},
            {0x00, 0xA9, 0xDB, 255},
            {0x04, 0xA1, 0xD6, 255},
            {0x3E, 0xED, 0xFA, 255},
        };
    case UpgradeColorClass::Red:
        return {
            {0xE8, 0x50, 0x57, 255},
            {0xB8, 0x32, 0x42, 255},
            {0xA1, 0x33, 0x3D, 255},
            {0xFF, 0x40, 0x58, 255},
        };
    case UpgradeColorClass::Yellow:
        return {
            {0xFF, 0xB8, 0x12, 255},
            {0xD9, 0x86, 0x00, 255},
            {0xED, 0x95, 0x37, 255},
            {0xFF, 0xC2, 0x0D, 255},
        };
    case UpgradeColorClass::Green:
        return {
            {0x48, 0xB8, 0x79, 255},
            {0x16, 0x8B, 0x5B, 255},
            {0x07, 0x9B, 0x65, 255},
            {0x00, 0xC7, 0x7B, 255},
        };
    case UpgradeColorClass::Purple:
        return {
            {0xA3, 0x69, 0xD8, 255},
            {0x75, 0x3A, 0xB2, 255},
            {0x71, 0x36, 0xA8, 255},
            {0xB5, 0x5A, 0xF0, 255},
        };
    }
    return upgradePalette(UpgradeColorClass::Blue);
}

float upgradeStatsPanelWidth() {
    return std::clamp(static_cast<float>(GetScreenWidth()) * 0.19F, 220.0F, 340.0F);
}

float upgradeCardsAreaWidth() {
    return static_cast<float>(GetScreenWidth()) - upgradeStatsPanelWidth() - 48.0F;
}

float cardLayoutScale(std::size_t count) {
    const float screenHeight = static_cast<float>(GetScreenHeight());
    const float horizontal =
        (upgradeCardsAreaWidth() - 48.0F) /
        (CardWidth * static_cast<float>(count) +
         CardGap * static_cast<float>(count - 1U));
    const float vertical = (screenHeight - 350.0F) / CardHeight;
    return std::clamp(std::min(horizontal, vertical), 0.30F, UpgradeUiScale);
}

UpgradeColorClass upgradeColorClass(RunUpgradeEffect effect) {
    if (runUpgradeBuilding(effect) || runUpgradeWeapon(effect)) return UpgradeColorClass::Yellow;
    if (runUpgradeProductionBuilding(effect)) return UpgradeColorClass::Green;
    switch (effect) {
    case RunUpgradeEffect::Damage:
    case RunUpgradeEffect::AttackSpeed:
    case RunUpgradeEffect::MaximumHealth:
    case RunUpgradeEffect::BloodHarvest:
        return UpgradeColorClass::Red;
    case RunUpgradeEffect::MoveSpeed:
    case RunUpgradeEffect::Luck:
        return UpgradeColorClass::Blue;
    case RunUpgradeEffect::RecoverableArmor:
    case RunUpgradeEffect::BuildingHealth:
    case RunUpgradeEffect::BuildRadius:
    case RunUpgradeEffect::DefenseDamage:
    case RunUpgradeEffect::DefenseFireRate:
    case RunUpgradeEffect::CannonHeavyShell:
    case RunUpgradeEffect::CannonScattershot:
    case RunUpgradeEffect::CannonClusterShell:
    case RunUpgradeEffect::ClusterPayload:
    case RunUpgradeEffect::ReactiveTraps:
    case RunUpgradeEffect::GunPiercing:
    case RunUpgradeEffect::GunShrapnel:
        return UpgradeColorClass::Yellow;
    case RunUpgradeEffect::ProductionSpeed:
    case RunUpgradeEffect::GatheringSpeed:
    case RunUpgradeEffect::RichResources:
    case RunUpgradeEffect::NightlyBomb:
    case RunUpgradeEffect::LongerDay:
    case RunUpgradeEffect::Salvager:
        return UpgradeColorClass::Green;
    case RunUpgradeEffect::WiderChoice:
    case RunUpgradeEffect::DoubleDown:
    case RunUpgradeEffect::LockChoice:
    case RunUpgradeEffect::RerollToken:
    case RunUpgradeEffect::RiskyInvestment:
    case RunUpgradeEffect::Overkill:
    case RunUpgradeEffect::Ricochet:
    case RunUpgradeEffect::TwinBatteries:
        return UpgradeColorClass::Purple;
    case RunUpgradeEffect::UnlockGunTurret:
    case RunUpgradeEffect::UnlockTurret:
    case RunUpgradeEffect::UnlockCannon:
    case RunUpgradeEffect::UnlockCatapult:
    case RunUpgradeEffect::UnlockSpikeTrap:
    case RunUpgradeEffect::UnlockSlowTrap:
    case RunUpgradeEffect::UnlockLumberMill:
    case RunUpgradeEffect::UnlockQuarry:
    case RunUpgradeEffect::UnlockCrystalMine:
    case RunUpgradeEffect::LumberMillProduction:
    case RunUpgradeEffect::QuarryProduction:
    case RunUpgradeEffect::CrystalMineProduction:
    case RunUpgradeEffect::Regeneration:
    case RunUpgradeEffect::UnlockClub:
    case RunUpgradeEffect::UnlockRifle:
    case RunUpgradeEffect::UnlockBomb:
    case RunUpgradeEffect::UnlockIceWand:
    case RunUpgradeEffect::UnlockFireWand:
        break;
    }
    return UpgradeColorClass::Blue;
}

std::string_view upgradeMark(RunUpgradeEffect effect) {
    if (runUpgradeBuilding(effect) || runUpgradeWeapon(effect)) return "NEW";
    switch (effect) {
    case RunUpgradeEffect::Damage: return "DMG";
    case RunUpgradeEffect::AttackSpeed: return "SPD";
    case RunUpgradeEffect::MoveSpeed: return "MOVE";
    case RunUpgradeEffect::Luck: return "LUCK";
    case RunUpgradeEffect::MaximumHealth: return "HP";
    case RunUpgradeEffect::RecoverableArmor: return "DEF";
    case RunUpgradeEffect::BuildingHealth: return "BASE";
    case RunUpgradeEffect::BuildRadius: return "AREA";
    case RunUpgradeEffect::DefenseDamage: return "DEF";
    case RunUpgradeEffect::DefenseFireRate: return "RATE";
    case RunUpgradeEffect::ProductionSpeed: return "PROD";
    case RunUpgradeEffect::NightlyBomb: return "BOMB";
    case RunUpgradeEffect::WiderChoice: return "+1";
    case RunUpgradeEffect::BloodHarvest: return "BLOOD";
    case RunUpgradeEffect::Overkill: return "OVER";
    case RunUpgradeEffect::Ricochet: return "RICO";
    case RunUpgradeEffect::DoubleDown: return "X2";
    case RunUpgradeEffect::LockChoice: return "LOCK";
    case RunUpgradeEffect::RerollToken: return "ROLL";
    case RunUpgradeEffect::RiskyInvestment: return "RISK";
    case RunUpgradeEffect::Salvager: return "SCRAP";
    case RunUpgradeEffect::TwinBatteries: return "X2";
    case RunUpgradeEffect::CannonHeavyShell: return "HEAVY";
    case RunUpgradeEffect::CannonScattershot: return "SPREAD";
    case RunUpgradeEffect::CannonClusterShell: return "BURST";
    case RunUpgradeEffect::ClusterPayload: return "BURST";
    case RunUpgradeEffect::ReactiveTraps: return "PULSE";
    case RunUpgradeEffect::GunPiercing: return "PIERCE";
    case RunUpgradeEffect::GunShrapnel: return "SHARD";
    case RunUpgradeEffect::GatheringSpeed: return "TOOL";
    case RunUpgradeEffect::LongerDay: return "DAY";
    case RunUpgradeEffect::RichResources: return "NODE";
    case RunUpgradeEffect::UnlockGunTurret:
    case RunUpgradeEffect::UnlockTurret:
    case RunUpgradeEffect::UnlockCannon:
    case RunUpgradeEffect::UnlockCatapult:
    case RunUpgradeEffect::UnlockSpikeTrap:
    case RunUpgradeEffect::UnlockSlowTrap:
    case RunUpgradeEffect::UnlockLumberMill:
    case RunUpgradeEffect::UnlockQuarry:
    case RunUpgradeEffect::UnlockCrystalMine:
    case RunUpgradeEffect::LumberMillProduction:
    case RunUpgradeEffect::QuarryProduction:
    case RunUpgradeEffect::CrystalMineProduction:
    case RunUpgradeEffect::Regeneration:
    case RunUpgradeEffect::UnlockClub:
    case RunUpgradeEffect::UnlockRifle:
    case RunUpgradeEffect::UnlockBomb:
    case RunUpgradeEffect::UnlockIceWand:
    case RunUpgradeEffect::UnlockFireWand:
        break;
    }
    return "+";
}

float smoothUnit(float value) {
    const float clamped = std::clamp(value, 0.0F, 1.0F);
    return clamped * clamped * (3.0F - 2.0F * clamped);
}

float popEntranceScale(float value) {
    const float t = smoothUnit(value);
    constexpr float Overshoot = 1.35F;
    constexpr float ShiftedOvershoot = Overshoot + 1.0F;
    const float shifted = t - 1.0F;
    return 1.0F + ShiftedOvershoot * shifted * shifted * shifted +
        Overshoot * shifted * shifted;
}

Rectangle scaleRectangle(Rectangle bounds, float scale, float lift) {
    const float centerX = bounds.x + bounds.width * 0.5F;
    const float centerY = bounds.y + bounds.height * 0.5F - lift;
    bounds.width *= scale;
    bounds.height *= scale;
    bounds.x = centerX - bounds.width * 0.5F;
    bounds.y = centerY - bounds.height * 0.5F;
    return bounds;
}

Color fadedColor(Color color, float opacity) {
    color.a = static_cast<unsigned char>(std::lround(
        static_cast<float>(color.a) *
        std::clamp(opacity, 0.0F, 1.0F)));
    return color;
}

float rayRandom(std::uint32_t index, std::uint32_t salt) {
    std::uint32_t bits = (index + 1U) * 0x9e3779b9U ^ salt;
    bits ^= bits >> 16U;
    bits *= 0x7feb352dU;
    bits ^= bits >> 15U;
    bits *= 0x846ca68bU;
    bits ^= bits >> 16U;
    return static_cast<float>(bits >> 8U) / 16777216.0F;
}

// Each slot respawns a stationary rectangular beam after its fade/shrink
// finishes. Hashes provide reproducible randomness without mutable RNG state
// or frame-rate-dependent births; neighbouring slots may overlap freely.
void drawUpgradeRays(Rectangle offers, double seconds, float fade, bool shop=false, bool reduceFlashes=false) {
    if (fade <= 0) return;
    const Vector2 center{offers.x + offers.width * 0.5F, offers.y + offers.height * 0.52F};
    const float screenRadius = std::hypot(static_cast<float>(uiScreenWidth()),
        static_cast<float>(uiScreenHeight())) * 0.72F;
    const float scale = std::min(1.0F,static_cast<float>(uiScreenHeight())/1080.0F);
    constexpr std::uint32_t rayCount = 28;
    for (std::uint32_t slot = 0; slot < rayCount; ++slot) {
        const double lifetime = (2.4 + rayRandom(slot, 0xbb67aeU) * 2.2) * (2.0 / 3.0) *
            (reduceFlashes ? 2.0 : 1.0);
        const double elapsed = std::max(0.0, seconds) + rayRandom(slot, 0x6a09e6U) * lifetime;
        const auto cycle = static_cast<std::uint32_t>(std::fmod(std::floor(elapsed / lifetime), 4294967296.0));
        const float age = static_cast<float>(std::fmod(elapsed, lifetime) / lifetime);
        const std::uint32_t seed = slot ^ (cycle * 0x9e3779b9U);
        const float angle = rayRandom(seed, 0x51f15eU) * (2 * PI);
        constexpr float fadeInFraction = 0.15F;
        // Brief appearance at full width, then one shared curve shrinks and
        // fades the beam together. Nothing remains at the final width.
        const float remaining = 1 - smoothUnit((age - fadeInFraction) / (1 - fadeInFraction));
        const float opacity = age < fadeInFraction ? smoothUnit(age / fadeInFraction) : remaining;
        const float widthSample = rayRandom(seed, 0x3c6ef3U);
        const float maxWidth = (12 + 268 * widthSample * widthSample) * scale;
        const float endWidthRatio = 0.15F + rayRandom(seed, 0x1f83d9U) * 0.15F;
        const float widthRatio = endWidthRatio + (1 - endWidthRatio) * remaining;
        const float halfWidth = maxWidth * widthRatio * 0.5F;
        const float length = screenRadius * (0.72F + rayRandom(seed, 0xa54ff5U) * 0.38F);
        const Vector2 direction{std::cos(angle), std::sin(angle)};
        const Vector2 side{-direction.y * halfWidth, direction.x * halfWidth};
        const Vector2 end{center.x + direction.x * length, center.y + direction.y * length};
        const std::array<Vector2, 4> points{{
            {center.x - side.x, center.y - side.y}, {center.x + side.x, center.y + side.y},
            {end.x + side.x, end.y + side.y}, {end.x - side.x, end.y - side.y}}};
        const bool bright=rayRandom(seed, 0x510e52U)<0.4F;
        const Color color=shop ? (bright?Color{255,212,105,255}:Color{255,168,51,255}) :
            (bright?Color{255,249,184,255}:Color{255,235,142,255});
        drawSolidTriangleFan(points.data(), static_cast<int>(points.size()), fadedColor(color, fade * opacity * (shop?.30F:.90F) * (reduceFlashes?.5F:1.0F)));
    }
}

Rectangle expandedRectangle(Rectangle bounds, float amount) {
    return {
        bounds.x - amount,
        bounds.y - amount,
        bounds.width + amount * 2.0F,
        bounds.height + amount * 2.0F,
    };
}

float roundedRectangleRoundness(Rectangle bounds, float radius) {
    return std::clamp(
        radius * 2.0F / std::min(bounds.width, bounds.height),
        0.0F, 1.0F);
}

void drawOutlinedCenteredText(
    std::string_view text, float centerX, float y, float fontSize,
    Color fill, float opacity, float scale,
    Color outline = CardInk, float outlinePixels = 3.2F,
    bool shadowEnabled = true) {
    const Vector2 measured = measureUiText(text, fontSize);
    SdfUiTextStyle style{
        .fillColor = fadedColor(fill, opacity),
        .outlineColor = fadedColor(outline, opacity),
        .shadowColor = shadowEnabled
            ? fadedColor({0, 0, 0, 210}, opacity) : BLANK,
        .shadowOffsetPixels = shadowEnabled
            ? Vector2{3.0F * scale, 5.0F * scale} : Vector2{},
        .outlinePixels = std::clamp(
            outlinePixels * scale, 1.8F, 4.0F),
        .softness = 0.68F,
    };
    drawStyledSdfUiText(
        text, {centerX - measured.x * 0.5F, y}, fontSize, style);
}

void drawRasterOutlinedCenteredText(
    const std::string& text, Vector2 center, float fontSize,
    float outlineRadius, Color fill, Color outline, float opacity) {
    const Font font = uiFont();
    const float spacing = fontSize * 0.02F;
    const Vector2 measured = MeasureTextEx(
        font, text.c_str(), fontSize, spacing);
    const Vector2 position{
        center.x - measured.x * 0.5F,
        center.y - measured.y * 0.5F,
    };
    const int radius = std::max(
        1, static_cast<int>(std::ceil(outlineRadius)));
    const float radiusSquared = outlineRadius * outlineRadius;
    // The outline is built from many overlapping text stamps. Fading every
    // stamp linearly makes their tiny rounded alphas accumulate into an
    // almost opaque black silhouette after the fill has nearly vanished.
    // Fade the redundant stamps faster so the complete glyph disappears
    // cleanly with its card.
    const float outlineOpacity = opacity * opacity * opacity;
    for (int y = -radius; y <= radius; ++y) {
        for (int x = -radius; x <= radius; ++x) {
            const float distanceSquared =
                static_cast<float>(x * x + y * y);
            if (distanceSquared > radiusSquared) continue;
            DrawTextEx(
                font, text.c_str(),
                {position.x + static_cast<float>(x),
                 position.y + static_cast<float>(y)},
                fontSize, spacing,
                fadedColor(outline, outlineOpacity));
        }
    }
    DrawTextEx(
        font, text.c_str(), position, fontSize, spacing,
        fadedColor(fill, opacity));
}

float fitRasterTextSize(
    const std::string& text, float preferredSize,
    float minimumSize, float maximumWidth) {
    const Font font = uiFont();
    float size = preferredSize;
    while (size > minimumSize) {
        const float spacing = size * 0.02F;
        if (MeasureTextEx(
                font, text.c_str(), size, spacing).x <= maximumWidth) {
            break;
        }
        size = std::max(minimumSize, size - 1.0F);
    }
    return size;
}

void drawLevelArrow(Vector2 center, float scale, Color color,
                    float opacity) {
    const Color outline = fadedColor(LevelOutline, opacity);
    const Color fill = fadedColor(color, opacity);
    DrawRectangleRounded(
        {center.x - 16.0F * scale, center.y - 8.0F * scale,
         20.0F * scale, 16.0F * scale},
        0.70F, 6, outline);
    DrawTriangle(
        {center.x + 16.0F * scale, center.y},
        {center.x + 1.0F * scale, center.y - 12.0F * scale},
        {center.x + 1.0F * scale, center.y + 12.0F * scale},
        outline);
    DrawRectangleRounded(
        {center.x - 13.0F * scale, center.y - 5.0F * scale,
         17.0F * scale, 10.0F * scale},
        0.70F, 6, fill);
    DrawTriangle(
        {center.x + 12.0F * scale, center.y},
        {center.x + 1.0F * scale, center.y - 8.0F * scale},
        {center.x + 1.0F * scale, center.y + 8.0F * scale},
        fill);
}

struct UpgradeValueCopy {
    std::string value;
    std::string label;
};

std::string statNumber(double value, int precision = 1) {
    std::ostringstream text;
    text << std::fixed << std::setprecision(precision) << value;
    std::string result = text.str();
    if (result.find('.') != std::string::npos) {
        while (result.back() == '0') result.pop_back();
        if (result.back() == '.') result.pop_back();
    }
    return result;
}

std::string upgradeDescription(const SimulationSnapshot& snapshot,
    RunUpgradeEffect effect, RunUpgradeRarity rarity) {
    if (runDefenseCard(effect)) {
        return snapshot.runUpgradeStacks[runUpgradeIndex(effect)] > 0
            ? "Upgrade every copy by one level; new copies inherit it"
            : "Unlock level 1 construction; uses one defense slot";
    }
    if (const auto weapon=runUpgradeWeapon(effect); weapon && runWeaponLevelCard(effect)) {
        return snapshot.unlockedWeapons[static_cast<std::size_t>(*weapon)]
            ? "Upgrade weapon by one level" : "Unlock level 1 weapon";
    }
    if (effect == RunUpgradeEffect::Damage || effect == RunUpgradeEffect::AttackSpeed) {
        const double gain = runUpgradeAppliedPowerGain(effect, rarity, snapshot.runUpgradePower[runUpgradeIndex(effect)]);
        const bool damage = effect == RunUpgradeEffect::Damage;
        return "+" + statNumber((damage ? 10.0 : 15.0) * gain) + "% " +
            localizeText(damage ? "Player damage" : "Attack speed") + " / +" +
            statNumber((damage ? 12.0 : 10.0) * gain) + "% " +
            localizeText(damage ? "Defense damage" : "Fire rate");
    }
    std::string description{runUpgradeDefinition(effect).description};
    if (fixedRunUpgradePower(effect)) return description;
    const std::size_t numberEnd = description.find_first_not_of("0123456789.", 1);
    if (numberEnd == std::string::npos || numberEnd <= 1) return description;
    const double power = runUpgradeAppliedPowerGain(
        effect, rarity, snapshot.runUpgradePower[runUpgradeIndex(effect)]);
    double amount = std::stod(description.substr(1, numberEnd - 1)) * power;
    if (effect == RunUpgradeEffect::BuildRadius) amount = std::round(amount);
    description.replace(1, numberEnd - 1, statNumber(amount, effect == RunUpgradeEffect::Regeneration ? 2 : 1));
    return description;
}

UpgradePalette rarityPalette(RunUpgradeRarity rarity) {
    switch (rarity) {
    case RunUpgradeRarity::Common: return upgradePalette(UpgradeColorClass::Green);
    case RunUpgradeRarity::Uncommon: return upgradePalette(UpgradeColorClass::Blue);
    case RunUpgradeRarity::Rare: return upgradePalette(UpgradeColorClass::Purple);
    case RunUpgradeRarity::Epic: return upgradePalette(UpgradeColorClass::Red);
    case RunUpgradeRarity::Legendary: return upgradePalette(UpgradeColorClass::Yellow);
    }
    return upgradePalette(UpgradeColorClass::Green);
}

void drawUpgradeStats(const SimulationSnapshot& snapshot, float fade,
                      std::optional<std::size_t> hoveredChoice) {
    const auto projected = hoveredChoice ? previewRunUpgrade(snapshot, *hoveredChoice) : snapshot;
    const bool cannonPreview = hoveredChoice &&
        runUpgradeCannonStyle(snapshot.runUpgradeChoices[*hoveredChoice]).has_value();
    const auto hoveredEffect = hoveredChoice ? snapshot.runUpgradeChoices[*hoveredChoice] : RunUpgradeEffect::Damage;
    const bool contextualPreview = hoveredChoice && (
        hoveredEffect == RunUpgradeEffect::ReactiveTraps || hoveredEffect == RunUpgradeEffect::GunShrapnel ||
        hoveredEffect == RunUpgradeEffect::BloodHarvest || hoveredEffect == RunUpgradeEffect::Overkill ||
        hoveredEffect == RunUpgradeEffect::Salvager || hoveredEffect == RunUpgradeEffect::NightlyBomb ||
        hoveredEffect == RunUpgradeEffect::LongerDay || hoveredEffect == RunUpgradeEffect::DoubleDown ||
        hoveredEffect == RunUpgradeEffect::RiskyInvestment || hoveredEffect == RunUpgradeEffect::RerollToken);
    const Rectangle bounds = runUpgradeStatsPanelBounds(snapshot.runUpgradeChoiceCount);
    DrawRectangleRounded(bounds, 0.08F, 8, fadedColor({24, 28, 27, 245}, fade));
    DrawRectangleRoundedLinesEx(bounds, 0.08F, 8, 2.0F,
        fadedColor({126, 112, 85, 255}, fade));
    const float scale = bounds.height / 765.0F;
    const float fontSize = std::min(23.0F * scale, bounds.width * 0.068F) / UiTextRasterScale;
    const float left = bounds.x + 18.0F;
    const float right = bounds.x + bounds.width - 18.0F;
    float y = bounds.y + 18.0F * scale;
    const float titleSize = fitUiTextSize("STATS",
        28.0F / UiTextRasterScale * scale, 16.0F / UiTextRasterScale * scale,
        right - left);
    drawUiText("STATS", {left, y}, titleSize, fadedColor(UiWhite, fade));
    if (hoveredChoice) {
        const ScopedUiFontRole body(UiFontRole::BodyBold);
        const float previewSize = fitUiTextSize("PREVIEW", fontSize, fontSize * 0.8F, bounds.width * 0.43F);
        const float previewWidth = measureUiText("PREVIEW", previewSize).x;
        drawUiText("PREVIEW", {right - previewWidth, y + 28.0F * scale}, previewSize,
            fadedColor({110, 229, 164, 255}, fade));
    }
    y += 45.0F * scale;
    const auto section = [&](std::string_view name) {
        y += 8.0F * scale;
        drawUiText(name, {left, y}, fontSize, fadedColor({255, 198, 94, 255}, fade));
        y += 30.0F * scale;
    };
    const auto row = [&](std::string_view label, const std::string& value,
                         const std::string& nextValue, bool positive = true) {
        const bool changed = value != nextValue;
        const Color changeColor = positive ? Color{110, 229, 164, 255} : Color{255, 173, 105, 255};
        float valueWidth{};
        {
            const ScopedUiFontRole valueFont(UiFontRole::BodyBold);
            float valueSize = fontSize;
            const auto width = [&](float size) {
                return measureUiText(value, size).x + (changed
                    ? measureUiText(" > ", size).x + measureUiText(nextValue, size).x : 0.0F);
            };
            const float available = (right - left) * (changed ? 0.53F : 0.46F);
            if (width(valueSize) > available) valueSize *= available / width(valueSize);
            valueWidth = width(valueSize);
            if (changed) {
                DrawRectangleRounded({left - 5.0F, y - 3.0F * scale,
                    right - left + 10.0F, 26.0F * scale}, 0.20F, 4,
                    fadedColor({48, 67, 53, 150}, fade));
                const float nextWidth = measureUiText(nextValue, valueSize).x;
                const float arrowWidth = measureUiText(" > ", valueSize).x;
                drawUiText(value, {right - valueWidth, y}, valueSize,
                    fadedColor({169, 178, 169, 255}, fade));
                drawUiText(" > ", {right - nextWidth - arrowWidth, y}, valueSize,
                    fadedColor({173, 187, 172, 255}, fade));
                drawUiText(nextValue, {right - nextWidth, y}, valueSize,
                    fadedColor(changeColor, fade));
            } else {
                drawUiText(value, {right - valueWidth, y}, valueSize, fadedColor(UiWhite, fade));
            }
        }
        const ScopedUiFontRole labelFont(UiFontRole::Body);
        const float labelSize = fitUiTextSize(label, fontSize, fontSize * 0.72F,
            bounds.width - 36.0F - valueWidth - 14.0F);
        drawUiText(label, {left, y}, labelSize,
            fadedColor(changed ? changeColor : Color{205, 207, 196, 255}, fade));
        y += 27.0F * scale;
    };
    const auto multiplier = [](double value) { return statNumber(value, 2) + "x"; };
    const RunUpgradeStats& stats = snapshot.upgradeStats;
    const RunUpgradeStats& next = projected.upgradeStats;
    section("DEFENSE");
    row("Arsenal", std::to_string(snapshot.buildingArsenalCount) + "/" + std::to_string(RunBuildingSlotCount),
        std::to_string(projected.buildingArsenalCount) + "/" + std::to_string(RunBuildingSlotCount));
    row("Defense damage", multiplier(stats.defenseDamage), multiplier(next.defenseDamage));
    row("Fire rate", multiplier(stats.defenseFireRate), multiplier(next.defenseFireRate));
    row("Building HP", multiplier(stats.buildingHealth), multiplier(next.buildingHealth));
    row("Side shots", std::to_string(stats.sideShots), std::to_string(next.sideShots));
    row("Gun piercing", std::to_string(stats.gunPiercing), std::to_string(next.gunPiercing));
    row("Shot bounces", std::to_string(stats.ricochets), std::to_string(next.ricochets));
    row("Cluster blasts", std::to_string(stats.clusterExplosions), std::to_string(next.clusterExplosions));
    const auto cannonStyleName = [](CannonAttackStyle style) {
        switch (style) {
        case CannonAttackStyle::Heavy: return "HEAVY";
        case CannonAttackStyle::Scatter: return "SPREAD";
        case CannonAttackStyle::Cluster: return "CLUSTER";
        case CannonAttackStyle::Standard: return "STANDARD";
        }
        return "STANDARD";
    };
    row("Cannon attack", cannonStyleName(stats.cannonAttackStyle), cannonStyleName(next.cannonAttackStyle));
    section("ECONOMY");
    row("Production", multiplier(stats.productionSpeed), multiplier(next.productionSpeed));
    row("Resource node capacity", multiplier(stats.resourceCapacity), multiplier(next.resourceCapacity));
    row("Build range", std::to_string(snapshot.coreBuildRadius) + "m", std::to_string(projected.coreBuildRadius) + "m");
    row("Luck", statNumber(stats.luck * 100.0) + "%", statNumber(next.luck * 100.0) + "%");
    section("PLAYER");
    if (snapshot.runUpgradePower[runUpgradeIndex(RunUpgradeEffect::PoisonChance)] > 0 ||
        projected.runUpgradePower[runUpgradeIndex(RunUpgradeEffect::PoisonChance)] > 0) {
        row("Poison chance", statNumber(runPoisonChance(snapshot.runUpgradePower[runUpgradeIndex(RunUpgradeEffect::PoisonChance)]) * 100) + "%",
            statNumber(runPoisonChance(projected.runUpgradePower[runUpgradeIndex(RunUpgradeEffect::PoisonChance)]) * 100) + "%");
    }
    row("Max HP", statNumber(snapshot.playerMaxHealth), statNumber(projected.playerMaxHealth));
    row("HP regen", statNumber(stats.healthRegeneration, 2) + "/s", statNumber(next.healthRegeneration, 2) + "/s");
    row("Shield", statNumber(snapshot.playerMaxRecoverableArmor), statNumber(projected.playerMaxRecoverableArmor));
    row("Player damage", multiplier(snapshot.playerDamageMultiplier), multiplier(projected.playerDamageMultiplier));
    row("Attack speed", multiplier(stats.playerAttackSpeed), multiplier(next.playerAttackSpeed));
    if (snapshot.runUpgradePower[runUpgradeIndex(RunUpgradeEffect::GatheringSpeed)] > 0 ||
        projected.runUpgradePower[runUpgradeIndex(RunUpgradeEffect::GatheringSpeed)] > 0)
        row("Tool speed", multiplier(stats.toolAttackSpeed), multiplier(next.toolAttackSpeed));
    row("Move speed", multiplier(snapshot.playerMoveSpeedMultiplier), multiplier(projected.playerMoveSpeedMultiplier));
    if (cannonPreview) {
        const auto before = cannonAttackProfile(stats.cannonAttackStyle);
        const auto after = cannonAttackProfile(next.cannonAttackStyle);
        const auto* character = playerClassDefinition(snapshot.playerClass);
        const double damage = stats.defenseDamage * (character && character->startingBuilding == BuildingType::Cannon
            ? character->startingBuildingDamageMultiplier : 1.0);
        const double rate = stats.defenseFireRate * (character && character->startingBuilding == BuildingType::Cannon
            ? character->startingBuildingFireRateMultiplier : 1.0);
        row("Cannon shell damage", multiplier(damage * before.damage), multiplier(damage * after.damage), after.damage >= before.damage);
        if (next.cannonAttackStyle == CannonAttackStyle::Scatter) {
            row("Shells per shot", std::to_string(before.projectileCount), std::to_string(after.projectileCount));
        } else if (next.cannonAttackStyle == CannonAttackStyle::Cluster) {
            row("Cannon mini blasts", std::to_string(before.clusterCount + stats.clusterExplosions),
                std::to_string(after.clusterCount + next.clusterExplosions));
        } else {
            row("Cannon fire rate", multiplier(rate / before.interval), multiplier(rate / after.interval), after.interval <= before.interval);
        }
    }
    if (hoveredChoice && runUpgradeProductionBuilding(hoveredEffect)) {
        const auto index = runUpgradeIndex(hoveredEffect);
        const auto label = hoveredEffect == RunUpgradeEffect::LumberMillProduction ? "Lumber mill speed"
            : hoveredEffect == RunUpgradeEffect::QuarryProduction ? "Quarry speed" : "Crystal mine speed";
        row(label, multiplier(stats.productionSpeed * (1.0 + 0.20 * snapshot.runUpgradePower[index])),
            multiplier(next.productionSpeed * (1.0 + 0.20 * projected.runUpgradePower[index])));
    }
    if (contextualPreview) {
        const auto index = runUpgradeIndex(hoveredEffect);
        const double before = snapshot.runUpgradePower[index];
        const double after = projected.runUpgradePower[index];
        const auto bonus = [&](std::string_view label, double step, std::string_view suffix = "") {
            row(label, statNumber(before * step) + std::string(suffix), statNumber(after * step) + std::string(suffix));
        };
        switch (hoveredEffect) {
        case RunUpgradeEffect::ReactiveTraps: bonus("Trap pulse", 6.0); break;
        case RunUpgradeEffect::GunShrapnel: bonus("Kill shards", 3.0); break;
        case RunUpgradeEffect::BloodHarvest: bonus("HP per 10 kills", 8.0); break;
        case RunUpgradeEffect::Overkill: bonus("Damage transfers", 1.0); break;
        case RunUpgradeEffect::Salvager: bonus("Salvage bonus", 30.0, "%"); break;
        case RunUpgradeEffect::NightlyBomb: bonus("Bombs per night", 1.0); break;
        case RunUpgradeEffect::LongerDay: bonus("Day extension", 5.0, "s"); break;
        case RunUpgradeEffect::DoubleDown: row("Next-level bonus", "-", "+2"); break;
        case RunUpgradeEffect::RiskyInvestment: row("Enemy difficulty", "-", "INCREASED", false); break;
        case RunUpgradeEffect::RerollToken: row("Rerolls", std::to_string(snapshot.runUpgradeRerollTokens),
            std::to_string(snapshot.runUpgradeRerollTokens + 1)); break;
        default: break;
        }
    }

}

UpgradeValueCopy upgradeValueCopy(
    std::string_view description, std::string_view fallback) {
    if (description.size() >= 2U &&
        (description.front() == '+' || description.front() == '-') &&
        std::isdigit(static_cast<unsigned char>(description[1]))) {
        const std::size_t separator = description.find(' ');
        if (separator != std::string_view::npos) {
            std::string label{description.substr(separator + 1U)};
            if (!label.empty()) label.front() = static_cast<char>(
                std::toupper(static_cast<unsigned char>(label.front())));
            if (!label.empty() && label.back() == '.') label.pop_back();
            return {
                std::string{description.substr(0U, separator)},
                std::move(label),
            };
        }
    }
    return {std::string{fallback}, "Special upgrade"};
}

} // namespace

Rectangle runUpgradeCardBounds(std::size_t index, std::size_t count) {
    const float screenWidth = upgradeCardsAreaWidth();
    const float screenHeight = static_cast<float>(GetScreenHeight());
    const float layoutScale = cardLayoutScale(count);
    const float cardWidth = CardWidth * layoutScale;
    const float cardHeight = CardHeight * layoutScale;
    const float gap = CardGap * layoutScale;
    const float totalWidth = cardWidth * static_cast<float>(count) +
        gap * static_cast<float>(count - 1U);
    const float desiredY =
        screenHeight * 0.5F - cardHeight * 0.5F + 125.0F;
    return {
        (screenWidth - totalWidth) * 0.5F +
            static_cast<float>(index) * (cardWidth + gap),
        std::clamp(desiredY, 186.0F,
                   std::max(186.0F, screenHeight - cardHeight - 58.0F)),
        cardWidth,
        cardHeight,
    };
}

Rectangle runUpgradeStatsPanelBounds(std::size_t count) {
    const float width = upgradeStatsPanelWidth();
    const float height = std::min(630.0F, static_cast<float>(GetScreenHeight()) - 80.0F);
    const Rectangle card = runUpgradeCardBounds(0, count);
    const float center = card.y + card.height * 0.5F;
    return {static_cast<float>(GetScreenWidth()) - width - 24.0F,
        std::clamp(center - height * 0.5F, 40.0F,
            static_cast<float>(GetScreenHeight()) - height - 40.0F), width, height};
}

std::optional<std::size_t> hoveredRunUpgradeChoice(
    const SimulationSnapshot& snapshot, Vector2 mousePosition) {
    if (!snapshot.runUpgradeChoicePending) return std::nullopt;
    for (std::size_t index = 0; index < snapshot.runUpgradeChoiceCount;
         ++index) {
        if (CheckCollisionPointRec(
                mousePosition,
                runUpgradeCardBounds(index,
                                     snapshot.runUpgradeChoiceCount))) {
            return index;
        }
    }
    return std::nullopt;
}

SimulationSnapshot previewRunUpgrade(const SimulationSnapshot& snapshot, std::size_t choiceIndex) {
    SimulationSnapshot projected = snapshot;
    if (choiceIndex >= snapshot.runUpgradeChoiceCount || choiceIndex >= MaximumRunUpgradeChoices) return projected;
    const auto effect = snapshot.runUpgradeChoices[choiceIndex];
    const auto index = runUpgradeIndex(effect);
    if (index >= RunUpgradeEffectCount ||
        static_cast<std::size_t>(snapshot.runUpgradeRarities[choiceIndex]) >= RunUpgradeRarityCount) return projected;
    const double current = snapshot.runUpgradePower[index];
    const double gain = runUpgradeAppliedPowerGain(effect, snapshot.runUpgradeRarities[choiceIndex], current);
    if (gain <= 0.0) return projected;
    projected.runUpgradePower[index] += gain;
    ++projected.runUpgradeStacks[index];
    if (effect == RunUpgradeEffect::Damage || effect == RunUpgradeEffect::AttackSpeed) {
        const auto defenseHalf = effect == RunUpgradeEffect::Damage ? RunUpgradeEffect::DefenseDamage : RunUpgradeEffect::DefenseFireRate;
        const double oldPower = snapshot.runUpgradePower[runUpgradeIndex(defenseHalf)];
        projected.runUpgradePower[runUpgradeIndex(defenseHalf)] += gain;
        if (effect == RunUpgradeEffect::Damage)
            projected.upgradeStats.defenseDamage *= (1.0 + 0.12 * (oldPower + gain)) / (1.0 + 0.12 * oldPower);
        else
            projected.upgradeStats.defenseFireRate *= (1.0 + 0.10 * (oldPower + gain)) / (1.0 + 0.10 * oldPower);
    }
    projected.tomeCount = runTomeCount(projected.runUpgradeStacks);
    const auto* character = playerClassDefinition(snapshot.playerClass);
    const auto ratio = [&](double base, double step) {
        return (base + step * (current + gain)) / (base + step * current);
    };
    if (runDefenseCard(effect) && snapshot.runUpgradeStacks[runUpgradeIndex(effect)] == 0)
        ++projected.buildingArsenalCount;
    if (const auto weapon = runUpgradeWeapon(effect)) {
        const auto weaponIndex=static_cast<std::size_t>(*weapon);
        projected.weaponLevels[weaponIndex]=snapshot.unlockedWeapons[weaponIndex]
            ? std::min(MaximumPlayerWeaponLevel,std::max(1,snapshot.weaponLevels[weaponIndex])+1) : 1;
        projected.unlockedWeapons[weaponIndex] = true;
        if (*weapon == PlayerWeapon::Bomb) projected.bombsRemaining += snapshot.bombPurchaseAmount;
    }
    if (const auto style = runUpgradeCannonStyle(effect)) projected.upgradeStats.cannonAttackStyle = *style;
    switch (effect) {
    case RunUpgradeEffect::DefenseDamage: projected.upgradeStats.defenseDamage *= (1.0 + 0.12 * (current + gain)) / (1.0 + 0.12 * current); break;
    case RunUpgradeEffect::DefenseFireRate: projected.upgradeStats.defenseFireRate *= (1.0 + 0.10 * (current + gain)) / (1.0 + 0.10 * current); break;
    case RunUpgradeEffect::BuildingHealth:
        projected.upgradeStats.buildingHealth *= ratio(character ? character->buildingHealthMultiplier : 1.0, 0.12); break;
    case RunUpgradeEffect::ProductionSpeed: projected.upgradeStats.productionSpeed *= ratio(1.0, 0.15); break;
    case RunUpgradeEffect::ResourceCapacity:
        projected.upgradeStats.resourceCapacity *= ratio(1.0, .20);
        break;
    case RunUpgradeEffect::AttackSpeed:
        projected.upgradeStats.playerAttackSpeed *= ratio(character ? character->attackSpeedMultiplier : 1.0, 0.15); break;
    case RunUpgradeEffect::Damage: projected.playerDamageMultiplier *= ratio(1.0, 0.10); break;
    case RunUpgradeEffect::MoveSpeed:
        projected.playerMoveSpeedMultiplier *= ratio(character ? character->moveSpeedMultiplier : 1.0, 0.07); break;
    case RunUpgradeEffect::MaximumHealth: projected.playerMaxHealth += 12.0 * gain; break;
    case RunUpgradeEffect::RecoverableArmor: projected.playerMaxRecoverableArmor += 8.0 * gain; break;
    case RunUpgradeEffect::BuildRadius: projected.coreBuildRadius += static_cast<int>(std::round(3.0 * gain)); break;
    case RunUpgradeEffect::GatheringSpeed: projected.upgradeStats.toolAttackSpeed *= ratio(1.0, 0.15); break;
    case RunUpgradeEffect::Luck: projected.upgradeStats.luck += 0.10 * gain; break;
    case RunUpgradeEffect::Regeneration: projected.upgradeStats.healthRegeneration += 0.5 * gain; break;
    case RunUpgradeEffect::TwinBatteries:
        projected.upgradeStats.sideShots = runUpgradeDiscreteAmount(effect, current + gain); break;
    case RunUpgradeEffect::GunPiercing:
        projected.upgradeStats.gunPiercing = runUpgradeDiscreteAmount(effect, current + gain); break;
    case RunUpgradeEffect::Ricochet:
        projected.upgradeStats.ricochets = runUpgradeDiscreteAmount(effect, current + gain); break;
    case RunUpgradeEffect::ClusterPayload:
        projected.upgradeStats.clusterExplosions = runUpgradeDiscreteAmount(effect, current + gain); break;
    default: break;
    }
    return projected;
}

void drawRunUpgradeOverlay(
    GameUi& ui,
    const SimulationSnapshot& snapshot,
    double entranceSeconds,
    double exitSeconds,
    std::optional<std::size_t> selectedChoice,
    std::span<const float> hoverAmounts, std::optional<Vector2> pointerPosition, bool reduceFlashes) {
    if (!snapshot.runUpgradeChoicePending ||
        snapshot.runUpgradeChoiceCount == 0U) return;
    const float exitProgress = std::clamp(static_cast<float>(
        exitSeconds / RunUpgradeOverlayExitDuration), 0.0F, 1.0F);
    const float exitEase = smoothUnit(exitProgress);
    const float overlayFade = 1.0F - exitEase;
    const float backgroundExitFade = 1.0F - smoothUnit(std::clamp(
        (exitProgress - 0.72F) / 0.28F, 0.0F, 1.0F));
    const float screenFade = smoothUnit(static_cast<float>(
        entranceSeconds / 0.20)) * backgroundExitFade;
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                  {30, 30, 30, static_cast<unsigned char>(
                      std::lround(170.0F * screenFade))});
    reward_backdrop::draw(ui,reward_backdrop::Kind::Experience,entranceSeconds+exitSeconds,
        {static_cast<float>(uiScreenWidth()),static_cast<float>(uiScreenHeight())},screenFade,reduceFlashes);
    const Vector2 mouse = pointerPosition.value_or(GetMousePosition());
    drawRunHammerInventory(ui, snapshot, {20.0F, 64.0F, std::min(600.0F, GetScreenWidth() * 0.4F), 44.0F}, overlayFade, mouse);
    const auto hoveredChoice = exitSeconds > 0.0 ? std::nullopt : hoveredRunUpgradeChoice(snapshot, mouse);
    drawUpgradeStats(snapshot, smoothUnit(static_cast<float>(entranceSeconds / 0.28)) * overlayFade, hoveredChoice);
    const float headingEntrance = std::clamp(static_cast<float>(
        (entranceSeconds - 0.01) / 0.23), 0.0F, 1.0F);
    const float headingMotion = popEntranceScale(headingEntrance);
    const Rectangle firstCard = runUpgradeCardBounds(
        0U, snapshot.runUpgradeChoiceCount);
    const float cardTop = firstCard.y;
    const Texture2D levelUpRays = ui.levelUpRaysTexture();
    if (IsTextureValid(levelUpRays)) {
        const Rectangle raysAnchorCard = runUpgradeCardBounds(
            std::min<std::size_t>(
                1U, snapshot.runUpgradeChoiceCount - 1U),
            snapshot.runUpgradeChoiceCount);
        const Vector2 raysCenter{
            raysAnchorCard.x + raysAnchorCard.width * 0.5F,
            raysAnchorCard.y + raysAnchorCard.height * 0.5F,
        };
        const float raysSize = std::max(
            static_cast<float>(GetScreenWidth()),
            static_cast<float>(GetScreenHeight())) * 0.90F;
        DrawTexturePro(
            levelUpRays,
            {0.0F, 0.0F, static_cast<float>(levelUpRays.width),
             static_cast<float>(levelUpRays.height)},
            {raysCenter.x, raysCenter.y, raysSize, raysSize},
            {raysSize * 0.5F, raysSize * 0.5F},
            static_cast<float>(GetTime()) * 2.0F,
            fadedColor(WHITE, screenFade * 0.10F));
    }
    const Texture2D levelUpBanner = ui.levelUpBannerTexture();
    if (IsTextureValid(levelUpBanner)) {
        const float maximumBannerWidth = std::max(
            180.0F, (cardTop - 64.0F) * 3.0F);
        const float bannerWidth = UpgradeUiScale * 0.82F * std::min({
            940.0F,
            static_cast<float>(GetScreenWidth()) - 60.0F,
            maximumBannerWidth,
        });
        const float bannerHeight = bannerWidth *
            static_cast<float>(levelUpBanner.height) /
            static_cast<float>(levelUpBanner.width);
        const float bannerScale =
            headingMotion * (1.0F - exitEase * 0.06F);
        const Rectangle bannerBase{
            (upgradeCardsAreaWidth() - bannerWidth) * 0.5F,
            cardTop - bannerHeight - 64.0F -
                (1.0F - smoothUnit(headingEntrance)) * 44.0F -
                exitEase * 28.0F,
            bannerWidth, bannerHeight,
        };
        const Rectangle banner = scaleRectangle(
            bannerBase, bannerScale, 0.0F);
        if (bannerScale > 0.001F) {
            DrawTexturePro(
                levelUpBanner,
                {0.0F, 0.0F, static_cast<float>(levelUpBanner.width),
                 static_cast<float>(levelUpBanner.height)},
                banner, {0.0F, 0.0F}, 0.0F,
                fadedColor(WHITE, overlayFade));
        }
    }

    for (std::size_t index = 0; index < snapshot.runUpgradeChoiceCount;
        ++index) {
        const RunUpgradeEffect effect = snapshot.runUpgradeChoices[index];
        const RunUpgradeRarity rarity = snapshot.runUpgradeRarities[index];
        const std::string_view title = runUpgradeDefinition(effect).name;
        const std::string description = upgradeDescription(snapshot, effect, rarity);
        const Rectangle baseBounds = runUpgradeCardBounds(
            index, snapshot.runUpgradeChoiceCount);
        const bool hovered = CheckCollisionPointRec(mouse, baseBounds);
        const float rawHover = index < hoverAmounts.size()
            ? std::clamp(hoverAmounts[index], 0.0F, 1.0F)
            : hovered ? 1.0F : 0.0F;
        const float hover = exitProgress > 0.0F ? 0.0F : rawHover;
        const double cardDelay = 0.055 +
            static_cast<double>(index) * 0.055;
        const float entrance = std::clamp(static_cast<float>(
            (entranceSeconds - cardDelay) / 0.26), 0.0F, 1.0F);
        const float entranceScale = popEntranceScale(entrance);
        if (entranceScale <= 0.001F) continue;
        const float entranceFade = smoothUnit(entrance);
        const bool selectedForExit = selectedChoice &&
            *selectedChoice == index;
        // Several opaque layers overlap inside each card. Fade them faster
        // near the end so their combined opacity reaches zero smoothly.
        const float cardExitFade = overlayFade * overlayFade;
        const float cardFade =
            smoothUnit(entrance * 2.5F) * cardExitFade;
        const float exitScale = selectedForExit
            ? 1.0F + exitEase * 0.13F
            : 1.0F - exitEase * 0.10F;
        const float animationScale = entranceScale *
            (1.0F + hover * 0.045F) * exitScale;
        const float layoutScale = baseBounds.height / CardHeight;
        const float lift =
            ((1.0F - entranceFade) * -55.0F + hover * 10.0F +
             (selectedForExit ? exitEase * 30.0F
                              : -exitEase * 24.0F)) *
            layoutScale;
        Rectangle bounds = scaleRectangle(
            baseBounds, animationScale, lift);
        const float slotFromCenter = static_cast<float>(index) -
            static_cast<float>(snapshot.runUpgradeChoiceCount - 1U) * 0.5F;
        bounds.x += slotFromCenter * 18.0F *
            (1.0F - entranceFade) * layoutScale;
        if (selectedChoice && !selectedForExit) {
            const float exitDirection = index < *selectedChoice
                ? -1.0F : 1.0F;
            bounds.x += exitDirection * exitEase *
                52.0F * layoutScale;
        } else if (selectedForExit) {
            const float selectedCenterX =
                bounds.x + bounds.width * 0.5F;
            bounds.x +=
                (upgradeCardsAreaWidth() * 0.5F -
                 selectedCenterX) * exitEase;
        }
        const float scale = layoutScale * animationScale;
        const bool locked = snapshot.lockedRunUpgrade == effect;
        const UpgradePalette palette = rarityPalette(rarity);
        const Color accent = palette.accent;
        const Rectangle colorBounds = expandedRectangle(
            bounds, 10.0F * scale);
        Rectangle colorShadowBounds = colorBounds;
        colorShadowBounds.y += 4.0F * scale;
        const Rectangle blackOutlineBounds = expandedRectangle(
            colorBounds, 7.0F * scale);
        Rectangle blackShadowOutlineBounds = blackOutlineBounds;
        blackShadowOutlineBounds.y += 4.0F * scale;
        Rectangle cardDropShadowBounds = blackShadowOutlineBounds;
        cardDropShadowBounds.y += 6.0F * scale;

        const float glowStrength = cardFade *
            (0.50F + hover * 0.65F +
             (selectedForExit ? exitEase * 0.35F : 0.0F));
        const auto drawCardGlowLayer = [&](float expansion,
                                           float opacity) {
            const Rectangle glowBounds = expandedRectangle(
                blackOutlineBounds, expansion * scale);
            DrawRectangleRounded(
                glowBounds,
                roundedRectangleRoundness(
                    glowBounds,
                    (57.0F + expansion) * scale),
                8,
                fadedColor(
                    palette.border,
                    glowStrength * opacity));
        };
        drawCardGlowLayer(38.0F, 0.018F);
        drawCardGlowLayer(30.0F, 0.026F);
        drawCardGlowLayer(23.0F, 0.036F);
        drawCardGlowLayer(16.0F, 0.050F);
        drawCardGlowLayer(10.0F, 0.072F);

        const Vector2 badgeGlowCenter{
            bounds.x + bounds.width * 0.5F,
            bounds.y - 20.0F * scale,
        };
        DrawCircleV(
            badgeGlowCenter, 75.0F * scale,
            fadedColor(palette.border, glowStrength * 0.022F));
        DrawCircleV(
            badgeGlowCenter, 68.0F * scale,
            fadedColor(palette.border, glowStrength * 0.040F));
        DrawCircleV(
            badgeGlowCenter, 62.0F * scale,
            fadedColor(palette.border, glowStrength * 0.070F));

        DrawRectangleRounded(
            cardDropShadowBounds,
            roundedRectangleRoundness(
                cardDropShadowBounds, 57.0F * scale),
            8, fadedColor(CardShadowBlack, cardFade));
        DrawRectangleRounded(
            blackShadowOutlineBounds,
            roundedRectangleRoundness(
                blackShadowOutlineBounds, 57.0F * scale),
            8, fadedColor(CardBlack, cardFade));
        DrawRectangleRounded(
            blackOutlineBounds,
            roundedRectangleRoundness(
                blackOutlineBounds, 57.0F * scale),
            8, fadedColor(CardBlack, cardFade));
        DrawRectangleRounded(
            colorShadowBounds,
            roundedRectangleRoundness(
                colorShadowBounds, 50.0F * scale),
            8, fadedColor(palette.borderShadow, cardFade));
        DrawRectangleRounded(
            colorBounds,
            roundedRectangleRoundness(colorBounds, 50.0F * scale),
            8, fadedColor(palette.border, cardFade));
        DrawRectangleRounded(
            bounds,
            roundedRectangleRoundness(bounds, 40.0F * scale),
            8, fadedColor(CardBrown, cardFade));

        const float centerX = bounds.x + bounds.width * 0.5F;
        const Vector2 emblemCenter{
            centerX, bounds.y + 128.0F * scale};

        const std::string_view mark = upgradeMark(effect);
        const auto iconBuilding = runUpgradeBuilding(effect) ? runUpgradeBuilding(effect)
            : runUpgradeProductionBuilding(effect);
        const Texture2D cardIcon = iconBuilding ? ui.buildingTexture(*iconBuilding) : ui.runUpgradeTexture(effect);
        const bool drawUpgradeIcon = IsTextureValid(cardIcon);
        if (const auto* hammer = runHammerDefinition(effect)) {
            const float iconSize = 235.0F * scale;
            drawRunHammerIcon(ui, {centerX - iconSize * 0.5F, bounds.y + 16.0F * scale,
                iconSize, iconSize}, *hammer, cardFade);
        } else if (drawUpgradeIcon) {
            const float iconSize = 256.0F * scale;
            DrawTexturePro(
                cardIcon,
                {0.0F, 0.0F, static_cast<float>(cardIcon.width),
                 static_cast<float>(cardIcon.height)},
                {centerX - iconSize * 0.5F,
                 bounds.y - 4.0F * scale,
                 iconSize, iconSize},
                {0.0F, 0.0F}, 0.0F,
                fadedColor(WHITE, cardFade));
        } else {
            const float emblemRadius = 85.0F * scale;
            DrawCircleV(
                emblemCenter, emblemRadius + 8.0F * scale,
                fadedColor(CardInk, cardFade));
            DrawCircleV(
                emblemCenter, emblemRadius,
                fadedColor(upgradePalette(upgradeColorClass(effect)).accent, cardFade));
            DrawCircleV(
                {emblemCenter.x - 18.0F * scale,
                 emblemCenter.y - 19.0F * scale},
                74.0F * scale,
                fadedColor({255, 255, 255, 34}, cardFade));
            const float markFontSize = fitUiTextSize(
                mark, 37.0F * scale, 18.0F * scale,
                emblemRadius * 1.55F);
            drawOutlinedCenteredText(
                mark, centerX,
                emblemCenter.y - measureUiText(mark, markFontSize).y * 0.5F,
                markFontSize, UiWhite, cardFade, scale);
        }

        const Vector2 numberCenter{
            centerX, bounds.y - 20.0F * scale};
        DrawCircleV(
            {numberCenter.x, numberCenter.y + 6.0F * scale},
            55.0F * scale,
            fadedColor(CardShadowBlack, cardFade));
        DrawCircleV(
            numberCenter, 55.0F * scale,
            fadedColor(CardBlack, cardFade));
        DrawCircleV(
            numberCenter, 48.0F * scale,
            fadedColor(palette.border, cardFade));
        DrawCircleV(
            numberCenter, 38.0F * scale,
            fadedColor(palette.badge, cardFade));
        const std::string key = std::to_string(index + 1U);
        Vector2 keyCenter{
            numberCenter.x,
            numberCenter.y + 4.0F * scale,
        };
        if (key == "1") keyCenter.x -= 2.0F * scale;
        drawRasterOutlinedCenteredText(
            key, keyCenter, 64.0F * scale, 5.0F * scale,
            WHITE, CardBlack, cardFade);

        if (locked) {
            const Rectangle lockBadge{
                bounds.x + bounds.width - 72.0F * scale,
                bounds.y + 20.0F * scale,
                54.0F * scale, 26.0F * scale};
            DrawRectangleRounded(
                lockBadge, 0.30F, 6,
                fadedColor({38, 24, 18, 235}, cardFade));
            const Vector2 lockSize = measureUiText(
                "LOCK", 11.0F * scale);
            drawSdfUiText(
                "LOCK",
                {lockBadge.x + (lockBadge.width - lockSize.x) * 0.5F,
                 lockBadge.y + 5.0F * scale},
                11.0F * scale, fadedColor(accent, cardFade));
        }

        std::string_view displayTitle = title;
        if (!runHammerDefinition(effect) &&
            (effect == RunUpgradeEffect::MoveSpeed ||
             effect == RunUpgradeEffect::MaximumHealth ||
             effect == RunUpgradeEffect::RecoverableArmor)) {
            displayTitle = mark;
        }
        const std::string displayTitleText{displayTitle};
        const float titleSize = 50.0F * scale;
        const float titleWidth = bounds.width - 40.0F * scale;
        const auto rasterWidth = [&](std::string_view value) {
            const std::string copy{value};
            return MeasureTextEx(uiFont(), copy.c_str(), titleSize, titleSize * 0.02F).x;
        };
        std::size_t split = std::string::npos;
        if (rasterWidth(displayTitleText) > titleWidth) {
            float bestWidth = rasterWidth(displayTitleText);
            for (auto space = displayTitleText.find(' '); space != std::string::npos;
                 space = displayTitleText.find(' ', space + 1)) {
                const float lineWidth = std::max(rasterWidth(displayTitle.substr(0, space)),
                    rasterWidth(displayTitle.substr(space + 1)));
                if (lineWidth < bestWidth) { bestWidth = lineWidth; split = space; }
            }
        }
        if (split != std::string::npos) {
            const std::string firstLine = displayTitleText.substr(0, split);
            const std::string secondLine = displayTitleText.substr(split + 1);
            const float commonSize = std::min(fitRasterTextSize(firstLine, titleSize, 38.0F * scale, titleWidth),
                fitRasterTextSize(secondLine, titleSize, 38.0F * scale, titleWidth));
            drawRasterOutlinedCenteredText(firstLine, {centerX, bounds.y + 258.0F * scale},
                commonSize, 5.0F * scale, WHITE, CardBlack, cardFade);
            drawRasterOutlinedCenteredText(secondLine, {centerX, bounds.y + 298.0F * scale},
                commonSize, 5.0F * scale, WHITE, CardBlack, cardFade);
        } else {
            drawRasterOutlinedCenteredText(displayTitleText, {centerX, bounds.y + 278.0F * scale},
                fitRasterTextSize(displayTitleText, titleSize, 38.0F * scale, titleWidth),
                5.0F * scale, WHITE, CardBlack, cardFade);
        }

        const Rectangle valuePanel{
            bounds.x + 20.0F * scale,
            bounds.y + 340.0F * scale,
            bounds.width - 40.0F * scale,
            100.0F * scale};
        DrawRectangleRounded(
            valuePanel,
            roundedRectangleRoundness(
                valuePanel, 25.0F * scale),
            8,
            fadedColor(CardInset, cardFade));
        const UpgradeValueCopy valueCopy =
            runUpgradeWeapon(effect) ? UpgradeValueCopy{"NEW", "Player weapon"}
                : runUpgradeBuilding(effect) ? UpgradeValueCopy{"NEW", "Building blueprint"}
                : effect == RunUpgradeEffect::CannonHeavyShell ? UpgradeValueCopy{"x5 DAMAGE", "Cannon fires 3x slower"}
                : effect == RunUpgradeEffect::CannonScattershot ? UpgradeValueCopy{"5 SHELLS", "40% damage each / smaller blasts"}
                : effect == RunUpgradeEffect::CannonClusterShell ? UpgradeValueCopy{"+3 BLASTS", "80% main damage / 36% mini"}
                : upgradeValueCopy(description, mark);
        const float valueSize = fitUiTextSize(
            valueCopy.value,
            56.0F / UiTextRasterScale * scale,
            32.0F / UiTextRasterScale * scale,
            valuePanel.width - 28.0F * scale);
        drawOutlinedCenteredText(
            valueCopy.value, centerX, valuePanel.y + 5.0F * scale,
            valueSize, accent, cardFade, scale,
            CardInk, 4.0F, false);
        {
            const ScopedUiFontRole descriptionFont(UiFontRole::Body);
            const float labelSize = fitUiTextSize(
                valueCopy.label, 30.0F / UiTextRasterScale * scale,
                18.0F / UiTextRasterScale * scale,
                valuePanel.width - 24.0F * scale);
            const Vector2 measured = measureUiText(valueCopy.label, labelSize);
            const SdfUiTextStyle style{
                .fillColor = fadedColor(CardValueCream, cardFade),
                .outlineColor = BLANK,
                .shadowColor = fadedColor({0, 0, 0, 140}, cardFade),
                .shadowOffsetPixels = {0.0F, scale},
                .outlinePixels = 0.0F,
            };
            drawStyledSdfUiText(valueCopy.label,
                {centerX - measured.x * 0.5F, valuePanel.y + 63.0F * scale},
                labelSize, style);
        }
        const auto weapon = runUpgradeWeapon(effect);
        const int currentLevel = weapon && snapshot.unlockedWeapons[static_cast<std::size_t>(*weapon)]
            ? std::max(1, snapshot.weaponLevels[static_cast<std::size_t>(*weapon)])
            : runTomeLevel(effect, snapshot.runUpgradeStacks);
        const int maximumLevel = runUpgradeMaximumLevel(effect);
        const Rectangle levelBadge{
            centerX - 100.0F * scale,
            bounds.y + 462.0F * scale,
            200.0F * scale,
            34.0F * scale};
        const float levelFontSize =
            28.0F / UiTextRasterScale * scale;
        const auto blueprint = runUpgradeBuilding(effect);
        const bool economyBlueprint = blueprint && !runCombatBuilding(*blueprint);
        const bool oneTime = (fixedRunUpgradePower(effect) && !blueprint && !runWeaponLevelCard(effect)) || economyBlueprint;
        if (oneTime) {
            drawOutlinedCenteredText((economyBlueprint || runUpgradeWeapon(effect)) ? "UNLOCK" : "ONE-TIME", centerX, levelBadge.y + 4.0F * scale,
                levelFontSize, LevelText, cardFade, scale, LevelOutline, 2.0F, false);
        } else {
            const std::string currentLevelText =
                (runDefenseCard(effect) || runWeaponLevelCard(effect)) && currentLevel == 0 ? "NEW"
                    : runHammerDefinition(effect) && currentLevel == 0 ? "NEW" : "LVL " + std::to_string(currentLevel);
            const std::string nextLevelText = std::to_string(std::min(currentLevel + 1, maximumLevel));
            const Vector2 currentLevelSize = measureUiText(
                currentLevelText, levelFontSize);
            const Vector2 nextLevelSize = measureUiText(
                nextLevelText, levelFontSize);
            const float currentLevelY = levelBadge.y +
                (levelBadge.height -
                 currentLevelSize.y) * 0.5F;
            const float nextLevelY = levelBadge.y +
                (levelBadge.height - nextLevelSize.y) * 0.5F;
            const float arrowWidth = 32.0F * scale;
            const float contentGap = 12.0F * scale;
            const float contentWidth = currentLevelSize.x +
                contentGap + arrowWidth + contentGap + nextLevelSize.x;
            const float contentX = centerX - contentWidth * 0.5F;
            const float currentLevelCenterX =
                contentX + currentLevelSize.x * 0.5F;
            const float arrowCenterX = contentX + currentLevelSize.x +
                contentGap + arrowWidth * 0.5F;
            const float nextLevelCenterX =
                contentX + currentLevelSize.x + contentGap +
                arrowWidth + contentGap + nextLevelSize.x * 0.5F;
            drawOutlinedCenteredText(
                currentLevelText, currentLevelCenterX,
                currentLevelY, levelFontSize,
                LevelText, cardFade, scale,
                LevelOutline, 3.0F, false);
            drawLevelArrow(
                {arrowCenterX,
                 levelBadge.y + levelBadge.height * 0.5F},
                scale, palette.accent, cardFade);
            drawOutlinedCenteredText(
                nextLevelText, nextLevelCenterX,
                nextLevelY, levelFontSize,
                palette.accent, cardFade, scale,
                LevelOutline, 3.0F, false);
        }
        drawOutlinedCenteredText(
            runUpgradeWeapon(effect) ? "NEW WEAPON"
                : runUpgradeBuilding(effect) ? "NEW BUILDING" : runUpgradeRarityName(rarity), centerX,
            bounds.y + 516.0F * scale,
            22.0F / UiTextRasterScale * scale,
            palette.accent, cardFade, scale,
            CardInk, 2.0F, false);
    }
    std::string controls;
    if (snapshot.runUpgradeRerollTokens > 0) {
        controls += "R  REROLL (" +
            std::to_string(snapshot.runUpgradeRerollTokens) + ")";
    }
    if (snapshot.runUpgradeLockUnlocked) {
        if (!controls.empty()) controls += "     ";
        controls += "RMB  LOCK CARD";
    }
    if (!controls.empty()) {
        const float controlsFade = smoothUnit(static_cast<float>(
            (entranceSeconds - 0.34) / 0.20)) * overlayFade;
        drawCenteredUiText(
            controls,
            cardTop + firstCard.height + 30.0F,
            15.0F,
            fadedColor({229, 219, 185, 245}, controlsFade));
    }
}

namespace {
RunShopLayout shopLayout() {
    const float s = std::min(static_cast<float>(uiScreenWidth()) / 1920.0F, static_cast<float>(uiScreenHeight()) / 1080.0F);
    const float x = (static_cast<float>(uiScreenWidth()) - 1920 * s) * .5F;
    const float y = (static_cast<float>(uiScreenHeight()) - 1080 * s) * .5F;
    const auto rect = [=](float a, float b, float w, float h) { return Rectangle{x+a*s,y+b*s,w*s,h*s}; };
    RunShopLayout l;
    l.scale = s;
    l.inventory = rect(32, 78, 260, 932);
    l.stats = rect(1516, 78, 372, 932);
    l.header = rect(316, 78, 1176, 104);
    l.reroll = rect(1200, 100, 268, 62);
    l.proceed = rect(1192, 946, 300, 64);
    for (std::size_t i = 0; i < RunShopSlotCount; ++i) {
        const float left = 316.0F + static_cast<float>(i) * 300.0F;
        l.cards[i] = rect(left, 208, 276, 620);
        l.buy[i] = rect(left + 16, 742, 244, 64);
        l.locks[i] = rect(left + 40, 850, 196, 52);
    }
    return l;
}
RunShopLayout scaledShopLayout(RunShopLayout layout,float scale) {
    const Vector2 center{static_cast<float>(uiScreenWidth())*.5F,static_cast<float>(uiScreenHeight())*.5F};
    const auto transform=[&](Rectangle r){return Rectangle{center.x+(r.x-center.x)*scale,
        center.y+(r.y-center.y)*scale,r.width*scale,r.height*scale};};
    for(auto& r:layout.cards) r=transform(r);
    for(auto& r:layout.buy) r=transform(r);
    for(auto& r:layout.locks) r=transform(r);
    for(auto* r:{&layout.inventory,&layout.stats,&layout.header,&layout.reroll,&layout.proceed}) *r=transform(*r);
    layout.scale*=scale;return layout;
}
void shopText(std::string_view text, Rectangle r, float size, Color color, float s, int maxLines = 8,float fade=1) {
    const ScopedUiFontRole font(UiFontRole::MenuBodyBold);
    std::istringstream words{std::string{text}};
    std::string word, line;
    int n = 0;
    const auto emit = [&] {
        label(line, {r.x, r.y + n * (size + 7 * s), r.width, size + 7 * s},
            size, color, fade, UiFontRole::BodyBold);
        ++n; line.clear();
    };
    while (words >> word) {
        if (!line.empty() && measureUiText(line + " " + word, size).x > r.width) emit();
        if (n >= maxLines) break;
        if (!line.empty()) line += " ";
        line += word;
    }
    if (!line.empty() && n < maxLines) emit();
}
}
RunShopLayout runShopLayout() {
    const ScopedUiCoordinates coordinates;
    auto l = shopLayout();
    if (coordinates.owns()) {
        for (auto& r : l.cards) r = coordinates.viewport().toScreen(r);
        for (auto& r : l.buy) r = coordinates.viewport().toScreen(r);
        for (auto& r : l.locks) r = coordinates.viewport().toScreen(r);
        for (auto* r : {&l.inventory, &l.stats, &l.header, &l.reroll, &l.proceed})
            *r = coordinates.viewport().toScreen(*r);
    }
    return l;
}
void drawRunShopOverlay(GameUi& ui, const SimulationSnapshot& snapshot,
    std::optional<int> keyboardFocus, std::optional<Vector2> pointerPosition,double entranceSeconds,bool reduceFlashes) {
    if (!snapshot.runShop.pending || snapshot.runUpgradeChoicePending || snapshot.state == RunState::Paused) return;
    const ScopedUiCanvas canvas;
    if (pointerPosition && canvas.owns()) pointerPosition = canvas.viewport().toUi(*pointerPosition);
    const float progress=std::clamp(static_cast<float>(entranceSeconds/RunShopOverlayEntranceDuration),0.0F,1.0F);
    const float fade=smoothUnit(progress);
    if(fade<=0) return;
    const bool interactive=entranceSeconds>=RunShopOverlayEntranceDuration;
    const auto pointer=interactive?pointerPosition.value_or(uiMousePosition()):Vector2{-10000,-10000};
    if(!interactive) keyboardFocus.reset();
    const auto l=scaledShopLayout(shopLayout(),.4F+.6F*fade+(reduceFlashes?0:.025F*std::sin(progress*PI)));
    const float s=l.scale;
    DrawRectangle(0, 0, uiScreenWidth(), uiScreenHeight(), fadedColor({11, 19, 22, 190},fade));
    const auto base=shopLayout();
    drawUpgradeRays({base.header.x,base.cards[0].y,base.header.width,base.cards[0].height},
        entranceSeconds,fade,true,reduceFlashes);
    reward_backdrop::draw(ui,reward_backdrop::Kind::Coins,entranceSeconds,
        {static_cast<float>(uiScreenWidth()),static_cast<float>(uiScreenHeight())},fade,reduceFlashes);
    auto preview = snapshot;
    preview.runUpgradeChoiceCount = RunShopSlotCount;
    std::optional<std::size_t> hovered;
    for (std::size_t i = 0; i < RunShopSlotCount; ++i) {
        preview.runUpgradeChoices[i] = snapshot.runShop.offers[i].effect;
        preview.runUpgradeRarities[i] = snapshot.runShop.offers[i].rarity;
        if (snapshot.runShopBuyable[i] && (CheckCollisionPointRec(pointer, l.cards[i]) ||
            (keyboardFocus && *keyboardFocus == static_cast<int>(i)))) hovered = i;
    }
    drawUpgradeStats(preview, {{}, l.inventory, l.stats, s, 0}, fade, hovered);
    const auto inventoryHover = drawUpgradeInventory(ui, snapshot, l.inventory, s, fade, pointer);
    plate(l.header, s, menuStyle::Frame, fade);
    label(localizeText("SHOP") + " - " + localizeText("WAVE") + " " + std::to_string(snapshot.runShop.wave),
        {l.header.x+22*s,l.header.y+10*s,430*s,46*s},32*s,menuStyle::Gold,fade,UiFontRole::Display);
    label(localizeText("TOMES") + " " + std::to_string(snapshot.tomeCount) + "/" + std::to_string(snapshot.tomeSlotLimit),
        {l.header.x+22*s,l.header.y+58*s,430*s,30*s},20*s,menuStyle::Muted,fade,UiFontRole::BodyBold);
    drawIcon(ui.resourceTexture(UiResourceIcon::Coin), {l.header.x+568*s,l.header.y+28*s,46*s,46*s},fade);
    label(std::to_string(snapshot.coins),{l.header.x+628*s,l.header.y+18*s,210*s,66*s},38*s,menuStyle::Gold,fade,UiFontRole::Display);
    const auto button = [&](Rectangle r, std::string_view text, bool enabled, int focus, Color color) {
        const bool over = CheckCollisionPointRec(pointer,r) || (keyboardFocus && *keyboardFocus == focus);
        plate(r,s*.65F,enabled ? (over ? menuStyle::Selected : menuStyle::Card) : menuStyle::Inset,fade);
        label(text,{r.x+8*s,r.y,r.width-16*s,r.height},23*s,(enabled || focus < 4) ? color : menuStyle::Muted,fade,UiFontRole::Display,1);
        if (over) drawSelectionCorners(r,s*.65F,fade);
    };
    bool unlocked = false;
    for (std::size_t i = 0; i < RunShopSlotCount; ++i)
        unlocked |= !snapshot.runShop.offers[i].locked || !snapshot.runShopBuyable[i];
    button(l.reroll,localizeText("REROLL") + "  " + std::to_string(snapshot.unlimitedResources ? 0 : runShopRerollPrice(snapshot.runShop)),
        unlocked && (snapshot.unlimitedResources || snapshot.coins >= runShopRerollPrice(snapshot.runShop)),8,menuStyle::Gold);
    for (std::size_t i = 0; i < RunShopSlotCount; ++i) {
        const auto& offer = snapshot.runShop.offers[i]; const auto r = l.cards[i];
        if (!offer.present) {
            plate(r,s,menuStyle::Inset,fade);
            label("SOLD",{r.x,r.y+r.height*.4F,r.width,72*s},34*s,menuStyle::Muted,fade,UiFontRole::Display,1);
            continue;
        }
        const auto accent = rarityColor(offer.rarity);
        plate(r,s,rarityFill(offer.rarity),fade);
        label(runUpgradeRarityName(offer.rarity),{r.x+16*s,r.y+14*s,r.width-32*s,30*s},20*s,accent,fade,UiFontRole::BodyBold,1);
        drawIcon(ui.runUpgradeTexture(offer.effect),{r.x+(r.width-116*s)*.5F,r.y+58*s,116*s,116*s},fade);
        shopText(localizeText(runUpgradeDefinition(offer.effect).name),
            {r.x+18*s,r.y+194*s,r.width-36*s,112*s},28*s,menuStyle::Cream,s,3,fade);
        shopText(localizedUpgradeDescription(snapshot,offer.effect,offer.rarity),
            {r.x+18*s,r.y+322*s,r.width-36*s,194*s},22*s,menuStyle::Cream,s,6,fade);
        const bool buyable = snapshot.runShopBuyable[i];
        const bool affordable = snapshot.unlimitedResources || snapshot.coins >= offer.price;
        const std::string price = buyable ? std::to_string(snapshot.unlimitedResources ? 0 : offer.price) : localizeText("LIMIT REACHED");
        button(l.buy[i],price,buyable && affordable,static_cast<int>(i),
            affordable ? menuStyle::Cream : Color{255,100,95,255});
        if (buyable) drawIcon(ui.resourceTexture(UiResourceIcon::Coin),
            {l.buy[i].x+l.buy[i].width*.5F+36*s,l.buy[i].y+16*s,32*s,32*s},fade);
        button(l.locks[i],offer.locked ? "OFFER LOCKED" : "LOCK",buyable || offer.locked,
            static_cast<int>(i+4),offer.locked ? menuStyle::Gold : menuStyle::Cream);
        if (hovered == i) drawSelectionCorners(r,s,fade);
    }
    button(l.proceed,"CONTINUE",true,9,menuStyle::Gold);
    label("R: REROLL / 1-4: BUY / ESC: CONTINUE",{l.header.x, l.proceed.y,820*s,30*s},
        19*s,menuStyle::Muted,fade,UiFontRole::BodyBold);
    label("Locked offers carry over. Prices rise every wave.",
        {l.header.x,l.proceed.y+35*s,850*s,28*s},18*s,menuStyle::Muted,fade);
    if (inventoryHover) drawItemTooltip(inventoryHover->title,inventoryHover->description,pointer,
        {static_cast<float>(uiScreenWidth()),static_cast<float>(uiScreenHeight())},s);
}
} // namespace ian::app_detail
