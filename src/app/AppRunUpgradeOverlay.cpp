#include "app/AppRunUpgradeOverlay.hpp"
#include "progression/DefenseAttackModifiers.hpp"
#include "progression/CoreCardUpgrades.hpp"
#include "ui/UiMenuStyle.hpp"
#include "ui/UiLabels.hpp"
#include "ui/UiItemDescriptions.hpp"
#include "ui/UiUpgradeDescriptions.hpp"
#include "ui/UiItemTooltip.hpp"
#include "localization/Localization.hpp"
#include "ui/UiViewport.hpp"
#include "ui/RewardBackdrop.hpp"
#include "ui/UiSolidGeometry.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace ian::app_detail {
namespace {

struct UpgradeLayout {
    Rectangle offers;
    Rectangle inventory;
    Rectangle stats;
    float scale;
    float rowHeight;
};

// All bounds come from one layout, shared by rendering and physical input.
UpgradeLayout upgradeLayout(std::size_t count) {
    const float width = static_cast<float>(uiScreenWidth());
    const float height = static_cast<float>(uiScreenHeight());
    const float scale = std::min({1.0F, height / 1080.0F, width / 1000.0F});
    count = std::clamp(count, std::size_t{1}, MaximumRunUpgradeChoices);
    const float offersWidth = std::min(1000.0F * scale, width - 64.0F * scale);
    const float desiredRowHeight = count <= DefaultRunUpgradeChoices ? 184.0F : 176.0F;
    const float rowHeight = std::min(desiredRowHeight * scale,
        (height - 240.0F * scale - static_cast<float>(count - 1) * 20.0F * scale) /
            static_cast<float>(count));
    const float offersHeight = 152.0F * scale + static_cast<float>(count) * rowHeight +
        static_cast<float>(count - 1) * 20.0F * scale;
    return {{(width - offersWidth) * 0.5F, (height - offersHeight) * 0.5F,
             offersWidth, offersHeight}, {}, {}, scale, rowHeight};
}

float smoothUnit(float value) {
    const float t = std::clamp(value, 0.0F, 1.0F);
    return t * t * (3.0F - 2.0F * t);
}
UpgradeLayout scaledUpgradeLayout(UpgradeLayout layout, float scale) {
    const Vector2 center{layout.offers.x + layout.offers.width * 0.5F,
        layout.offers.y + layout.offers.height * 0.5F};
    const auto transform = [center, scale](Rectangle bounds) {
        return Rectangle{center.x + (bounds.x - center.x) * scale,
            center.y + (bounds.y - center.y) * scale, bounds.width * scale, bounds.height * scale};
    };
    layout.offers = transform(layout.offers);
    layout.inventory = transform(layout.inventory);
    layout.stats = transform(layout.stats);
    layout.scale *= scale;
    layout.rowHeight *= scale;
    return layout;
}
Rectangle upgradeCardBounds(const UpgradeLayout& layout, std::size_t index) {
    const float s = layout.scale;
    return {layout.offers.x + 44 * s,
        layout.offers.y + 90 * s + index * (layout.rowHeight + 20 * s),
        layout.offers.width - 88 * s, layout.rowHeight};
}
Rectangle upgradeUtilityBounds(const UpgradeLayout& layout,std::size_t action) {
    const float s=layout.scale;
    const float width=(layout.offers.width-112*s)/3;
    return {layout.offers.x+44*s+static_cast<float>(action)*(width+12*s),
        layout.offers.y+layout.offers.height-52*s,width,38*s};
}
Rectangle upgradeRerollBounds(const UpgradeLayout& layout) { return upgradeUtilityBounds(layout,0); }

Color fadedColor(Color color, float opacity) {
    color.a = static_cast<unsigned char>(std::lround(color.a * std::clamp(opacity, 0.0F, 1.0F)));
    return color;
}
void label(std::string_view value, Rectangle bounds, float size, Color color, float fade,
           UiFontRole role = UiFontRole::Body, int align = 0) {
    if (role == UiFontRole::BodyBold) role = UiFontRole::MenuBodyBold;
    const ScopedUiFontRole font(role);
    const float fitted = fitUiTextSize(value, size, size * 0.60F, bounds.width, bounds.height);
    const Vector2 measured = measureUiText(value, fitted);
    const float x = align == 1 ? bounds.x + (bounds.width - measured.x) * 0.5F
        : align == 2 ? bounds.x + bounds.width - measured.x : bounds.x;
    SdfUiTextStyle style{};
    style.fillColor = fadedColor(color, fade);
    style.outlineColor = fadedColor({18, 18, 15, 255}, fade);
    style.outlinePixels = fitted * (role == UiFontRole::Display ? 0.12F
        : role == UiFontRole::MenuBodyBold ? 0.09F : 0.04F);
    style.shadowColor = fadedColor(BLACK, fade);
    style.shadowOffsetPixels = {0, fitted * 0.1F};
    style.softness = 0.6F;
    drawStyledSdfUiText(value, {x, bounds.y + (bounds.height - measured.y) * 0.5F}, fitted, style);
}
void plate(Rectangle bounds, float scale, Color color, float fade) {
    menuStyle::panel(bounds, scale, color, fade);
}
Color tinted(Color base, Color accent, float amount) {
    const auto channel = [amount](unsigned char from, unsigned char to) {
        return static_cast<unsigned char>(std::lround(from + (to - from) * amount));
    };
    return {channel(base.r, accent.r), channel(base.g, accent.g), channel(base.b, accent.b), 255};
}
void panelHeader(Rectangle bounds, float scale, std::string_view title, float fade) {
    plate(bounds, scale, menuStyle::Frame, fade);
    const Rectangle header{bounds.x, bounds.y, bounds.width, 72 * scale};
    plate(header, scale, menuStyle::Card, fade);
    label(title, {header.x + 20 * scale, header.y, header.width - 40 * scale, header.height},
        32 * scale, menuStyle::Cream, fade, UiFontRole::Display);
}
Color rarityColor(RunUpgradeRarity rarity) {
    switch (rarity) {
    case RunUpgradeRarity::Common: return {200, 204, 204, 255};
    case RunUpgradeRarity::Uncommon: return {89, 185, 255, 255};
    case RunUpgradeRarity::Rare: return {80, 229, 147, 255};
    case RunUpgradeRarity::Epic: return {205, 120, 246, 255};
    case RunUpgradeRarity::Legendary: return {255, 221, 90, 255};
    }
    return menuStyle::Cream;
}
Color rarityFill(RunUpgradeRarity rarity) {
    switch (rarity) {
    case RunUpgradeRarity::Common: return {45, 47, 48, 255};
    case RunUpgradeRarity::Uncommon: return {20, 54, 83, 255};
    case RunUpgradeRarity::Rare: return {18, 66, 44, 255};
    case RunUpgradeRarity::Epic: return {62, 29, 79, 255};
    case RunUpgradeRarity::Legendary: return {91, 76, 24, 255};
    }
    return menuStyle::Card;
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

// Each slot respawns a rectangular beam after its fade/shrink finishes.
// Hashes provide reproducible randomness without mutable RNG state
// or frame-rate-dependent births; neighbouring slots may overlap freely.
void drawUpgradeRays(Rectangle offers, double seconds, float fade, bool shop=false, bool reduceFlashes=false) {
    if (fade <= 0) return;
    const Vector2 center{offers.x + offers.width * 0.5F, offers.y + offers.height * 0.52F};
    const float screenRadius = std::hypot(static_cast<float>(uiScreenWidth()),
        static_cast<float>(uiScreenHeight())) * 0.72F;
    const float scale = std::min(1.0F,static_cast<float>(uiScreenHeight())/1080.0F);
    if (!shop) {
        const float breath = 0.08F + 0.025F * std::sin(static_cast<float>(seconds) * 1.1F);
        const float arrival = reduceFlashes ? 0.0F :
            0.12F * std::exp(-3.0F * static_cast<float>(std::max(0.0, seconds)));
        DrawCircleGradient(center, screenRadius * 0.65F,
            fadedColor({255, 206, 97, 255}, fade * (breath + arrival) *
                (reduceFlashes ? 0.5F : 1.0F)), BLANK);
    }
    constexpr std::uint32_t rayCount = 28;
    for (std::uint32_t slot = 0; slot < rayCount; ++slot) {
        const double lifetime = (2.4 + rayRandom(slot, 0xbb67aeU) * 2.2) * (2.0 / 3.0) *
            (reduceFlashes ? 2.0 : 1.0);
        const double elapsed = std::max(0.0, seconds) + rayRandom(slot, 0x6a09e6U) * lifetime;
        const auto cycle = static_cast<std::uint32_t>(std::fmod(std::floor(elapsed / lifetime), 4294967296.0));
        const float age = static_cast<float>(std::fmod(elapsed, lifetime) / lifetime);
        const std::uint32_t seed = slot ^ (cycle * 0x9e3779b9U);
        const float angle = rayRandom(seed, 0x51f15eU) * (2 * PI) +
            (shop ? 0.0F : static_cast<float>(seconds) * 0.018F);
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
        const float settle = shop ? 1.0F : 0.45F + 0.55F *
            (1.0F - smoothUnit(static_cast<float>(seconds / 0.8)));
        drawSolidTriangleFan(points.data(), static_cast<int>(points.size()), fadedColor(color,
            fade * opacity * settle * (shop?.30F:.90F) * (reduceFlashes?.5F:1.0F)));
    }
}

void drawIcon(Texture2D texture, Rectangle bounds, float fade, float rotation = 0.0F) {
    if (!IsTextureValid(texture)) return;
    const float size = std::min(bounds.width, bounds.height);
    DrawTexturePro(texture, {0, 0, static_cast<float>(texture.width), static_cast<float>(texture.height)},
        {bounds.x + bounds.width * 0.5F, bounds.y + bounds.height * 0.5F, size, size},
        {size * 0.5F, size * 0.5F}, rotation, fadedColor(WHITE, fade));
}

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
            : "Unlock level 1 construction; uses one structure slot";
    }
    if (const auto weapon=runUpgradeWeapon(effect); weapon && runWeaponLevelCard(effect)) {
        return snapshot.unlockedWeapons[static_cast<std::size_t>(*weapon)]
            ? "Upgrade weapon by one level" : "Unlock level 1 weapon";
    }
    std::string description{runUpgradeDefinition(effect).description};
    if (fixedRunUpgradePower(effect)) return description;
    const std::size_t numberEnd = description.find_first_not_of("0123456789.", 1);
    if (numberEnd == std::string::npos || numberEnd <= 1) return description;
    const double power = runUpgradeAppliedPowerGain(
        effect, rarity, snapshot.runUpgradePower[runUpgradeIndex(effect)]);
    double amount = std::stod(description.substr(1, numberEnd - 1)) * power;
    description.replace(1, numberEnd - 1, statNumber(amount, effect == RunUpgradeEffect::Regeneration ? 2 : 1));
    return description;
}

