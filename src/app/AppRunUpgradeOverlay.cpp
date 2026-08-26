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

constexpr float CardGap = 16.0F;
constexpr float CardHeight = 300.0F;

Color upgradeColor(RunUpgradeEffect effect) {
    switch (effect) {
    case RunUpgradeEffect::Damage:
    case RunUpgradeEffect::AttackSpeed:
        return {224, 91, 75, 255};
    case RunUpgradeEffect::MoveSpeed:
    case RunUpgradeEffect::MaximumHealth:
    case RunUpgradeEffect::RecoverableArmor:
        return {73, 176, 198, 255};
    case RunUpgradeEffect::BuildingHealth:
    case RunUpgradeEffect::BuildRadius:
    case RunUpgradeEffect::DefenseDamage:
    case RunUpgradeEffect::DefenseFireRate:
    case RunUpgradeEffect::TwinBatteries:
    case RunUpgradeEffect::ClusterPayload:
    case RunUpgradeEffect::ReactiveTraps:
        return {224, 157, 56, 255};
    case RunUpgradeEffect::ProductionSpeed:
    case RunUpgradeEffect::NightlyBomb:
        return {72, 184, 121, 255};
    case RunUpgradeEffect::WiderChoice:
    case RunUpgradeEffect::DoubleDown:
    case RunUpgradeEffect::LockChoice:
    case RunUpgradeEffect::RerollToken:
    case RunUpgradeEffect::RiskyInvestment:
        return {163, 105, 216, 255};
    case RunUpgradeEffect::BloodHarvest:
    case RunUpgradeEffect::Overkill:
        return {207, 67, 79, 255};
    case RunUpgradeEffect::Ricochet:
        return {76, 185, 215, 255};
    case RunUpgradeEffect::Salvager:
        return {219, 164, 73, 255};
    }
    return RAYWHITE;
}

