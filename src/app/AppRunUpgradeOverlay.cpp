#include "app/AppRunUpgradeOverlay.hpp"

#include "progression/ProgressionCardRules.hpp"
#include "ui/UiText.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

namespace ian::app_detail {
namespace {

constexpr float CardWidth = 360.0F;
constexpr float CardHeight = 520.0F;
constexpr float CardGap = 64.0F;
constexpr Color CardBrown{0x6E, 0x37, 0x21, 255};
constexpr Color CardBlack{0, 0, 0, 255};
constexpr Color CardShadowBlack{0, 0, 0, 64};
constexpr Color CardInset{0x34, 0x22, 0x15, 255};
constexpr Color CardInk{20, 21, 20, 255};
constexpr Color CardValueCream{0xF9, 0xE8, 0xCD, 255};
constexpr Color LevelHighlight{0x98, 0x6D, 0x47, 255};
constexpr Color LevelShadow{0x2E, 0x24, 0x19, 255};
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

float cardLayoutScale(std::size_t count) {
    const float screenWidth = static_cast<float>(GetScreenWidth());
    const float screenHeight = static_cast<float>(GetScreenHeight());
    const float horizontal =
        (screenWidth - 72.0F) /
        (CardWidth * static_cast<float>(count) +
         CardGap * static_cast<float>(count - 1U));
    const float vertical = (screenHeight - 350.0F) / CardHeight;
    return std::clamp(std::min(horizontal, vertical), 0.50F, 1.0F);
}

UpgradeColorClass upgradeColorClass(RunUpgradeEffect effect) {
    switch (effect) {
    case RunUpgradeEffect::Damage:
    case RunUpgradeEffect::AttackSpeed:
    case RunUpgradeEffect::MaximumHealth:
    case RunUpgradeEffect::BloodHarvest:
        return UpgradeColorClass::Red;
    case RunUpgradeEffect::MoveSpeed:
        return UpgradeColorClass::Blue;
    case RunUpgradeEffect::RecoverableArmor:
    case RunUpgradeEffect::BuildingHealth:
    case RunUpgradeEffect::BuildRadius:
    case RunUpgradeEffect::DefenseDamage:
    case RunUpgradeEffect::DefenseFireRate:
    case RunUpgradeEffect::ClusterPayload:
    case RunUpgradeEffect::ReactiveTraps:
        return UpgradeColorClass::Yellow;
    case RunUpgradeEffect::ProductionSpeed:
    case RunUpgradeEffect::NightlyBomb:
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
    }
    return UpgradeColorClass::Blue;
}

std::string_view upgradeMark(RunUpgradeEffect effect) {
    switch (effect) {
    case RunUpgradeEffect::Damage: return "DMG";
    case RunUpgradeEffect::AttackSpeed: return "SPD";
    case RunUpgradeEffect::MoveSpeed: return "MOVE";
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
    case RunUpgradeEffect::ClusterPayload: return "BURST";
    case RunUpgradeEffect::ReactiveTraps: return "PULSE";
    }
    return "+";
}

UpgradeColorClass skillColorClass(SkillBranch branch) {
    switch (branch) {
    case SkillBranch::Gathering: return UpgradeColorClass::Green;
    case SkillBranch::Weapons: return UpgradeColorClass::Red;
    case SkillBranch::Construction: return UpgradeColorClass::Yellow;
    case SkillBranch::Movement: return UpgradeColorClass::Blue;
    case SkillBranch::Economy: return UpgradeColorClass::Purple;
    case SkillBranch::Root: return UpgradeColorClass::Blue;
    }
    return UpgradeColorClass::Blue;
}

std::string_view skillMark(SkillBranch branch) {
    switch (branch) {
    case SkillBranch::Gathering: return "GATHER";
    case SkillBranch::Weapons: return "COMBAT";
    case SkillBranch::Construction: return "BUILD";
    case SkillBranch::Movement: return "MOVE";
    case SkillBranch::Economy: return "ECON";
    case SkillBranch::Root: return "CORE";
    }
    return "+";
}

std::vector<std::string> wrappedLines(
    std::string_view text, float fontSize, float maximumWidth) {
    std::vector<std::string> lines;
    std::string line;
    std::size_t position = 0U;
    while (position < text.size()) {
        while (position < text.size() && text[position] == ' ') ++position;
        const std::size_t end = text.find(' ', position);
        const std::string word{text.substr(
            position, end == std::string_view::npos
                ? text.size() - position : end - position)};
        const std::string candidate = line.empty()
            ? word : line + " " + word;
        if (!line.empty() &&
            measureUiText(candidate, fontSize).x > maximumWidth) {
            lines.push_back(line);
            line = word;
        } else {
            line = candidate;
        }
        if (end == std::string_view::npos) break;
        position = end + 1U;
    }
    if (!line.empty()) lines.push_back(line);
    return lines;
}

float smoothUnit(float value) {
    const float clamped = std::clamp(value, 0.0F, 1.0F);
    return clamped * clamped * (3.0F - 2.0F * clamped);
}

float cardEntranceScale(float value) {
    const float t = std::clamp(value, 0.0F, 1.0F);
    constexpr float Overshoot = 1.70158F;
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
    for (int y = -radius; y <= radius; ++y) {
        for (int x = -radius; x <= radius; ++x) {
            const float distanceSquared =
                static_cast<float>(x * x + y * y);
            if (distanceSquared > radiusSquared) continue;
            DrawTextEx(
                font, text.c_str(),
                {position.x + static_cast<float>(x),
                 position.y + static_cast<float>(y)},
                fontSize, spacing, fadedColor(outline, opacity));
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

std::string uppercaseAscii(std::string_view text) {
    std::string result{text};
    std::ranges::transform(result, result.begin(), [](unsigned char value) {
        return static_cast<char>(std::toupper(value));
    });
    return result;
}

struct UpgradeValueCopy {
    std::string value;
    std::string label;
};

struct SkillCardCopy {
    std::string_view title;
    std::string_view value;
    std::string_view label;
};

SkillCardCopy skillCardCopy(const SkillNodeDefinition& skill) {
    const std::string_view id = skill.id;
    if (id == "axe") return {"AXE", "+15%", "AXE DAMAGE"};
    if (id == "pickaxe") {
        return {"PICKAXE", "+15%", "PICKAXE DAMAGE"};
    }
    if (id == "efficient_strikes") {
        return {"GATHER", "+25%", "RESOURCE DAMAGE"};
    }
    if (id == "power_swing") {
        return {"CLEAVE", "3RD HIT", "AREA STRIKE"};
    }
    if (id == "wide_swing") {
        return {"WIDE SWING", "+50%", "CLEAVE AREA"};
    }
    if (id == "hands_on") {
        return {"HANDS ON", "+45%", "MANUAL GATHERING"};
    }
    if (id == "industrialist") {
        return {"INDUSTRY", "+40%", "PRODUCTION SPEED"};
    }
    if (id == "lumber_mill") {
        return {"LUMBER MILL", "UNLOCK", "WOOD PRODUCTION"};
    }
    if (id == "quarry") {
        return {"QUARRY", "UNLOCK", "STONE PRODUCTION"};
    }
    if (id == "crystal_mine") {
        return {"CRYSTAL MINE", "UNLOCK", "CRYSTAL PRODUCTION"};
    }
    if (id == "night_shift") {
        return {"NIGHT SHIFT", "+50%", "NIGHT PRODUCTION"};
    }
    if (id == "combat_training") {
        return {"POWER", "+10%", "ATTACK DAMAGE"};
    }
    if (id == "club") return {"CLUB", "UNLOCK", "MELEE WEAPON"};
    if (id == "rifle") return {"RIFLE", "UNLOCK", "RANGED WEAPON"};
    if (id == "ice_wand") {
        return {"ICE WAND", "UNLOCK", "FREEZE MAGIC"};
    }
    if (id == "fire_wand") {
        return {"FIRE WAND", "UNLOCK", "FIRE MAGIC"};
    }
    if (id == "bombs") return {"BOMBS", "+2", "BOMBS EACH NIGHT"};
    if (id == "bomb_pouch") {
        return {"BOMB POUCH", "+1", "NIGHTLY BOMB"};
    }
    if (id == "concussive_swings") {
        return {"KNOCKBACK", "+3", "CLUB KNOCKBACK"};
    }
    if (id == "bruiser") {
        return {"BRUISER", "+55%", "CLUB DAMAGE"};
    }
    if (id == "crowd_breaker") {
        return {"CROWD BREAKER", "+55%", "CLUB AREA"};
    }
    if (id == "marksman") {
        return {"MARKSMAN", "+70%", "RIFLE DAMAGE"};
    }
    if (id == "assault_rifle") {
        return {"ASSAULT", "+50%", "RIFLE SPEED"};
    }
    if (id == "deep_freeze") {
        return {"DEEP FREEZE", "+55%", "FREEZE TIME"};
    }
    if (id == "ice_lance") {
        return {"ICE LANCE", "+65%", "ICE DAMAGE"};
    }
    if (id == "wildfire") {
        return {"WILDFIRE", "+50%", "BURN TIME"};
    }
    if (id == "inferno") {
        return {"INFERNO", "+45%", "FIRE DAMAGE"};
    }
    if (id == "thermal_shock") {
        return {"THERMAL SHOCK", "+18", "BURST DAMAGE"};
    }
    if (id == "hammer") {
        return {"FORTIFY", "10 SEC", "FORTIFICATION"};
    }
    if (id == "reinforced_frames") {
        return {"FORTIFY", "+20%", "BUILDING HP"};
    }
    if (id == "field_repairs") {
        return {"REPAIR", "+18%", "NIGHTLY REPAIR"};
    }
    if (id == "defense_engineering") {
        return {"TOWER POWER", "+12%", "DEFENSE DAMAGE"};
    }
    if (id == "crossbow_unlock") {
        return {"CROSSBOW", "UNLOCK", "DEFENSE"};
    }
    if (id == "turret_calibration") {
        return {"CALIBRATION", "+20%", "RANGE & DAMAGE"};
    }
    if (id == "rapid_battery") {
        return {"RAPID FIRE", "+45%", "TOWER SPEED"};
    }
    if (id == "long_watch") {
        return {"LONG WATCH", "+45%", "TOWER RANGE"};
    }
    if (id == "cannon_unlock") {
        return {"CANNON", "UNLOCK", "DEFENSE"};
    }
    if (id == "catapult_unlock") {
        return {"CATAPULT", "UNLOCK", "DEFENSE"};
    }
    if (id == "artillery_corps") {
        return {"ARTILLERY", "+30%", "BLAST & DAMAGE"};
    }
    if (id == "trap_engineer") {
        return {"TRAPS", "+30%", "TRAP DAMAGE"};
    }
    if (id == "high_ground") {
        return {"HIGH GROUND", "+35%", "DEFENSE DAMAGE"};
    }
    if (id == "light_footwork") {
        return {"MOVE", "+15%", "MOVE SPEED"};
    }
    if (id == "sprinter") {
        return {"SPRINT", "+10%", "MOVE SPEED"};
    }
    if (id == "dash") return {"DASH", "UNLOCK", "MOVEMENT SKILL"};
    if (id == "impact_dash") {
        return {"IMPACT DASH", "+10", "DASH DAMAGE"};
    }
    if (id == "long_dash") {
        return {"LONG DASH", "+30%", "DASH SPEED"};
    }
    if (id == "rapid_dash") {
        return {"RAPID DASH", "+45%", "DASH RECOVERY"};
    }
    if (id == "nightly_chest") {
        return {"SUPPLY DROP", "3 NIGHTS", "REWARD CHEST"};
    }
    if (id == "frequent_bounty") {
        return {"DELIVERY", "+1", "CHEST EACH NIGHT"};
    }
    if (id == "safe_delivery") {
        return {"SAFE DROP", "NEAR CORE", "REWARD CHESTS"};
    }
    if (id == "keymaster") {
        return {"KEYMASTER", "-20%", "CHEST COST"};
    }
    if (id == "early_planning") {
        return {"PLANNING", "+25%", "EARLY REWARDS"};
    }
    if (id == "mercenary_contract") {
        return {"MERCENARY", "BONUS", "COINS, NO XP"};
    }
    if (id == "scholar_contract") {
        return {"SCHOLAR", "BONUS", "XP, NO COINS"};
    }
    if (id == "expanded_storage") {
        return {"STORAGE", "+40%", "RESOURCE CAPACITY"};
    }
    if (id == "scavenger") {
        return {"SCAVENGE", "+50%", "PROP COINS"};
    }
    if (id == "longer_days") {
        return {"LONG DAYS", "+15 SEC", "DAY TIME"};
    }
    return {skill.title, skillMark(skill.branch), "SPECIAL UPGRADE"};
}

UpgradeValueCopy upgradeValueCopy(
    std::string_view description, std::string_view fallback) {
    if (description.size() >= 2U &&
        (description.front() == '+' || description.front() == '-') &&
        std::isdigit(static_cast<unsigned char>(description[1]))) {
        const std::size_t separator = description.find(' ');
        if (separator != std::string_view::npos) {
            std::string label = uppercaseAscii(
                description.substr(separator + 1U));
            if (!label.empty() && label.back() == '.') label.pop_back();
            return {
                std::string{description.substr(0U, separator)},
                std::move(label),
            };
        }
    }
    return {std::string{fallback}, "SPECIAL UPGRADE"};
}

} // namespace

Rectangle runUpgradeCardBounds(std::size_t index, std::size_t count) {
    const float screenWidth = static_cast<float>(GetScreenWidth());
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

void drawRunUpgradeOverlay(
    GameUi& ui,
    const SimulationSnapshot& snapshot,
    const SkillTree& skillTree,
    double entranceSeconds,
    double exitSeconds,
    std::optional<std::size_t> selectedChoice,
    std::span<const float> hoverAmounts) {
    if (!snapshot.runUpgradeChoicePending) return;
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
                      std::lround(242.0F * screenFade))});
    const float headingEntrance = std::clamp(static_cast<float>(
        (entranceSeconds - 0.01) / 0.25), 0.0F, 1.0F);
    const float headingMotion = cardEntranceScale(headingEntrance);
    const float headingFade = smoothUnit(headingEntrance) * overlayFade;
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
        const float bannerWidth = std::min({
            940.0F,
            static_cast<float>(GetScreenWidth()) - 60.0F,
            maximumBannerWidth,
        });
        const float bannerHeight = bannerWidth *
            static_cast<float>(levelUpBanner.height) /
            static_cast<float>(levelUpBanner.width);
        const float bannerScale =
            (0.80F + headingMotion * 0.20F) *
            (1.0F - exitEase * 0.06F);
        const Rectangle bannerBase{
            (static_cast<float>(GetScreenWidth()) - bannerWidth) * 0.5F,
            cardTop - bannerHeight - 64.0F -
                (1.0F - smoothUnit(headingEntrance)) * 44.0F -
                exitEase * 28.0F,
            bannerWidth, bannerHeight,
        };
        const Rectangle banner = scaleRectangle(
            bannerBase, bannerScale, 0.0F);
        DrawTexturePro(
            levelUpBanner,
            {0.0F, 0.0F, static_cast<float>(levelUpBanner.width),
             static_cast<float>(levelUpBanner.height)},
            banner, {0.0F, 0.0F}, 0.0F,
            fadedColor(WHITE, headingFade));
    }