std::string buildingStatBonusLine(AttackStat stat, double power, BuildingType type) {
    const double value = buildingStatUpgradeIncrement(stat) * power;
    std::string_view name = "";
    switch (stat) {
    case AttackStat::Damage: name = "Damage"; break;
    case AttackStat::AttackSpeed: name = "Fire rate"; break;
    case AttackStat::Range: name = "Range"; break;
    case AttackStat::Quantity:
        name = type == BuildingType::TeslaTower ? "Attack count" : "Extra projectiles";
        break;
    case AttackStat::Bounces: name = "Bounces"; break;
    case AttackStat::Pierce: name = "Pierce"; break;
    case AttackStat::CritChance: name = "Crit chance"; break;
    case AttackStat::CritDamage: name = "Crit damage"; break;
    case AttackStat::ProjectileSpeed: name = "Projectile speed"; break;
    case AttackStat::Size: name = "Size"; break;
    case AttackStat::Duration: name = "Duration"; break;
    case AttackStat::Knockback: name = "Knockback"; break;
    case AttackStat::SlowStrength: name = "Slow strength"; break;
    default: break;
    }
    const bool percentagePoints = stat == AttackStat::CritChance || stat == AttackStat::CritDamage;
    const bool percentage = !buildingStatUpgradeIsFlat(stat) || percentagePoints;
    const std::string amount = statNumber(value * (percentage ? 100.0 : 1.0), 2);
    return "+" + amount + (percentagePoints ? " pp " : percentage ? "% " :
        stat == AttackStat::Range ? "m " : " ") +
        localizeText(name);
}

std::string buildingStatRollLine(AttackStat stat, RunUpgradeRarity rarity, BuildingType type) {
    return buildingStatBonusLine(stat, runUpgradeRarityMultiplier(rarity), type);
}

std::string buildingUpgradeSummary(BuildingType type,
    const BuildingUpgradePowers& powers) {
    const auto typeIndex = static_cast<std::size_t>(type);
    std::string result;
    for (const auto stat : buildingStatUpgradePool(type)) {
        const double power = powers[typeIndex][static_cast<std::size_t>(stat)];
        if (power <= 0.0) continue;
        if (!result.empty()) result += " · ";
        result += buildingStatBonusLine(stat, power, type);
    }
    return result;
}

std::string buildingStatRollDescription(BuildingType type, RunUpgradeRarity rarity,
    std::uint64_t seed, std::string_view separator) {
    const auto roll = rollBuildingStatUpgrade(type, rarity, seed);
    std::string result;
    for (std::size_t i = 0; i < roll.count; ++i) {
        if (i != 0) result += separator;
        result += buildingStatRollLine(roll.stats[i], rarity, type);
    }
    return result;
}

std::string localizedUpgradeDescription(const SimulationSnapshot& snapshot,
    RunUpgradeEffect effect, RunUpgradeRarity rarity, std::uint64_t rollSeed = 0) {
    if (effect == RunUpgradeEffect::UpgradeCore) {
        const auto roll = rollCoreCardUpgrade(rarity, rollSeed);
        std::string result;
        for (std::size_t i = 0; i < roll.count; ++i) {
            if (i) result += " / ";
            const auto stat = roll.stats[i];
            const double oldPower = snapshot.coreCardPowers[static_cast<std::size_t>(stat)];
            switch (stat) {
            case CoreCardStat::Health:
                result += "+" + statNumber(15.0 * roll.power) + "% " + localizeText("Core HP");
                break;
            case CoreCardStat::BuildRadius:
                result += "+" + statNumber(std::floor(oldPower + roll.power) - std::floor(oldPower), 0) + "m " + localizeText("Build range");
                break;
            case CoreCardStat::Buildings:
                result += "+" + statNumber(std::floor(2.0 * (oldPower + roll.power)) - std::floor(2.0 * oldPower), 0) + " " + localizeText("Buildings");
                break;
            case CoreCardStat::Storage:
                result += "+" + statNumber(100.0 * CoreStorageBonusPerPoint * roll.power) + "% " + localizeText("All storage");
                break;
            }
        }
        return result;
    }
    if (const auto building = runUpgradeBuilding(effect);
        building && runCombatBuilding(*building) &&
        snapshot.runUpgradeStacks[runUpgradeIndex(effect)] > 0) {
        return buildingStatRollDescription(*building, rarity, rollSeed, " / ");
    }
    if (effect == RunUpgradeEffect::AttackSpeed) {
        const double gain = runUpgradeAppliedPowerGain(effect, rarity,
            snapshot.runUpgradePower[runUpgradeIndex(effect)]);
        return "+" + statNumber(10.0 * gain) + "% " + localizeText("Defense speed");
    }
    if (effect == RunUpgradeEffect::Damage) {
        const double gain = runUpgradeAppliedPowerGain(effect, rarity, snapshot.runUpgradePower[runUpgradeIndex(effect)]);
        return "+" + statNumber(10.0 * gain) + "% " +
            localizeText("Player damage") + " / +" +
            statNumber(12.0 * gain) + "% " +
            localizeText("Defense damage");
    }
    if (effect == RunUpgradeEffect::CriticalChance) {
        const double gain = runUpgradeAppliedPowerGain(effect, rarity,
            snapshot.runUpgradePower[runUpgradeIndex(effect)]);
        return "+" + statNumber(7.5 * gain, 2) + " pp " + localizeText("Crit chance");
    }
    const auto description = upgradeDescription(snapshot, effect, rarity);
    const auto separator = description.find(' ');
    if (separator != std::string::npos && description.size() > 1 &&
        (description.front() == '+' || description.front() == '-') &&
        std::isdigit(static_cast<unsigned char>(description[1]))) {
        std::string name = description.substr(separator + 1);
        if (!name.empty()) name.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(name.front())));
        return description.substr(0, separator + 1) + localizeText(name);
    }
    return localizeText(description);
}

struct OfferStatLine { std::string name, before, after; };
struct OfferDescription {
    std::vector<OfferStatLine> stats;
    std::vector<std::string> roleLines;
    [[nodiscard]] std::size_t lineCount() const {
        return roleLines.empty() ? stats.size() : roleLines.size();
    }
};