std::string_view upgradeMark(RunUpgradeEffect effect) {
    switch (effect) {
    case RunUpgradeEffect::Damage: return "DMG";
    case RunUpgradeEffect::AttackSpeed: return "SPD";
    case RunUpgradeEffect::MoveSpeed: return "RUN";
    case RunUpgradeEffect::MaximumHealth: return "HP";
    case RunUpgradeEffect::RecoverableArmor: return "ARM";
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

Color skillColor(SkillBranch branch) {
    switch (branch) {
    case SkillBranch::Gathering: return {72, 184, 121, 255};
    case SkillBranch::Weapons: return {224, 91, 75, 255};
    case SkillBranch::Construction: return {224, 157, 56, 255};
    case SkillBranch::Movement: return {73, 176, 198, 255};
    case SkillBranch::Economy: return {163, 105, 216, 255};
    case SkillBranch::Root: return {229, 219, 185, 255};
    }
    return RAYWHITE;
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

Color mixedColor(Color from, Color to, float amount) {
    const float t = std::clamp(amount, 0.0F, 1.0F);
    const auto mixChannel = [t](unsigned char first, unsigned char second) {
        return static_cast<unsigned char>(std::lround(
            static_cast<float>(first) +
            (static_cast<float>(second) - static_cast<float>(first)) * t));
    };
    return {
        mixChannel(from.r, to.r),
        mixChannel(from.g, to.g),
        mixChannel(from.b, to.b),
        mixChannel(from.a, to.a),
    };
}

} // namespace

Rectangle runUpgradeCardBounds(std::size_t index, std::size_t count) {
    const float screenWidth = static_cast<float>(GetScreenWidth());
    const float usableWidth = std::max(600.0F, screenWidth - 64.0F);
    const float cardWidth = std::clamp(
        (usableWidth - CardGap * static_cast<float>(count - 1U)) /
            static_cast<float>(count),
        188.0F, 260.0F);
    const float totalWidth = cardWidth * static_cast<float>(count) +
        CardGap * static_cast<float>(count - 1U);
    return {
        (screenWidth - totalWidth) * 0.5F +
            static_cast<float>(index) * (cardWidth + CardGap),
        static_cast<float>(GetScreenHeight()) * 0.5F - 108.0F,
        cardWidth,
        CardHeight,
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
    const SimulationSnapshot& snapshot,
    const SkillTree& skillTree,
    double entranceSeconds,
    std::span<const float> hoverAmounts) {
    if (!snapshot.runUpgradeChoicePending) return;
    const float screenFade = smoothUnit(static_cast<float>(
        entranceSeconds / 0.32));
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                  {4, 8, 12, static_cast<unsigned char>(
                      std::lround(206.0F * screenFade))});
    const float headingFade = smoothUnit(static_cast<float>(
        (entranceSeconds - 0.08) / 0.30));
    drawCenteredUiText(
        "LEVEL UP",
        static_cast<float>(GetScreenHeight()) * 0.5F - 232.0F -
            (1.0F - headingFade) * 18.0F,
        38.0F,
        fadedColor({208, 177, 255, 255}, headingFade));
    const std::string prompt = snapshot.runUpgradeSelectionsRemaining > 1
        ? std::to_string(snapshot.runUpgradeSelectionsRemaining) +
            " LEVELS READY  -  CHOOSE AN UPGRADE"
        : "CHOOSE ONE UPGRADE";
    drawCenteredUiText(
        prompt,
        static_cast<float>(GetScreenHeight()) * 0.5F - 184.0F,
        16.0F, fadedColor({213, 220, 216, 240}, headingFade));

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
        const std::string_view title = skillCard
            ? std::string_view{skill->title}
            : runUpgradeDefinition(effect).name;
        const std::string_view description = skillCard
            ? std::string_view{skill->description}
            : runUpgradeDefinition(effect).description;
        const Rectangle baseBounds = runUpgradeCardBounds(
            index, snapshot.runUpgradeChoiceCount);
        const bool hovered = CheckCollisionPointRec(mouse, baseBounds);
        const float hover = index < hoverAmounts.size()
            ? std::clamp(hoverAmounts[index], 0.0F, 1.0F)
            : hovered ? 1.0F : 0.0F;
        const double cardDelay = 0.12 +
            static_cast<double>(index) * 0.075;
        const float entrance = std::clamp(static_cast<float>(
            (entranceSeconds - cardDelay) / 0.42), 0.0F, 1.0F);
        if (entrance <= 0.0F) continue;
        const float cardFade = smoothUnit(entrance);
        const float scale = std::max(
            0.001F,
            cardEntranceScale(entrance) * (1.0F + hover * 0.045F));
        const float lift = (1.0F - cardFade) * -42.0F + hover * 10.0F;
        Rectangle bounds = scaleRectangle(baseBounds, scale, lift);
        const bool locked = !skillCard &&
            snapshot.lockedRunUpgrade == effect;
        const Color accent = skillCard
            ? skillColor(skill->branch)
            : upgradeColor(effect);
        const Rectangle shadow{
            bounds.x + 7.0F * scale,
            bounds.y + 11.0F * scale,
            bounds.width, bounds.height};
        DrawRectangleRounded(
            shadow, 0.08F, 8,
            fadedColor({0, 0, 0, 175}, cardFade));
        if (hover > 0.01F) {
            const Rectangle glow{
                bounds.x - 7.0F * hover,
                bounds.y - 7.0F * hover,
                bounds.width + 14.0F * hover,
                bounds.height + 14.0F * hover};
            DrawRectangleRounded(
                glow, 0.08F, 8,
                fadedColor(
                    {accent.r, accent.g, accent.b, 42},
                    cardFade * hover));
        }
        DrawRectangleRounded(
            bounds, 0.08F, 8,
            fadedColor(
                mixedColor(
                    {22, 26, 30, 250},
                    {38, 43, 45, 252}, hover),
                cardFade));
        DrawRectangleRounded(
            {bounds.x + 5.0F * scale,
             bounds.y + 5.0F * scale,
             bounds.width - 10.0F * scale,
             58.0F * scale},
            0.08F, 8,
            fadedColor(
                {accent.r, accent.g, accent.b,
                 static_cast<unsigned char>(38 + 35 * hover)},
                cardFade));
        DrawRectangleRoundedLinesEx(bounds, 0.08F, 8,
                                    std::max(1.0F,
                                        (2.0F + 2.0F * hover +
                                         (locked ? 1.0F : 0.0F)) * scale),
                                    fadedColor(accent, cardFade));
        const Rectangle innerBorder{
            bounds.x + 6.0F * scale,
            bounds.y + 6.0F * scale,
            bounds.width - 12.0F * scale,
            bounds.height - 12.0F * scale};
        DrawRectangleRoundedLinesEx(
            innerBorder, 0.075F, 8,
            std::max(0.7F, scale),
            fadedColor(
                {accent.r, accent.g, accent.b, 92}, cardFade));
        if (locked) {
            DrawRectangleRounded(
                {bounds.x + bounds.width - 69.0F * scale,
                 bounds.y + 16.0F * scale,
                 52.0F * scale, 24.0F * scale},
                0.22F, 6, fadedColor(accent, cardFade));
            drawUiText(
                "LOCK",
                {bounds.x + bounds.width - 63.0F * scale,
                 bounds.y + 20.0F * scale},
                11.0F * scale,
                fadedColor(RAYWHITE, cardFade));
        }
        DrawRectangleRounded(
            {bounds.x + 15.0F * scale,
             bounds.y + 15.0F * scale,
             34.0F * scale, 30.0F * scale},
            0.18F, 6, fadedColor(accent, cardFade));
        const std::string key = std::to_string(index + 1U);
        const Vector2 keySize = measureUiText(key, 16.0F * scale);
        drawUiText(key,
                   {bounds.x + 32.0F * scale - keySize.x * 0.5F,
                    bounds.y + 21.0F * scale},
                   16.0F * scale,
                   fadedColor(RAYWHITE, cardFade));

        const std::string_view mark = skillCard
            ? skillMark(skill->branch)
            : upgradeMark(effect);
        const Vector2 emblemCenter{
            bounds.x + bounds.width * 0.5F,
            bounds.y + 88.0F * scale};
        DrawCircleV(
            emblemCenter, 35.0F * scale,
            fadedColor({7, 11, 15, 205}, cardFade));
        DrawCircleLinesV(
            emblemCenter, 34.0F * scale,
            fadedColor(accent, cardFade));
        DrawCircleLinesV(
            emblemCenter, 29.0F * scale,
            fadedColor(
                {accent.r, accent.g, accent.b, 110}, cardFade));
        const Vector2 markSize = measureUiText(mark, 25.0F * scale);
        drawUiText(mark,
                   {emblemCenter.x - markSize.x * 0.5F,
                    emblemCenter.y - markSize.y * 0.5F},
                   25.0F * scale,
                   fadedColor(accent, cardFade));
        DrawRectangleRec(
            {bounds.x + 24.0F * scale,
             bounds.y + 126.0F * scale,
             bounds.width - 48.0F * scale,
             std::max(1.0F, 2.0F * scale)},
            fadedColor(
                {accent.r, accent.g, accent.b, 155}, cardFade));

        const float titleSize = fitUiTextSize(
            title, 18.0F, 13.0F, baseBounds.width - 28.0F);
        const Vector2 titleSizeMeasured = measureUiText(
            title, titleSize * scale);
        drawUiText(title,
                   {bounds.x + bounds.width * 0.5F -
                        titleSizeMeasured.x * 0.5F,
                    bounds.y + 143.0F * scale},
                   titleSize * scale,
                   fadedColor(RAYWHITE, cardFade));
        const auto lines = wrappedLines(
            description, 14.0F, baseBounds.width - 34.0F);
        float lineY = bounds.y + 184.0F * scale;
        for (const std::string& line : lines) {
            const Vector2 size = measureUiText(line, 14.0F * scale);
            drawUiText(line,
                       {bounds.x + bounds.width * 0.5F - size.x * 0.5F,
                        lineY},
                       14.0F * scale,
                       fadedColor({218, 220, 211, 245}, cardFade));
            lineY += 20.0F * scale;
        }
        const int currentLevel = skillCard ? 0 : snapshot.runUpgradeStacks[
            runUpgradeIndex(effect)];
        const std::string level = skillCard
            ? (skill->cost >= 3 ? "LEGENDARY" :
               skill->cost == 2 ? "RARE" : "NEW")
            : currentLevel == 0
                ? "NEW" : "LEVEL " + std::to_string(currentLevel) +
                    "  >  " + std::to_string(currentLevel + 1);
        DrawRectangleRounded(
            {bounds.x + 16.0F * scale,
             bounds.y + bounds.height - 48.0F * scale,
             bounds.width - 32.0F * scale,
             31.0F * scale},
            0.24F, 6,
            fadedColor(
                {accent.r, accent.g, accent.b, 32}, cardFade));
        const Vector2 levelSize = measureUiText(
            level, 12.0F * scale);
        drawUiText(level,
                   {bounds.x + bounds.width * 0.5F - levelSize.x * 0.5F,
                    bounds.y + bounds.height - 39.0F * scale},
                   12.0F * scale,
                   fadedColor(accent, cardFade));
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
            (entranceSeconds - 0.48) / 0.28));
        drawCenteredUiText(
            controls,
            static_cast<float>(GetScreenHeight()) * 0.5F + 218.0F,
            15.0F,
            fadedColor({229, 219, 185, 245}, controlsFade));
    }
}