    const Vector2 mouse = GetMousePosition();
    for (std::size_t index = 0; index < snapshot.runUpgradeChoiceCount;
         ++index) {
        const ProgressionCardId card = snapshot.runUpgradeChoices[index];
        const bool skillCard = isSkillProgressionCard(card);
        const std::size_t skillIndex = skillCard
            ? progressionCardSkillIndex(card) : 0U;
        if (skillCard && skillIndex >= skillTree.nodes().size()) continue;
        const RunUpgradeEffect effect = skillCard
            ? RunUpgradeEffect::Damage
            : progressionCardRunUpgrade(card);
        const SkillNodeDefinition* skill = skillCard
            ? &skillTree.nodes()[skillIndex] : nullptr;
        const SkillCardCopy skillCopy = skillCard
            ? skillCardCopy(*skill) : SkillCardCopy{};
        const std::string_view title = skillCard
            ? skillCopy.title
            : runUpgradeDefinition(effect).name;
        const std::string_view description = skillCard
            ? std::string_view{skill->description}
            : runUpgradeDefinition(effect).description;
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
            (entranceSeconds - cardDelay) / 0.30), 0.0F, 1.0F);
        if (entrance <= 0.0F) continue;
        const float entranceFade = smoothUnit(entrance);
        const bool selectedForExit = selectedChoice &&
            *selectedChoice == index;
        const float cardExitFade = selectedForExit
            ? 1.0F - smoothUnit(std::clamp(
                  (exitProgress - 0.62F) / 0.38F, 0.0F, 1.0F))
            : overlayFade;
        const float cardFade = entranceFade * cardExitFade;
        const float exitScale = selectedForExit
            ? 1.0F + exitEase * 0.13F
            : 1.0F - exitEase * 0.10F;
        const float animationScale = std::max(
            0.001F,
            cardEntranceScale(entrance) *
                (1.0F + hover * 0.045F) * exitScale);
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
                (static_cast<float>(GetScreenWidth()) * 0.5F -
                 selectedCenterX) * exitEase;
        }
        const float scale = layoutScale * animationScale;
        const bool locked = !skillCard &&
            snapshot.lockedRunUpgrade == effect;
        const UpgradePalette palette = upgradePalette(
            skillCard
                ? skillColorClass(skill->branch)
                : upgradeColorClass(effect));
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
            (1.0F + hover * 0.55F +
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
            centerX, bounds.y + 140.0F * scale};

        const std::string_view mark = skillCard
            ? skillMark(skill->branch)
            : upgradeMark(effect);
        const RunUpgradeEffect iconEffect =
            skillCard && skill->branch == SkillBranch::Movement
                ? RunUpgradeEffect::MoveSpeed : effect;
        const Texture2D cardIcon = ui.runUpgradeTexture(iconEffect);
        const bool drawUpgradeIcon = IsTextureValid(cardIcon) &&
            (!skillCard || skill->branch == SkillBranch::Movement);
        if (drawUpgradeIcon) {
            const float iconSize = 320.0F * scale;
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
            const float emblemRadius = 93.0F * scale;
            DrawCircleV(
                emblemCenter, emblemRadius + 8.0F * scale,
                fadedColor(CardInk, cardFade));
            DrawCircleV(
                emblemCenter, emblemRadius,
                fadedColor(accent, cardFade));
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
        if (!skillCard &&
            (effect == RunUpgradeEffect::MoveSpeed ||
             effect == RunUpgradeEffect::MaximumHealth ||
             effect == RunUpgradeEffect::RecoverableArmor)) {
            displayTitle = mark;
        }
        const std::string displayTitleText{displayTitle};
        const float titleSize = fitRasterTextSize(
            displayTitleText, 64.0F * scale, 32.0F * scale,
            bounds.width - 54.0F * scale);
        const Vector2 titleMeasured = MeasureTextEx(
            uiFont(), displayTitleText.c_str(), titleSize,
            titleSize * 0.02F);
        const float titlePillWidth = std::min(
            bounds.width - 40.0F * scale,
            titleMeasured.x + 72.0F * scale);
        const Rectangle titlePill{
            centerX - titlePillWidth * 0.5F,
            bounds.y + 261.0F * scale,
            titlePillWidth,
            50.0F * scale,
        };
        DrawRectangleRounded(
            titlePill, 1.0F, 8,
            fadedColor({0x34, 0x22, 0x15, 255}, cardFade));
        drawRasterOutlinedCenteredText(
            displayTitleText,
            {centerX, bounds.y + 280.0F * scale},
            titleSize, 7.0F * scale,
            WHITE, CardBlack, cardFade);

        const Rectangle valuePanel{
            bounds.x + 20.0F * scale,
            bounds.y + 327.0F * scale,
            bounds.width - 40.0F * scale,
            105.0F * scale};
        DrawRectangleRounded(
            valuePanel,
            roundedRectangleRoundness(
                valuePanel, 25.0F * scale),
            8,
            fadedColor(CardInset, cardFade));
        const UpgradeValueCopy valueCopy = skillCard
            ? UpgradeValueCopy{
                  std::string{skillCopy.value},
                  std::string{skillCopy.label}}
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
        const float labelPreferredSize =
            36.0F / UiTextRasterScale * scale;
        const float labelSize = fitUiTextSize(
            valueCopy.label, labelPreferredSize,
            20.0F / UiTextRasterScale * scale,
            valuePanel.width - 24.0F * scale);
        drawOutlinedCenteredText(
            valueCopy.label, centerX,
            valuePanel.y + 63.0F * scale,
            labelSize, CardValueCream, cardFade, scale,
            CardInk, 4.0F, false);
        const int currentLevel = skillCard
            ? skillTree.level(skillIndex)
            : snapshot.runUpgradeStacks[runUpgradeIndex(effect)];
        const int maximumLevel = skillCard
            ? skill->maximumLevel
            : runUpgradeMaximumLevel(effect);
        const Rectangle levelBadge{
            centerX - 100.0F * scale,
            bounds.y + 448.0F * scale,
            200.0F * scale,
            54.0F * scale};
        Rectangle upperShadow = levelBadge;
        upperShadow.y -= 2.0F * scale;
        Rectangle lowerShadow = levelBadge;
        lowerShadow.y += 2.0F * scale;
        Rectangle upperOutline = expandedRectangle(
            upperShadow, 3.0F * scale);
        Rectangle lowerOutline = expandedRectangle(
            lowerShadow, 3.0F * scale);
        DrawRectangleRounded(
            upperOutline, 1.0F, 8,
            fadedColor(CardBlack, cardFade));
        DrawRectangleRounded(
            lowerOutline, 1.0F, 8,
            fadedColor(CardBlack, cardFade));
        DrawRectangleRounded(
            lowerShadow, 1.0F, 8,
            fadedColor(LevelShadow, cardFade));
        DrawRectangleRounded(
            upperShadow, 1.0F, 8,
            fadedColor(LevelHighlight, cardFade));
        DrawRectangleRounded(
            levelBadge, 1.0F, 8,
            fadedColor(CardBrown, cardFade));
        const float levelFontSize =
            32.0F / UiTextRasterScale * scale;
        const std::string currentLevelText =
            "LVL " + std::to_string(currentLevel);
        const std::string nextLevelText = std::to_string(
            std::min(currentLevel + 1, maximumLevel));
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

std::size_t cardCollectionPageCount(const SkillTree& skillTree) {
    const std::size_t count = static_cast<std::size_t>(std::ranges::count_if(
        skillTree.nodes(), [](const SkillNodeDefinition& node) {
            return node.cost > 0 &&
                !progressionCardIsHidden(node.id);
        }));
    return std::max<std::size_t>(1U, (count + 11U) / 12U);
}

void drawCardCollectionOverlay(
    GameUi& ui,
    const SimulationSnapshot& snapshot,
    const SkillTree& skillTree,
    std::size_t page) {
    constexpr std::size_t CardsPerPage = 12U;
    constexpr std::size_t Columns = 4U;
    constexpr float Gap = 12.0F;
    std::vector<std::size_t> cards;
    cards.reserve(skillTree.nodes().size());
    for (std::size_t index = 0; index < skillTree.nodes().size(); ++index) {
        if (skillTree.nodes()[index].cost > 0 &&
            !progressionCardIsHidden(
                skillTree.nodes()[index].id)) {
            cards.push_back(index);
        }
    }
    const std::size_t pages = std::max<std::size_t>(
        1U, (cards.size() + CardsPerPage - 1U) / CardsPerPage);
    page = std::min(page, pages - 1U);
    const float screenWidth = static_cast<float>(GetScreenWidth());
    const float screenHeight = static_cast<float>(GetScreenHeight());
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                  {4, 7, 11, 235});
    drawCenteredUiText("CARD COLLECTION", 34.0F, 32.0F,
                       {224, 201, 255, 255});
    drawCenteredUiText(
        "K / ESC CLOSE   •   LEFT / RIGHT PAGE",
        75.0F, 12.0F, {205, 211, 207, 235});

    const float usableWidth = std::min(1180.0F, screenWidth - 48.0F);
    const float cardWidth =
        (usableWidth - Gap * static_cast<float>(Columns - 1U)) /
        static_cast<float>(Columns);
    const float cardHeight = std::clamp(
        (screenHeight - 154.0F - Gap * 2.0F) / 3.0F,
        118.0F, 172.0F);
    const float startX = (screenWidth - usableWidth) * 0.5F;
    const float startY = 105.0F;
    const ProgressionCardContext context{
        .playerLevel = snapshot.playerLevel,
        .coreLevel = snapshot.coreLevel,
        .wavesSurvived = snapshot.runStatistics.wavesSurvived,
        .playerClass = snapshot.playerClass,
    };
    const std::size_t first = page * CardsPerPage;
    const std::size_t last = std::min(first + CardsPerPage, cards.size());
    for (std::size_t visible = first; visible < last; ++visible) {
        const std::size_t slot = visible - first;
        const std::size_t index = cards[visible];
        const SkillNodeDefinition& node = skillTree.nodes()[index];
        const bool unlocked = skillTree.isUnlocked(node.id);
        const int level = skillTree.level(index);
        const bool maximumLevel = skillTree.isMaxLevel(index);
        const bool available = progressionCardEligible(
            skillTree, index, context);
        const Color accent = upgradePalette(
            skillColorClass(node.branch)).accent;
        const float x = startX +
            static_cast<float>(slot % Columns) * (cardWidth + Gap);
        const float y = startY +
            static_cast<float>(slot / Columns) * (cardHeight + Gap);
        const Rectangle bounds{x, y, cardWidth, cardHeight};
        ui.drawCardPanel(
            bounds,
            unlocked || available
                ? accent : Color{83, 88, 91, 210},
            unlocked ? 252 : available ? 246 : 222);
        const std::string state = maximumLevel
            ? "MAX LEVEL"
            : unlocked
                ? "LVL " + std::to_string(level) + " / " +
                    std::to_string(node.maximumLevel)
                : available ? "AVAILABLE" : "LOCKED";
        drawUiText(state, {x + 12.0F, y + 10.0F}, 10.0F,
                   unlocked || available
                       ? accent : Color{137, 140, 140, 220});
        const float titleSize = fitUiTextSize(
            node.title, 16.0F, 11.0F, cardWidth - 24.0F);
        drawUiText(node.title, {x + 12.0F, y + 31.0F},
                   titleSize, {35, 54, 69, 255});
        const auto lines = wrappedLines(
            node.description, 10.5F, cardWidth - 24.0F);
        float lineY = y + 57.0F;
        for (std::size_t line = 0;
             line < std::min<std::size_t>(3U, lines.size()); ++line) {
            drawUiText(lines[line], {x + 12.0F, lineY}, 10.5F,
                       {78, 101, 117, 235});
            lineY += 15.0F;
        }
        std::string requirement;
        if (!unlocked) {
            if (skillTree.isExcluded(index)) {
                requirement = "MUTUALLY EXCLUSIVE";
            } else if (const int minimumLevel =
                           progressionCardMinimumLevel(node);
                       snapshot.playerLevel < minimumLevel) {
                requirement = "LVL " + std::to_string(minimumLevel);
            } else if (snapshot.coreLevel < std::max(
                           node.minimumCoreLevel,
                           progressionCardRequiredCoreLevel(node.id))) {
                requirement = "CORE " +
                    std::to_string(std::max(
                        node.minimumCoreLevel,
                        progressionCardRequiredCoreLevel(node.id)));
            } else if (snapshot.runStatistics.wavesSurvived <
                       node.minimumWavesSurvived) {
                requirement = "WAVE " +
                    std::to_string(node.minimumWavesSurvived);
            }
        }
        if (!requirement.empty()) {
            drawUiText(
                requirement, {x + 12.0F, y + cardHeight - 23.0F},
                10.0F, {242, 176, 99, 245});
        }
    }
    drawCenteredUiText(
        "PAGE " + std::to_string(page + 1U) + " / " +
            std::to_string(pages),
        screenHeight - 34.0F, 13.0F,
        {220, 208, 236, 245});
}

} // namespace ian::app_detail