OfferDescription describeOffer(const SimulationSnapshot& snapshot, std::size_t choice,
                               float width, float scale) {
    OfferDescription description;
    auto& lines = description.stats;
    const auto effect = snapshot.runUpgradeChoices[choice];
    const auto offeredBuilding = runUpgradeBuilding(effect);
    const auto offeredWeapon = runUpgradeWeapon(effect);
    const bool producerOffer = offeredBuilding && std::find(RunEconomyBuildingTypes.begin(),
        RunEconomyBuildingTypes.end(), *offeredBuilding) != RunEconomyBuildingTypes.end();
    const bool acquired = snapshot.runUpgradeStacks[runUpgradeIndex(effect)] > 0 ||
        (offeredBuilding && snapshot.unlockedBuildings[static_cast<std::size_t>(*offeredBuilding)]) ||
        (offeredWeapon && snapshot.unlockedWeapons[static_cast<std::size_t>(*offeredWeapon)]);
    const auto role = runUpgradeRoleDescription(effect);
    if (!role.empty() && !acquired && !producerOffer && snapshot.shrines.offer == ShrineKind::None) {
        const ScopedUiFontRole roleFont(UiFontRole::MenuBodyBold);
        const float size = fitUiTextSize("Ag", 29.7F * scale, 21.6F * scale,
            width, 43.2F * scale);
        std::istringstream words(localizeText(role));
        std::string word, line;
        while (words >> word) {
            if (!line.empty() && measureUiText(line + " " + word, size).x > width) {
                description.roleLines.push_back(std::move(line));
                line.clear();
            }
            if (!line.empty()) line += ' ';
            line += word;
        }
        if (!line.empty()) description.roleLines.push_back(std::move(line));
        return description;
    }
    const auto compare = [&](std::string_view name, double before, double after,
                             std::string_view suffix = "", int precision = 2) {
        lines.push_back({localizeText(name), statNumber(before, precision) + std::string(suffix),
            statNumber(after, precision) + std::string(suffix)});
    };
    if (effect == RunUpgradeEffect::UpgradeCore) {
        const auto roll = rollCoreCardUpgrade(snapshot.runUpgradeRarities[choice],
            snapshot.runUpgradeRollSeeds[choice]);
        for (std::size_t i = 0; i < roll.count; ++i) {
            const auto stat = roll.stats[i];
            const double power = snapshot.coreCardPowers[static_cast<std::size_t>(stat)];
            switch (stat) {
            case CoreCardStat::Health:
                compare("Core HP", 100 + 15 * power, 100 + 15 * (power + roll.power), "%"); break;
            case CoreCardStat::BuildRadius:
                compare("Build range", snapshot.coreBuildRadius,
                    snapshot.coreBuildRadius + std::floor(power + roll.power) - std::floor(power), "m", 0); break;
            case CoreCardStat::Buildings:
                compare("Buildings", snapshot.totalBuildingLimit,
                    snapshot.totalBuildingLimit + std::floor(2 * (power + roll.power)) - std::floor(2 * power), "", 0); break;
            case CoreCardStat::Storage:
                compare("All storage", 100 + 100 * CoreStorageBonusPerPoint * power,
                    100 + 100 * CoreStorageBonusPerPoint * (power + roll.power), "%"); break;
            }
        }
    } else if (offeredBuilding == BuildingType::Wall) {
        const double power = snapshot.runUpgradePower[runUpgradeIndex(effect)];
        const double gain = runUpgradeAppliedPowerGain(effect, snapshot.runUpgradeRarities[choice], power);
        compare("HP", wallMaxHealthAtPower(snapshot.wallBaseMaxHealth, power) * snapshot.upgradeStats.buildingHealth,
            wallMaxHealthAtPower(snapshot.wallBaseMaxHealth, power + gain) * snapshot.upgradeStats.buildingHealth, "", 0);
    } else if (const auto producer = runUpgradeBuilding(effect);
        producer && std::find(RunEconomyBuildingTypes.begin(),
            RunEconomyBuildingTypes.end(), *producer) != RunEconomyBuildingTypes.end()) {
        const auto index = static_cast<std::size_t>(*producer);
        const bool owned = snapshot.runUpgradeStacks[runUpgradeIndex(effect)] > 0;
        const double beforeRate = owned ? snapshot.producerRatesPerSecond[index] : 0.0;
        const double afterRate = owned ? snapshot.nextProducerRatesPerSecond[index]
            : snapshot.producerRatesPerSecond[index];
        compare("Income", beforeRate * 60.0, afterRate * 60.0, "/min", 1);
    } else if (const auto building = runUpgradeBuilding(effect);
        building && runCombatBuilding(*building) &&
        snapshot.runUpgradeStacks[runUpgradeIndex(effect)] > 0) {
        const auto roll = rollBuildingStatUpgrade(*building,
            snapshot.runUpgradeRarities[choice], snapshot.runUpgradeRollSeeds[choice]);
        for (std::size_t i = 0; i < roll.count; ++i) {
            const AttackStat stat = roll.stats[i];
            const auto [before, after] = buildingStatBonusPreview(
                snapshot.buildingUpgradePowers, *building, stat,
                snapshot.runUpgradeRarities[choice]);
            std::string_view name = "Power";
            switch (stat) {
            case AttackStat::Damage: name = "Damage"; break;
            case AttackStat::AttackSpeed: name = "Fire rate"; break;
            case AttackStat::Range: name = "Range"; break;
            case AttackStat::Quantity: name = *building == BuildingType::TeslaTower ? "Attack count" : "Extra projectiles"; break;
            case AttackStat::Bounces: name = "Bounces"; break;
            case AttackStat::Pierce: name = "Pierce"; break;
            case AttackStat::CritChance: name = "Crit chance"; break;
            case AttackStat::CritDamage: name = "Crit damage"; break;
            case AttackStat::ProjectileSpeed: name = "Projectile speed"; break;
            case AttackStat::Size: name = "Size"; break;
            case AttackStat::Duration: name = "Duration"; break;
            case AttackStat::Knockback: name = "Knockback"; break;
            case AttackStat::SlowStrength: name = "Slow strength"; break;
            default: break;
            }
            if (stat == AttackStat::Range) {
                const auto range = buildingRangeUpgradePreview(snapshot, *building,
                    snapshot.runUpgradeRarities[choice], snapshot.runUpgradeRollSeeds[choice]);
                compare(name, range.first, range.second, "m");
            }
            else if (stat == AttackStat::Quantity && *building == BuildingType::TeslaTower) {
                const auto count = buildingAttackCountUpgradePreview(snapshot, *building,
                    snapshot.runUpgradeRarities[choice], snapshot.runUpgradeRollSeeds[choice]);
                compare(name, count.first, count.second, "", 0);
            }
            else if (stat == AttackStat::Quantity || stat == AttackStat::Bounces || stat == AttackStat::Pierce)
                compare(name, std::floor(before + 1e-6), std::floor(after + 1e-6), "", 0);
            else if (stat == AttackStat::CritDamage)
                compare(name, 200 + 100 * before, 200 + 100 * after, "%");
            else if (stat == AttackStat::CritChance)
                compare(name, 100 * before, 100 * after, "%");
            else compare(name, 100 * (1 + before), 100 * (1 + after), "%");
        }
    } else {
    const auto next = previewRunUpgrade(snapshot, choice);
    if (const auto weapon = runUpgradeWeapon(effect); weapon && runWeaponLevelCard(effect)) {
        const auto index = static_cast<std::size_t>(*weapon);
        if (*weapon == PlayerWeapon::Axe || *weapon == PlayerWeapon::Pickaxe)
            compare("Power", 100 * playerWeaponLevelDamage(*weapon, snapshot.weaponLevels[index]),
                100 * playerWeaponLevelDamage(*weapon, next.weaponLevels[index]), "%", 0);
        else compare("Weapon level", snapshot.weaponLevels[index], next.weaponLevels[index], "", 0);
    } else if (runUpgradeBuilding(effect)) {
        compare("Building level", snapshot.runUpgradeStacks[runUpgradeIndex(effect)],
            snapshot.runUpgradeStacks[runUpgradeIndex(effect)] + 1, "", 0);
    }
    switch (effect) {
    case RunUpgradeEffect::PoisonChance: compare("Poison chance",
        runPoisonChance(snapshot.runUpgradePower[runUpgradeIndex(effect)]) * 100 +
            100 * .10 * shrineBonusPower(snapshot.shrines, snapshot.shrines.offer, effect),
        runPoisonChance(next.runUpgradePower[runUpgradeIndex(effect)]) * 100 +
            100 * .10 * shrineBonusPower(next.shrines, snapshot.shrines.offer, effect), "%"); break;
    case RunUpgradeEffect::Damage:
        compare("Player damage", snapshot.playerDamageMultiplier, next.playerDamageMultiplier, "x");
        if (snapshot.shrines.offer == ShrineKind::None)
            compare("Defense damage", snapshot.upgradeStats.defenseDamage, next.upgradeStats.defenseDamage, "x");
        break;
    case RunUpgradeEffect::AttackSpeed:
        if (snapshot.shrines.offer == ShrineKind::Player)
            compare("Weapon speed", snapshot.upgradeStats.playerAttackSpeed, next.upgradeStats.playerAttackSpeed, "x");
        else compare("Defense speed", snapshot.upgradeStats.defenseFireRate, next.upgradeStats.defenseFireRate, "x");
        break;
    case RunUpgradeEffect::GatheringSpeed: compare("Tool speed", snapshot.upgradeStats.toolAttackSpeed, next.upgradeStats.toolAttackSpeed, "x"); break;
    case RunUpgradeEffect::MoveSpeed: compare("Move speed", snapshot.playerMoveSpeedMultiplier, next.playerMoveSpeedMultiplier, "x"); break;
    case RunUpgradeEffect::MaximumHealth:
        compare("Max HP", snapshot.playerMaxHealth, next.playerMaxHealth);
        if(snapshot.shrines.offer!=ShrineKind::None)
            compare("Building HP",snapshot.upgradeStats.buildingHealth,next.upgradeStats.buildingHealth,"x");
        break;
    case RunUpgradeEffect::RecoverableArmor: compare("Shield", snapshot.playerMaxRecoverableArmor, next.playerMaxRecoverableArmor); break;
    case RunUpgradeEffect::Regeneration: compare("HP regen", snapshot.upgradeStats.healthRegeneration, next.upgradeStats.healthRegeneration, "/s"); break;
    case RunUpgradeEffect::BuildingHealth:
        compare("Max HP",snapshot.playerMaxHealth,next.playerMaxHealth);
        compare("Building HP", snapshot.upgradeStats.buildingHealth, next.upgradeStats.buildingHealth, "x"); break;
    case RunUpgradeEffect::DefenseDamage: compare("Defense damage", snapshot.upgradeStats.defenseDamage, next.upgradeStats.defenseDamage, "x"); break;
    case RunUpgradeEffect::DefenseFireRate: compare("Fire rate", snapshot.upgradeStats.defenseFireRate, next.upgradeStats.defenseFireRate, "x"); break;
    case RunUpgradeEffect::ProductionSpeed: compare("Production", snapshot.upgradeStats.productionSpeed, next.upgradeStats.productionSpeed, "x"); break;
    case RunUpgradeEffect::ExperienceGain: compare("Experience bonus",
        (snapshot.upgradeStats.experienceGain - 1.0) * 100.0,
        (next.upgradeStats.experienceGain - 1.0) * 100.0, "%"); break;
    case RunUpgradeEffect::ResourceCapacity: compare("Resource node capacity", snapshot.upgradeStats.resourceCapacity, next.upgradeStats.resourceCapacity, "x"); break;
    case RunUpgradeEffect::BuildingCapacity:
    case RunUpgradeEffect::ExtraBuilding: compare("Buildings", snapshot.totalBuildingLimit, next.totalBuildingLimit); break;
    case RunUpgradeEffect::BuildRadius: compare("Build range", snapshot.coreBuildRadius, next.coreBuildRadius, "m"); break;
    case RunUpgradeEffect::Luck: compare("Luck", snapshot.upgradeStats.luck * 100, next.upgradeStats.luck * 100, "%"); break;
    case RunUpgradeEffect::ProjectileCount: compare("Extra projectiles", std::floor(snapshot.upgradeStats.projectileCountBonus+1e-9), std::floor(next.upgradeStats.projectileCountBonus+1e-9), "", 0); break;
    case RunUpgradeEffect::Pierce: compare("Pierce", std::floor(snapshot.upgradeStats.pierce+1e-9), std::floor(next.upgradeStats.pierce+1e-9), "", 0); break;
    case RunUpgradeEffect::Bounces: compare("Bounces", std::floor(snapshot.upgradeStats.bounces+1e-9), std::floor(next.upgradeStats.bounces+1e-9), "", 0); break;
    case RunUpgradeEffect::ProjectileSpeed: compare("Projectile speed", snapshot.upgradeStats.projectileSpeed, next.upgradeStats.projectileSpeed, "x"); break;
    case RunUpgradeEffect::AttackDuration: compare("Duration", snapshot.upgradeStats.attackDuration, next.upgradeStats.attackDuration, "x"); break;
    case RunUpgradeEffect::AttackSize: compare("Size", snapshot.upgradeStats.attackSize, next.upgradeStats.attackSize, "x"); break;
    case RunUpgradeEffect::CriticalChance: compare("Crit chance", snapshot.upgradeStats.critChance * 100, next.upgradeStats.critChance * 100, "%"); break;
    case RunUpgradeEffect::CriticalDamage: compare("Crit damage", snapshot.upgradeStats.critDamage, next.upgradeStats.critDamage, "x"); break;
    case RunUpgradeEffect::AttackKnockback: compare("Knockback", snapshot.upgradeStats.knockback, next.upgradeStats.knockback, "x"); break;
    case RunUpgradeEffect::ClusterPayload: compare("Cluster blasts", snapshot.upgradeStats.clusterExplosions, next.upgradeStats.clusterExplosions); break;
    default: break;
    }
    }
    if (lines.empty()) compare("Level", snapshot.runUpgradeStacks[runUpgradeIndex(effect)],
        snapshot.runUpgradeStacks[runUpgradeIndex(effect)] + 1, "", 0);
    return description;
}