std::size_t cardCollectionPageCount(const SkillTree& skillTree) {
    const std::size_t count = static_cast<std::size_t>(std::ranges::count_if(
        skillTree.nodes(), [](const SkillNodeDefinition& node) {
            return node.cost > 0 &&
                !progressionCardIsLegacyBuildingUnlock(node.id);
        }));
    return std::max<std::size_t>(1U, (count + 11U) / 12U);
}

void drawCardCollectionOverlay(
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
            !progressionCardIsLegacyBuildingUnlock(
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
        const bool available = progressionCardEligible(
            skillTree, index, context);
        const Color accent = skillColor(node.branch);
        const float x = startX +
            static_cast<float>(slot % Columns) * (cardWidth + Gap);
        const float y = startY +
            static_cast<float>(slot / Columns) * (cardHeight + Gap);
        const Rectangle bounds{x, y, cardWidth, cardHeight};
        DrawRectangleRounded(
            bounds, 0.08F, 7,
            unlocked ? Color{27, 43, 37, 248}
                : available ? Color{30, 32, 36, 248}
                : Color{17, 19, 22, 232});
        DrawRectangleRoundedLinesEx(
            bounds, 0.08F, 7, unlocked ? 2.5F : 1.5F,
            unlocked || available
                ? accent
                : Color{83, 88, 91, 210});
        const std::string_view state = unlocked
            ? "OWNED" : available ? "AVAILABLE" : "LOCKED";
        drawUiText(state, {x + 12.0F, y + 10.0F}, 10.0F,
                   unlocked || available
                       ? accent : Color{137, 140, 140, 220});
        const float titleSize = fitUiTextSize(
            node.title, 16.0F, 11.0F, cardWidth - 24.0F);
        drawUiText(node.title, {x + 12.0F, y + 31.0F},
                   titleSize, RAYWHITE);
        const auto lines = wrappedLines(
            node.description, 10.5F, cardWidth - 24.0F);
        float lineY = y + 57.0F;
        for (std::size_t line = 0;
             line < std::min<std::size_t>(3U, lines.size()); ++line) {
            drawUiText(lines[line], {x + 12.0F, lineY}, 10.5F,
                       {204, 208, 202, 235});
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
            } else {
                const auto required = progressionCardRequiredCards(node.id);
                if (!required.empty()) {
                    requirement = "NEEDS ";
                    for (std::size_t requiredIndex = 0;
                         requiredIndex < required.size(); ++requiredIndex) {
                        if (requiredIndex > 0) requirement += " + ";
                        std::string name{required[requiredIndex]};
                        std::ranges::replace(name, '_', ' ');
                        std::ranges::transform(
                            name, name.begin(), [](unsigned char value) {
                                return static_cast<char>(std::toupper(value));
                            });
                        requirement += name;
                    }
                }
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