void drawOfferDescription(const OfferDescription& description, Rectangle bounds,
                          float scale, float fade) {
    const ScopedUiFontRole statFont(UiFontRole::MenuBodyBold);
    const float lineHeight = 43.2F * scale;
    for (std::size_t i = 0; i < description.roleLines.size(); ++i) {
        label(description.roleLines[i], {bounds.x, bounds.y + static_cast<float>(i) * lineHeight,
            bounds.width, lineHeight}, 29.7F * scale, menuStyle::Cream, fade,
            UiFontRole::BodyBold);
    }
    const auto& lines = description.stats;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const auto& line = lines[i];
        const std::string title = line.name + ": ";
        const float size = fitUiTextSize(title + "  " + line.before + "      " + line.after,
            29.7F * scale, 21.6F * scale, bounds.width - 30 * scale, lineHeight);
        float x = bounds.x;
        const float y = bounds.y + static_cast<float>(i) * lineHeight;
        const float titleWidth = measureUiText(title, size).x;
        label(title, {x, y, titleWidth + scale, lineHeight}, size,
            menuStyle::Gold, fade, UiFontRole::BodyBold);
        x += titleWidth + 9 * scale;
        const float beforeWidth = measureUiText(line.before, size).x;
        label(line.before, {x, y, beforeWidth + scale, lineHeight}, size,
            menuStyle::Cream, fade, UiFontRole::BodyBold);
        x += beforeWidth + 11 * scale;
        const float arrowY = y + lineHeight * 0.5F;
        const Color arrow = fadedColor(menuStyle::Cream, fade);
        DrawLineEx({x, arrowY}, {x + 14 * scale, arrowY}, 3 * scale, arrow);
        DrawTriangle({x + 20 * scale, arrowY},
            {x + 11 * scale, arrowY - 6 * scale},
            {x + 11 * scale, arrowY + 6 * scale}, arrow);
        x += 32 * scale;
        label(line.after, {x, y, std::max(0.0F, bounds.x + bounds.width - x), lineHeight},
            size, menuStyle::Green, fade, UiFontRole::BodyBold);
    }
}

void drawUpgradeStats(const SimulationSnapshot& snapshot, const UpgradeLayout& layout, float fade,
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
        hoveredEffect == RunUpgradeEffect::RerollToken);
    const Rectangle bounds = layout.stats;
    const float scale = std::min(layout.scale, bounds.height / 1010.0F);
    panelHeader(bounds, scale, "STATS", fade);
    const float left = bounds.x + 18 * scale;
    const float right = bounds.x + bounds.width - 18 * scale;
    float y = bounds.y + 82 * scale;
    const auto section = [&](std::string_view name) {
        y += 8 * scale;
        label(name, {left, y, right - left, 25 * scale}, 19 * scale,
            menuStyle::Gold, fade, UiFontRole::Display);
        y += 30 * scale;
    };
    const auto row = [&](std::string_view name, const std::string& value,
                         const std::string& nextValue, bool positive = true) {
        const bool changed = value != nextValue;
        const Color accent = positive ? menuStyle::Green : Color{255, 145, 112, 255};
        const float valueWidth = (right - left) * (changed ? 0.48F : 0.29F);
        if (changed) drawChamferedFill({left - 4 * scale, y, right - left + 8 * scale, 24 * scale},
            4 * scale, fadedColor({53, 66, 57, 255}, fade));
        label(name, {left, y, right - left - valueWidth - 8 * scale, 24 * scale},
            18 * scale, changed ? menuStyle::Cream : menuStyle::Muted, fade);
        label(changed ? value + " > " + nextValue : value,
            {right - valueWidth, y, valueWidth, 24 * scale}, 18 * scale,
            changed ? accent : menuStyle::Cream, fade, UiFontRole::BodyBold, 2);
        y += 24 * scale;
    };
    const auto multiplier = [](double value) { return statNumber(value, 2) + "x"; };
    const RunUpgradeStats& stats = snapshot.upgradeStats;
    const RunUpgradeStats& next = projected.upgradeStats;
    section("DEFENSE");
    row("Arsenal", std::to_string(snapshot.buildingArsenalCount) + "/" + std::to_string(snapshot.buildingSlotLimit),
        std::to_string(projected.buildingArsenalCount) + "/" + std::to_string(projected.buildingSlotLimit));
    row("Defense damage", multiplier(stats.defenseDamage), multiplier(next.defenseDamage));
    row("Fire rate", multiplier(stats.defenseFireRate), multiplier(next.defenseFireRate));
    row("Building HP", multiplier(stats.buildingHealth), multiplier(next.buildingHealth));
    row("Extra projectiles", statNumber(stats.projectileCountBonus), statNumber(next.projectileCountBonus));
    row("Cluster blasts", statNumber(stats.clusterExplosions), statNumber(next.clusterExplosions));
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
    row("Projectile speed", multiplier(stats.projectileSpeed), multiplier(next.projectileSpeed));
    row("Duration", multiplier(stats.attackDuration), multiplier(next.attackDuration));
    row("Size", multiplier(stats.attackSize), multiplier(next.attackSize));
    row("Crit chance", statNumber(stats.critChance * 100.0) + "%", statNumber(next.critChance * 100.0) + "%");
    row("Crit damage", multiplier(stats.critDamage), multiplier(next.critDamage));
    row("Knockback", multiplier(stats.knockback), multiplier(next.knockback));
    section("ECONOMY");
    row("Production", multiplier(stats.productionSpeed), multiplier(next.productionSpeed));
    row("Experience bonus", statNumber((stats.experienceGain - 1.0) * 100.0) + "%",
        statNumber((next.experienceGain - 1.0) * 100.0) + "%");
    row("Resource node capacity", multiplier(stats.resourceCapacity), multiplier(next.resourceCapacity));
    row("Build range", std::to_string(snapshot.coreBuildRadius) + "m", std::to_string(projected.coreBuildRadius) + "m");
    row("Luck", statNumber(stats.luck * 100.0) + "%", statNumber(next.luck * 100.0) + "%");
    section("PLAYER");
    const auto poisonChance=[](const SimulationSnapshot& state,ShrineKind kind) {
        return runPoisonChance(state.runUpgradePower[runUpgradeIndex(RunUpgradeEffect::PoisonChance)]+
            shrineBonusPower(state.shrines,kind,RunUpgradeEffect::PoisonChance));
    };
    if (poisonChance(snapshot,ShrineKind::Player)>0 || poisonChance(projected,ShrineKind::Player)>0)
        row("Poison chance",statNumber(poisonChance(snapshot,ShrineKind::Player)*100)+"%",
            statNumber(poisonChance(projected,ShrineKind::Player)*100)+"%");
    if (poisonChance(snapshot,ShrineKind::Buildings)>0 || poisonChance(projected,ShrineKind::Buildings)>0)
        row("Defense poison",statNumber(poisonChance(snapshot,ShrineKind::Buildings)*100)+"%",
            statNumber(poisonChance(projected,ShrineKind::Buildings)*100)+"%");
    row("Max HP", statNumber(snapshot.playerMaxHealth), statNumber(projected.playerMaxHealth));
    row("HP regen", statNumber(stats.healthRegeneration, 2) + "/s", statNumber(next.healthRegeneration, 2) + "/s");
    row("Shield", statNumber(snapshot.playerMaxRecoverableArmor), statNumber(projected.playerMaxRecoverableArmor));
    row("Player damage", multiplier(snapshot.playerDamageMultiplier), multiplier(projected.playerDamageMultiplier));
    row("Attack speed", multiplier(stats.playerAttackSpeed), multiplier(next.playerAttackSpeed));
    if (snapshot.runUpgradePower[runUpgradeIndex(RunUpgradeEffect::GatheringSpeed)] > 0 ||
        projected.runUpgradePower[runUpgradeIndex(RunUpgradeEffect::GatheringSpeed)] > 0 ||
        shrineBonusPower(projected.shrines,ShrineKind::Player,RunUpgradeEffect::GatheringSpeed)>0)
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
            row("Cannon mini blasts", statNumber(before.clusterCount + stats.clusterExplosions),
                statNumber(after.clusterCount + next.clusterExplosions));
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
        const double before = snapshot.runUpgradePower[index]+shrineBonusPower(snapshot.shrines,ShrineKind::Player,hoveredEffect);
        const double after = projected.runUpgradePower[index]+shrineBonusPower(projected.shrines,ShrineKind::Player,hoveredEffect);
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
        case RunUpgradeEffect::RerollToken: row("Rerolls", std::to_string(snapshot.runUpgradeRerollTokens),
            std::to_string(snapshot.runUpgradeRerollTokens + 1)); break;
        default: break;
        }
    }

}

struct InventoryEntry {
    Texture2D icon;
    std::string title;
    std::string_view description;
    int level;
    std::optional<BuildingType> buildingType{};
};
std::optional<InventoryEntry> drawUpgradeInventory(GameUi& ui, const SimulationSnapshot& snapshot, Rectangle bounds,
                          float scale, float fade, Vector2 pointer) {
    std::array<std::vector<InventoryEntry>, 4> groups;
    constexpr std::array<std::string_view, 7> weaponNames{
        "AXE", "PICKAXE", "CLUB", "RIFLE", "BOMB", "ICE WAND", "FIRE WAND"};
    for (std::size_t index = 0; index < PlayerWeaponHotbarOrder.size(); ++index) {
        const auto weapon = PlayerWeaponHotbarOrder[index];
        if (snapshot.unlockedWeapons[static_cast<std::size_t>(weapon)])
            groups[0].push_back({ui.weaponTexture(weapon), std::string{weaponNames[index]}, weaponDescription(weapon), 0});
    }
    for (const auto type : RunBuildingTypes)
        if (type != BuildingType::Wall &&
            (std::find(RunEconomyBuildingTypes.begin(), RunEconomyBuildingTypes.end(), type) ==
                RunEconomyBuildingTypes.end() ||
             snapshot.runUpgradeStacks[runUpgradeIndex(*buildingRunUpgrade(type))] > 0) &&
            snapshot.unlockedBuildings[static_cast<std::size_t>(type)])
            groups[1].push_back({ui.buildingTexture(type), std::string{buildingDisplayName(type)}, buildingDescription(type),
                snapshot.sandboxMode || snapshot.unlimitedResources ? 0 :
                    snapshot.runUpgradeStacks[runUpgradeIndex(*buildingRunUpgrade(type))], type});
    for (const auto& definition : RunUpgradeDefinitions) {
        if (definition.effect == RunUpgradeEffect::DefenseDamage || definition.effect == RunUpgradeEffect::DefenseFireRate) continue;
        const int level = runTomeLevel(definition.effect, snapshot.runUpgradeStacks);
        if (level > 0 && !retiredRunUpgrade(definition.effect) &&
            !runUpgradeBuilding(definition.effect) && !runUpgradeWeapon(definition.effect))
            groups[2].push_back({ui.runUpgradeTexture(definition.effect), std::string{definition.name}, definition.description, level});
    }
    for (std::size_t i=0;i<LootUpgradeEffectCount;++i) {
        const int stacks = snapshot.lootStacks[i];
        if (stacks > 0) {
            const auto effect = static_cast<LootUpgradeEffect>(i);
            groups[3].push_back({ui.lootTexture(effect),lootUpgradeName(effect),lootUpgradeDescription(effect),stacks});
        }
    }
    const float innerWidth = bounds.width - 36 * scale;
    const std::size_t columns = std::max(std::size_t{1}, static_cast<std::size_t>(innerWidth / (54 * scale)));
    std::size_t rows = 0;
    for (const auto& group : groups) rows += std::max(std::size_t{1}, (group.size() + columns - 1) / columns);
    const float contentHeight = std::min(bounds.height, (302 + static_cast<float>(rows) * 54) * scale);
    bounds.y += (bounds.height - contentHeight) * 0.5F;
    bounds.height = contentHeight;
    panelHeader(bounds, scale, "INVENTORY", fade);
    const float cell = std::min(innerWidth / static_cast<float>(columns),
        (bounds.height - 264 * scale) / static_cast<float>(rows));
    float y = bounds.y + 86 * scale;
    const std::array<std::string_view, 4> titles{"WEAPONS", "BUILDINGS", "TOMES & PERKS", "ITEMS"};
    const InventoryEntry* hovered = nullptr;
    for (std::size_t groupIndex = 0; groupIndex < groups.size(); ++groupIndex) {
        const auto& group = groups[groupIndex];
        label(titles[groupIndex], {bounds.x + 18 * scale, y, innerWidth, 30 * scale},
            21 * scale, menuStyle::Cream, fade, UiFontRole::Display);
        y += 34 * scale;
        if (group.empty()) label("—", {bounds.x + 20 * scale, y, innerWidth, cell},
            20 * scale, menuStyle::Muted, fade);
        for (std::size_t index = 0; index < group.size(); ++index) {
            const auto& entry = group[index];
            const Rectangle slot{bounds.x + 18 * scale + (index % columns) * cell,
                y + (index / columns) * cell, cell - 5 * scale, cell - 5 * scale};
            const bool over = CheckCollisionPointRec(pointer, slot);
            plate(slot, scale * 0.45F, over ? menuStyle::Hover : menuStyle::Inset, fade);
            drawIcon(entry.icon, {slot.x + 4 * scale, slot.y + 3 * scale,
                slot.width - 8 * scale, slot.height - 6 * scale}, fade);
            if (entry.level > 0) label(std::to_string(entry.level),
                {slot.x, slot.y + slot.height - 19 * scale, slot.width - 4 * scale, 19 * scale},
                16 * scale, menuStyle::Cream, fade, UiFontRole::BodyBold, 2);
            if (over) hovered = &entry;
        }
        y += static_cast<float>(std::max(std::size_t{1}, (group.size() + columns - 1) / columns)) * cell + 10 * scale;
    }
    return hovered ? std::optional<InventoryEntry>{*hovered} : std::nullopt;
}

void drawSelectionCorners(Rectangle bounds, float scale, float fade) {
    const float length = 15 * scale;
    const float thickness = 3 * scale;
    const float offset = 6 * scale;
    for (int xSide : {0, 1}) for (int ySide : {0, 1}) {
        const float x = xSide ? bounds.x + bounds.width + offset : bounds.x - offset;
        const float y = ySide ? bounds.y + bounds.height + offset : bounds.y - offset;
        const float left = xSide ? x - length : x;
        const float top = ySide ? y - length : y;
        DrawRectangleRec({left - scale, y - thickness * 0.5F - scale,
            length + scale * 2, thickness + scale * 2}, fadedColor(BLACK, fade));
        DrawRectangleRec({x - thickness * 0.5F - scale, top - scale,
            thickness + scale * 2, length + scale * 2}, fadedColor(BLACK, fade));
        DrawRectangleRec({left, y - thickness * 0.5F, length, thickness}, fadedColor(menuStyle::Cream, fade));
        DrawRectangleRec({x - thickness * 0.5F, top, thickness, length}, fadedColor(menuStyle::Cream, fade));
    }
}
} // namespace

RunUpgradeOverlayTransition runUpgradeOverlayTransition(double entranceSeconds, double exitSeconds, bool closing) {
    const float visibility = closing
        ? 1.0F - smoothUnit(static_cast<float>(
            (exitSeconds - RunUpgradeChoiceFeedbackDuration) / RunUpgradeOverlayExitDuration))
        : smoothUnit(static_cast<float>(entranceSeconds / RunUpgradeOverlayEntranceDuration));
    return {visibility, 0.4F + 0.6F * visibility};
}

Rectangle runUpgradeCardBounds(std::size_t index, std::size_t count) {
    const ScopedUiCoordinates coordinates;
    if (count == 0 || index >= count || index >= MaximumRunUpgradeChoices) return {};
    const auto layout = upgradeLayout(count);
    const Rectangle bounds = upgradeCardBounds(layout, index);
    return coordinates.owns() ? coordinates.viewport().toScreen(bounds) : bounds;
}
Rectangle runUpgradeRerollBounds(std::size_t count) {
    const ScopedUiCoordinates coordinates;
    const auto layout = upgradeLayout(count);
    const Rectangle bounds = upgradeRerollBounds(layout);
    return coordinates.owns() ? coordinates.viewport().toScreen(bounds) : bounds;
}
Rectangle runUpgradeUtilityBounds(std::size_t count,std::size_t action) {
    const ScopedUiCoordinates coordinates;
    const auto bounds=upgradeUtilityBounds(upgradeLayout(count),action);
    return coordinates.owns() ? coordinates.viewport().toScreen(bounds) : bounds;
}
std::optional<std::size_t> hoveredRunUpgradeChoice(const SimulationSnapshot& snapshot, Vector2 pointer) {
    const ScopedUiCoordinates coordinates;
    if (coordinates.owns()) pointer = coordinates.viewport().toUi(pointer);
    if (!snapshot.runUpgradeChoicePending) return std::nullopt;
    for (std::size_t index = 0; index < snapshot.runUpgradeChoiceCount; ++index)
        if (CheckCollisionPointRec(pointer, runUpgradeCardBounds(index, snapshot.runUpgradeChoiceCount))) return index;
    return std::nullopt;
}

static std::pair<FinalAttackStats,FinalAttackStats> buildingAttackStatsUpgradePreview(
    const SimulationSnapshot& snapshot, BuildingType type, RunUpgradeRarity rarity,
    std::uint64_t rollSeed) {
    const auto index = static_cast<std::size_t>(type);
    const auto effect = buildingRunUpgrade(type);
    if (!effect || index >= snapshot.buildingBlueprintLevels.size()) return {};
    const int level = std::clamp(snapshot.buildingBlueprintLevels[index] > 0
        ? static_cast<int>(snapshot.buildingBlueprintLevels[index])
        : snapshot.runUpgradeStacks[runUpgradeIndex(*effect)], 1, static_cast<int>(MaxStructureLevel));
    const auto base = [type](int tier) {
        const auto storedLevel = static_cast<std::uint8_t>(tier);
        if (type == BuildingType::Cannon || type == BuildingType::Catapult || type == BuildingType::Mortar)
            return CannonSystem::baseStats(type, storedLevel);
        if (type == BuildingType::SpikeTrap || type == BuildingType::SlowTrap)
            return TrapSystem::baseStats(type, storedLevel);
        return TowerSystem::baseStats(type, storedLevel);
    };
    auto upgraded = snapshot.buildingUpgradePowers;
    applyBuildingStatUpgrade(upgraded, type, rollBuildingStatUpgrade(type, rarity, rollSeed), rarity);
    const auto run = compatibleAttackModifiers(type,
        shrineDefenseAttackModifiers(snapshot.runUpgradePower, snapshot.shrines));
    const auto resolved = [&](int tier, const BuildingUpgradePowers& powers) {
        const std::array sources{run, buildingStatUpgradeModifiers(type, powers)};
        return resolveAttackStats(base(tier), sources);
    };
    return {resolved(level, snapshot.buildingUpgradePowers),
        resolved(std::min(level + 1, static_cast<int>(MaxStructureLevel)), upgraded)};
}

std::pair<double,double> buildingRangeUpgradePreview(
    const SimulationSnapshot& snapshot, BuildingType type, RunUpgradeRarity rarity,
    std::uint64_t rollSeed) {
    const auto [before, after] = buildingAttackStatsUpgradePreview(snapshot,type,rarity,rollSeed);
    const bool scalesWithSize = type == BuildingType::Flamethrower || type == BuildingType::WindBlower ||
        type == BuildingType::SlowTrap || type == BuildingType::SpikeTrap;
    return {before.range * (scalesWithSize ? before.size : 1.0),
        after.range * (scalesWithSize ? after.size : 1.0)};
}

std::pair<double,double> buildingAttackCountUpgradePreview(
    const SimulationSnapshot& snapshot, BuildingType type, RunUpgradeRarity rarity,
    std::uint64_t rollSeed) {
    const auto [before, after] = buildingAttackStatsUpgradePreview(snapshot,type,rarity,rollSeed);
    return {effectiveAttackCount(before.quantity,1),effectiveAttackCount(after.quantity,1)};
}

SimulationSnapshot previewRunUpgrade(const SimulationSnapshot& snapshot, std::size_t choiceIndex) {
    SimulationSnapshot projected = snapshot;
    if (choiceIndex >= snapshot.runUpgradeChoiceCount || choiceIndex >= MaximumRunUpgradeChoices) return projected;
    const auto effect = snapshot.runUpgradeChoices[choiceIndex];
    const auto index = runUpgradeIndex(effect);
    if (index >= RunUpgradeEffectCount ||
        static_cast<std::size_t>(snapshot.runUpgradeRarities[choiceIndex]) >= RunUpgradeRarityCount) return projected;
    if (const auto* blessing=shrineBlessing(snapshot.shrines.offer,effect)) {
        ++shrineBonusCounts(projected.shrines,snapshot.shrines.offer)[index];
        const double previousPower=shrineBonusPower(snapshot.shrines,snapshot.shrines.offer,effect);
        const double gain=effect==RunUpgradeEffect::UpgradeCore ? 0.0 :
            runUpgradeRarityMultiplier(snapshot.runUpgradeRarities[choiceIndex]);
        shrineBonusPowers(projected.shrines,snapshot.shrines.offer)[index]+=gain;
        const double ratio=(1+blessing->increment*(previousPower+gain)) /
            (1+blessing->increment*previousPower);
        switch(effect) {
        case RunUpgradeEffect::Damage: projected.playerDamageMultiplier*=ratio; break;
        case RunUpgradeEffect::AttackSpeed: projected.upgradeStats.playerAttackSpeed*=ratio; projected.upgradeStats.toolAttackSpeed*=ratio; break;
        case RunUpgradeEffect::MoveSpeed: projected.playerMoveSpeedMultiplier*=ratio; break;
        case RunUpgradeEffect::MaximumHealth:
            projected.playerMaxHealth+=blessing->increment*gain*fortitudePlayerHealthMultiplier(snapshot.runUpgradePower,snapshot.shrines);
            projected.upgradeStats.buildingHealth*=shrineMultiplier(projected.shrines,RunUpgradeEffect::BuildingHealth)/
                shrineMultiplier(snapshot.shrines,RunUpgradeEffect::BuildingHealth);
            break;
        case RunUpgradeEffect::RecoverableArmor: projected.playerMaxRecoverableArmor+=blessing->increment*gain; break;
        case RunUpgradeEffect::DefenseDamage: projected.upgradeStats.defenseDamage*=ratio; break;
        case RunUpgradeEffect::DefenseFireRate: projected.upgradeStats.defenseFireRate*=ratio; break;
        case RunUpgradeEffect::BuildingCapacity: {
            const int added=static_cast<int>(std::floor(previousPower+gain+1e-9)-std::floor(previousPower+1e-9));
            projected.totalBuildingLimit+=added;
            projected.buildingLimitBonus+=added;
            break;
        }
        case RunUpgradeEffect::BuildingHealth:
            projected.playerMaxHealth=(snapshot.playerMaxHealth-snapshot.playerTemporaryHealth)*
                fortitudePlayerHealthMultiplier(snapshot.runUpgradePower,projected.shrines)/
                fortitudePlayerHealthMultiplier(snapshot.runUpgradePower,snapshot.shrines)+snapshot.playerTemporaryHealth;
            projected.upgradeStats.buildingHealth*=shrineMultiplier(projected.shrines,effect)/shrineMultiplier(snapshot.shrines,effect);
            break;
        case RunUpgradeEffect::ProductionSpeed: projected.upgradeStats.productionSpeed*=ratio; break;
        case RunUpgradeEffect::ExperienceGain: projected.upgradeStats.experienceGain+=blessing->increment*gain; break;
        case RunUpgradeEffect::ResourceCapacity: projected.upgradeStats.resourceCapacity+=blessing->increment*gain; break;
        case RunUpgradeEffect::Luck: projected.upgradeStats.luck+=blessing->increment*gain; break;
        case RunUpgradeEffect::Regeneration: projected.upgradeStats.healthRegeneration+=blessing->increment*gain; break;
        case RunUpgradeEffect::GatheringSpeed: {
            const double power=snapshot.runUpgradePower[index]+previousPower;
            projected.upgradeStats.toolAttackSpeed*=(1+.15*(power+gain))/(1+.15*power);
            break;
        }
        case RunUpgradeEffect::ProjectileCount:
            projected.upgradeStats.projectileCountBonus+=std::floor(previousPower+gain+1e-9)-std::floor(previousPower+1e-9); break;
        case RunUpgradeEffect::Bounces:
            projected.upgradeStats.bounces+=std::floor(previousPower+gain+1e-9)-std::floor(previousPower+1e-9); break;
        case RunUpgradeEffect::Pierce:
            projected.upgradeStats.pierce+=std::floor(previousPower+gain+1e-9)-std::floor(previousPower+1e-9); break;
        case RunUpgradeEffect::ProjectileSpeed: projected.upgradeStats.projectileSpeed*=ratio; break;
        case RunUpgradeEffect::AttackDuration: projected.upgradeStats.attackDuration*=ratio; break;
        case RunUpgradeEffect::AttackSize: projected.upgradeStats.attackSize*=ratio; break;
        case RunUpgradeEffect::CriticalChance: projected.upgradeStats.critChance=std::min(1.0,projected.upgradeStats.critChance+blessing->increment*gain); break;
        case RunUpgradeEffect::CriticalDamage: projected.upgradeStats.critDamage*=ratio; break;
        case RunUpgradeEffect::AttackKnockback: projected.upgradeStats.knockback*=ratio; break;
        default: break;
        }
        return projected;
    }
    const double current = snapshot.runUpgradePower[index];
    const double passivePower=current+shrineBonusPower(snapshot.shrines,ShrineKind::Player,effect);
    const double gain = runUpgradeAppliedPowerGain(effect, snapshot.runUpgradeRarities[choiceIndex], current);
    if (gain <= 0.0) return projected;
    projected.runUpgradePower[index] += gain;
    ++projected.runUpgradeStacks[index];
    if (const auto building = runUpgradeBuilding(effect);
        building && runCombatBuilding(*building) && snapshot.runUpgradeStacks[index] > 0) {
        applyBuildingStatUpgrade(projected.buildingUpgradePowers, *building,
            rollBuildingStatUpgrade(*building, snapshot.runUpgradeRarities[choiceIndex],
                snapshot.runUpgradeRollSeeds[choiceIndex]), snapshot.runUpgradeRarities[choiceIndex]);
    }
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
    if (runDefenseCard(effect) && snapshot.runUpgradeStacks[runUpgradeIndex(effect)] == 0 &&
        std::find(RunEconomyBuildingTypes.begin(), RunEconomyBuildingTypes.end(),
            *runUpgradeBuilding(effect)) == RunEconomyBuildingTypes.end())
        ++projected.buildingArsenalCount;
    if (const auto weapon = runUpgradeWeapon(effect)) {
        const auto weaponIndex=static_cast<std::size_t>(*weapon);
        projected.weaponLevels[weaponIndex]=snapshot.unlockedWeapons[weaponIndex]
            ? std::min(MaximumPlayerWeaponLevel,std::max(1,snapshot.weaponLevels[weaponIndex])+1) : 1;
        projected.unlockedWeapons[weaponIndex] = true;
    }
    if (const auto style = runUpgradeCannonStyle(effect)) projected.upgradeStats.cannonAttackStyle = *style;
    switch (effect) {
    case RunUpgradeEffect::DefenseDamage:
        projected.upgradeStats.defenseDamage *= (1.0 + 0.12 * (current + gain)) / (1.0 + 0.12 * current);
        break;
    case RunUpgradeEffect::DefenseFireRate: projected.upgradeStats.defenseFireRate *= (1.0 + 0.10 * (current + gain)) / (1.0 + 0.10 * current); break;
    case RunUpgradeEffect::BuildingHealth:
        projected.playerMaxHealth=(snapshot.playerMaxHealth-snapshot.playerTemporaryHealth)*
            (1+.15*(current+gain))/(1+.15*current)+snapshot.playerTemporaryHealth;
        projected.upgradeStats.buildingHealth *= ratio(character ? character->buildingHealthMultiplier : 1.0, 0.15); break;
    case RunUpgradeEffect::ProductionSpeed: projected.upgradeStats.productionSpeed *= ratio(1.0, 0.12); break;
    case RunUpgradeEffect::ExperienceGain: projected.upgradeStats.experienceGain += .10 * gain; break;
    case RunUpgradeEffect::ResourceCapacity:
        projected.upgradeStats.resourceCapacity += .20*gain;
        break;
    case RunUpgradeEffect::AttackSpeed: break;
    case RunUpgradeEffect::Damage: projected.playerDamageMultiplier *= ratio(1.0, 0.10); break;
    case RunUpgradeEffect::MoveSpeed:
        projected.playerMoveSpeedMultiplier *= ratio(character ? character->moveSpeedMultiplier : 1.0, 0.08); break;
    case RunUpgradeEffect::MaximumHealth:
        projected.playerMaxHealth += 12.0 * gain * fortitudePlayerHealthMultiplier(snapshot.runUpgradePower,snapshot.shrines); break;
    case RunUpgradeEffect::RecoverableArmor: projected.playerMaxRecoverableArmor += 8.0 * gain; break;
    case RunUpgradeEffect::BuildingCapacity:
    case RunUpgradeEffect::ExtraBuilding: {
        const int extra = runUpgradeDiscreteAmount(effect, current + gain) - runUpgradeDiscreteAmount(effect, current);
        projected.totalBuildingLimit += extra;
        projected.buildingLimitBonus += extra;
        break;
    }
    case RunUpgradeEffect::BuildRadius: projected.coreBuildRadius += static_cast<int>(std::floor(3.0 * (current + gain)) - std::floor(3.0 * current)); break;
    case RunUpgradeEffect::GatheringSpeed: projected.upgradeStats.toolAttackSpeed *= (1+.15*(passivePower+gain))/(1+.15*passivePower); break;
    case RunUpgradeEffect::Luck: projected.upgradeStats.luck += 0.10 * gain; break;
    case RunUpgradeEffect::Regeneration: projected.upgradeStats.healthRegeneration += 0.5 * gain; break;
    case RunUpgradeEffect::ProjectileCount:
        projected.upgradeStats.projectileCountBonus = runUpgradeDiscreteAmount(effect, current + gain); break;
    case RunUpgradeEffect::Pierce:
        projected.upgradeStats.pierce = runUpgradeDiscreteAmount(effect, current + gain); break;
    case RunUpgradeEffect::Bounces:
        projected.upgradeStats.bounces = runUpgradeDiscreteAmount(effect, current + gain); break;
    case RunUpgradeEffect::ClusterPayload:
        projected.upgradeStats.clusterExplosions = 2.0 * (current + gain); break;
    default: break;
    }
    const auto modifiers = shrineDefenseAttackModifiers(projected.runUpgradePower,projected.shrines);
    projected.upgradeStats.projectileCountBonus=modifiers[AttackStat::Quantity].flat;
    projected.upgradeStats.bounces=modifiers[AttackStat::Bounces].flat;
    projected.upgradeStats.pierce=modifiers[AttackStat::Pierce].flat;
    projected.upgradeStats.projectileSpeed = modifiers[AttackStat::ProjectileSpeed].multiplier;
    projected.upgradeStats.attackDuration = modifiers[AttackStat::Duration].multiplier;
    projected.upgradeStats.attackSize = modifiers[AttackStat::Size].multiplier;
    projected.upgradeStats.critChance = std::clamp(modifiers[AttackStat::CritChance].flat, 0.0, 1.0);
    projected.upgradeStats.critDamage = 2.0 * modifiers[AttackStat::CritDamage].multiplier;
    projected.upgradeStats.knockback = modifiers[AttackStat::Knockback].multiplier;
    return projected;
}

void drawRunUpgradeOverlay(GameUi& ui, const SimulationSnapshot& snapshot,
    double entranceSeconds, double exitSeconds, std::optional<std::size_t> selectedChoice,
    std::span<const float> hoverAmounts, std::optional<Vector2> pointerPosition,
    std::optional<std::size_t> keyboardChoice, bool reduceFlashes, bool banishArmed) {
    if (!snapshot.runUpgradeChoicePending || snapshot.runUpgradeChoiceCount == 0) return;
    const ScopedUiCanvas canvas;
    if (pointerPosition && canvas.owns()) pointerPosition = canvas.viewport().toUi(*pointerPosition);
    const Vector2 pointer = pointerPosition.value_or(uiMousePosition());
    const auto baseLayout = upgradeLayout(snapshot.runUpgradeChoiceCount);
    const bool closing = selectedChoice.has_value() || exitSeconds > 0;
    const auto transition = runUpgradeOverlayTransition(entranceSeconds, exitSeconds, closing);
    const float fade = transition.opacity;
    if (fade <= 0) return;
    const auto layout = scaledUpgradeLayout(baseLayout, transition.scale);
    const float s = layout.scale;
    const float selectionProgress = std::clamp(static_cast<float>(
        exitSeconds / RunUpgradeChoiceFeedbackDuration), 0.0F, 1.0F);
    const float selectionPulse = std::sin(selectionProgress * PI) * (reduceFlashes ? 0.2F : 1.0F);
    const bool interactive = !closing && entranceSeconds >= RunUpgradeOverlayEntranceDuration;
    DrawRectangle(0, 0, uiScreenWidth(), uiScreenHeight(), fadedColor({7, 12, 19, 175}, fade));
    reward_backdrop::draw(ui,reward_backdrop::Kind::Experience,entranceSeconds+exitSeconds,
        {static_cast<float>(uiScreenWidth()),static_cast<float>(uiScreenHeight())},fade,reduceFlashes);
    drawUpgradeRays(baseLayout.offers, entranceSeconds + exitSeconds, fade, false, reduceFlashes);
    const auto hovered = interactive ? hoveredRunUpgradeChoice(snapshot, pointer) : std::nullopt;
    if (keyboardChoice && *keyboardChoice >= snapshot.runUpgradeChoiceCount) keyboardChoice.reset();
    const auto preview = closing ? selectedChoice : interactive ? (keyboardChoice ? keyboardChoice : hovered) : std::nullopt;
    plate(layout.offers, s, {18, 21, 19, 255}, fade);
    const Rectangle header{layout.offers.x, layout.offers.y, layout.offers.width, 72 * s};
    plate(header, s, menuStyle::Card, fade);
    const bool shrineOffer = snapshot.shrines.offer != ShrineKind::None;
    label("UPGRADES", {header.x + 20 * s, header.y, header.width - 40 * s, header.height},
        36 * s, menuStyle::Cream, fade, UiFontRole::Display, 1);
    for (std::size_t index = 0; index < snapshot.runUpgradeChoiceCount; ++index) {
        const auto effect = snapshot.runUpgradeChoices[index];
        const auto rarity = snapshot.runUpgradeRarities[index];
        const bool selected = closing && selectedChoice == index;
        const float cardScale = s * (selected ? 1.0F + selectionPulse * 0.035F : 1.0F);
        Rectangle row = upgradeCardBounds(layout, index);
        if (selected) {
            const float growth = selectionPulse * 0.035F;
            row = {row.x - row.width * growth * 0.5F, row.y - row.height * growth * 0.5F,
                row.width * (1.0F + growth), row.height * (1.0F + growth)};
        }
        const int price = runUpgradeChoicePrice(index, snapshot.runUpgradePaidChoicePrice);
        const bool affordable = canSelectRunUpgradeChoice(snapshot, index);
        const float otherCardFade = closing && !selected
            ? 1.0F - 0.65F * smoothUnit(static_cast<float>(exitSeconds / 0.16)) : 1.0F;
        const float opacity = fade * (affordable ? 1.0F : 0.35F) * otherCardFade;
        const bool active = preview == index || (selectedChoice && *selectedChoice == index);
        const Color accent = rarityColor(rarity);
        const float hover = interactive && index < hoverAmounts.size() ? std::clamp(hoverAmounts[index], 0.0F, 1.0F)
            : active ? 1.0F : 0.0F;
        const Color cardBase = rarityFill(rarity);
        if (selected && selectionPulse > 0.001F) {
            const float rim = 7 * cardScale;
            plate({row.x - rim, row.y - rim, row.width + rim * 2, row.height + rim * 2},
                cardScale, accent, fade * selectionPulse * 0.7F);
        }
        plate(row, cardScale, tinted(cardBase, accent,
            hover * 0.08F + (selected ? selectionPulse * 0.18F : 0.0F)), opacity);
        const float iconSize = std::min(row.height - 44 * cardScale, 132 * cardScale);
        const Rectangle icon{row.x + 16 * cardScale, row.y + row.height - iconSize - 12 * cardScale, iconSize, iconSize};
        // Flat rarity colour, matching the reference; no glow or radial wash.
        plate(icon, cardScale * 0.55F, tinted(cardBase, accent, 0.36F + hover * 0.08F), opacity);
        const auto* blessing=shrineBlessing(snapshot.shrines.offer,effect);
        label(runUpgradeRarityName(rarity), {icon.x - 4 * cardScale, row.y + 9 * cardScale, icon.width + 8 * cardScale, 23 * cardScale},
            15 * cardScale, accent, opacity, UiFontRole::BodyBold, 1);
        const float iconPop = selected ? 1.0F + selectionPulse * 0.16F : 1.0F;
        const float artworkSize = (icon.width - 8 * cardScale) * iconPop;
        const float iconRotation = selected && !reduceFlashes
            ? std::sin(selectionProgress * 2 * PI) * 4.0F * (1.0F - selectionProgress) : 0.0F;
        drawIcon(ui.runUpgradeTexture(blessing ? blessing->icon : effect),
            {icon.x + (icon.width - artworkSize) * 0.5F,
             icon.y + (icon.height - artworkSize) * 0.5F, artworkSize, artworkSize}, opacity, iconRotation);
        const float left = icon.x + icon.width + 20 * cardScale;
        const float width = row.x + row.width - left - 24 * cardScale;
        const float badgeWidth = std::min(148 * cardScale, width * 0.25F);
        const float descriptionWidth = width - (price > 0 ? 125 * cardScale : 0);
        const auto description = describeOffer(snapshot, index, descriptionWidth, cardScale);
        const float descriptionHeight = static_cast<float>(description.lineCount()) * 43.2F * cardScale;
        const float blockHeight = 40 * cardScale + descriptionHeight;
        const float top = row.y + (row.height - blockHeight) * 0.5F;
        label(blessing ? blessing->name : runUpgradeDefinition(effect).name,
            {left, top, width - badgeWidth - 8 * cardScale, 34 * cardScale},
            30 * cardScale, menuStyle::Cream, opacity, UiFontRole::Display);
        drawOfferDescription(description, {left, top + 40 * cardScale,
            descriptionWidth, descriptionHeight}, cardScale, opacity);
        const auto weapon=runUpgradeWeapon(effect);
        const int level=weapon && snapshot.unlockedWeapons[static_cast<std::size_t>(*weapon)]
            ? std::max(1,snapshot.weaponLevels[static_cast<std::size_t>(*weapon)]) : runTomeLevel(effect,snapshot.runUpgradeStacks);
        const std::string levelCopy = effect == RunUpgradeEffect::UpgradeCore ? "CORE"
            : level == 0 ? "NEW" : "LVL " + std::to_string(level + 1);
        label(shrineOffer && effect != RunUpgradeEffect::UpgradeCore ? "BLESSING" : levelCopy, {left + width - badgeWidth, row.y + 18 * cardScale, badgeWidth, 26 * cardScale},
            21.25F * cardScale, level == 0 && effect != RunUpgradeEffect::UpgradeCore ? menuStyle::Gold : accent,
            opacity, UiFontRole::Display, 2);
        if (selected) {
            drawSelectionCorners(row, cardScale, fade);
            if (!reduceFlashes && selectionPulse > 0.001F) {
                const Vector2 center{icon.x + icon.width * 0.5F, icon.y + icon.height * 0.5F};
                for (int spark = 0; spark < 8; ++spark) {
                    const float angle = static_cast<float>(spark) * (2 * PI / 8) + 0.2F;
                    const float radius = (48 + 52 * selectionProgress) * cardScale;
                    const Vector2 point{center.x + std::cos(angle) * radius,
                        center.y + std::sin(angle) * radius};
                    const float reach = (3 + 5 * selectionPulse) * cardScale;
                    const Color glint = fadedColor(accent, fade * selectionPulse * 0.85F);
                    DrawLineEx({point.x - reach, point.y}, {point.x + reach, point.y}, 2 * cardScale, glint);
                    DrawLineEx({point.x, point.y - reach}, {point.x, point.y + reach}, 2 * cardScale, glint);
                }
            }
        }
        if (price > 0) {
            const ScopedUiFontRole priceFont(UiFontRole::Display);
            const std::string priceText = std::to_string(price);
            const float priceIconSize = 44 * cardScale;
            const float numberSize = fitUiTextSize(priceText, 36 * cardScale, 21.6F * cardScale,
                150 * cardScale, priceIconSize);
            const float numberWidth = measureUiText(priceText, numberSize).x;
            const float right = row.x + row.width - 24 * cardScale;
            const float priceTop = row.y + row.height - 56 * cardScale;
            drawIcon(ui.resourceTexture(UiResourceIcon::Coin),
                {right - numberWidth - priceIconSize, priceTop, priceIconSize, priceIconSize}, fade);
            label(priceText, {right - numberWidth, priceTop, numberWidth, priceIconSize},
                numberSize, affordable ? menuStyle::Cream : Color{245, 102, 91, 255},
                fade, UiFontRole::Display, 2);
        }
    }
    // One focus frame moves between rows instead of leaving four fading frames.
    Rectangle focus{};
    float weight = 0;
    if (interactive) {
        for (std::size_t index = 0; index < std::min(hoverAmounts.size(), snapshot.runUpgradeChoiceCount); ++index) {
            const float amount = std::clamp(hoverAmounts[index], 0.0F, 1.0F);
            const auto bounds = upgradeCardBounds(layout, index);
            focus.x += bounds.x * amount;
            focus.y += bounds.y * amount;
            focus.width += bounds.width * amount;
            focus.height += bounds.height * amount;
            weight += amount;
        }
    }
    if (weight > 0.01F) {
        focus = {focus.x / weight, focus.y / weight, focus.width / weight, focus.height / weight};
        drawSelectionCorners(focus, s, fade * std::min(weight, 1.0F));
    } else if (preview && !closing) {
        focus = upgradeCardBounds(layout, *preview);
        drawSelectionCorners(focus, s, fade);
    }
    if (!shrineOffer && (snapshot.runUpgradeRerollTokens>0 || snapshot.runUpgradeSkipTokens>0 || snapshot.runUpgradeBanishTokens>0)) {
        const std::array counts{snapshot.runUpgradeRerollTokens,snapshot.runUpgradeSkipTokens,snapshot.runUpgradeBanishTokens};
        constexpr std::array names{"R  REROLL","X  SKIP","Z  BANISH"};
        for(std::size_t action=0;action<counts.size();++action) {
            const auto bounds=upgradeUtilityBounds(layout,action);
            const bool enabled=counts[action]>0;
            const bool selected=action==2 && banishArmed;
            plate(bounds,s*.65F,selected ? Color{113,52,49,255} :
                enabled && interactive && CheckCollisionPointRec(pointer,bounds) ? menuStyle::Selected : menuStyle::Card,fade);
            label(localizeText(names[action])+"  "+std::to_string(counts[action]),bounds,16*s,
                enabled ? (selected ? Color{255,146,121,255} : menuStyle::Cream) : menuStyle::Muted,
                fade,UiFontRole::Display,1);
        }
        if(banishArmed) label("CHOOSE A CARD TO BANISH",{layout.offers.x,layout.offers.y-30*s,layout.offers.width,24*s},
            17*s,{255,146,121,255},fade,UiFontRole::Display,1);
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
        preview.runUpgradeRollSeeds[i] = snapshot.runShop.offers[i].rollSeed;
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
        const bool iconButton=focus==8 || focus<4;
        if(iconButton) drawIcon(ui.shopActionTexture(focus==8 ? ShopActionIcon::Reroll : ShopActionIcon::Purchase),
            {r.x+12*s,r.y+(r.height-36*s)*.5F,36*s,36*s},fade);
        label(text,{r.x+(iconButton ? 50*s : 8*s),r.y,r.width-(iconButton ? 58*s : 16*s),r.height},23*s,(enabled || focus < 4) ? color : menuStyle::Muted,fade,UiFontRole::Display,1);
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
        shopText(localizedUpgradeDescription(snapshot,offer.effect,offer.rarity,offer.rollSeed),
            {r.x+18*s,r.y+322*s,r.width-36*s,194*s},22*s,menuStyle::Cream,s,6,fade);
        const bool buyable = snapshot.runShopBuyable[i];
        const bool affordable = snapshot.unlimitedResources || snapshot.coins >= offer.price;
        const std::string price = buyable ? std::to_string(snapshot.unlimitedResources ? 0 : offer.price) : localizeText("LIMIT REACHED");
        button(l.buy[i],price,buyable && affordable,static_cast<int>(i),
            affordable ? menuStyle::Cream : Color{255,100,95,255});
        button(l.locks[i],offer.locked ? "OFFER LOCKED" : "LOCK",buyable || offer.locked,
            static_cast<int>(i+4),offer.locked ? menuStyle::Gold : menuStyle::Cream);
        if (hovered == i) drawSelectionCorners(r,s,fade);
    }
    button(l.proceed,"CONTINUE",true,9,menuStyle::Gold);
    label("R: REROLL / 1-4: BUY / ESC: CONTINUE",{l.header.x, l.proceed.y,820*s,30*s},
        19*s,menuStyle::Muted,fade,UiFontRole::BodyBold);
    label("Locked offers carry over. Prices rise every wave.",
        {l.header.x,l.proceed.y+35*s,850*s,28*s},18*s,menuStyle::Muted,fade);
    if (inventoryHover) {
        std::string detail = localizeText(inventoryHover->description);
        if (inventoryHover->buildingType) {
            const auto bonus = buildingUpgradeSummary(*inventoryHover->buildingType,
                snapshot.buildingUpgradePowers);
            if (!bonus.empty()) detail += "  " + bonus;
        }
        drawItemTooltip(inventoryHover->title, detail, pointer,
            {static_cast<float>(uiScreenWidth()),static_cast<float>(uiScreenHeight())}, s);
    }
}
} // namespace ian::app_detail
