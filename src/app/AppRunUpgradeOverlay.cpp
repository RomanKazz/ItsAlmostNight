#include "app/AppRunUpgradeOverlay.hpp"

#include "ui/UiText.hpp"

#include <algorithm>
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

void drawRunUpgradeOverlay(const SimulationSnapshot& snapshot) {
    if (!snapshot.runUpgradeChoicePending) return;
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                  {4, 8, 12, 206});
    drawCenteredUiText(
        "NIGHT SURVIVED", static_cast<float>(GetScreenHeight()) * 0.5F - 232.0F,
        38.0F, {255, 224, 145, 255});
    const std::string prompt = snapshot.runUpgradeSelectionsRemaining > 1
        ? "CHOOSE " + std::to_string(snapshot.runUpgradeSelectionsRemaining) +
            " UPGRADES FOR THIS RUN"
        : "CHOOSE ONE UPGRADE FOR THIS RUN";
    drawCenteredUiText(
        prompt,
        static_cast<float>(GetScreenHeight()) * 0.5F - 184.0F,
        16.0F, {213, 220, 216, 240});

    const Vector2 mouse = GetMousePosition();
    for (std::size_t index = 0; index < snapshot.runUpgradeChoiceCount;
         ++index) {
        const RunUpgradeEffect effect = snapshot.runUpgradeChoices[index];
        const auto& definition = runUpgradeDefinition(effect);
        Rectangle bounds = runUpgradeCardBounds(
            index, snapshot.runUpgradeChoiceCount);
        const bool hovered = CheckCollisionPointRec(mouse, bounds);
        const bool locked = snapshot.lockedRunUpgrade == effect;
        if (hovered) {
            bounds.y -= 8.0F;
        }
        const Color accent = upgradeColor(effect);
        DrawRectangleRounded(bounds, 0.08F, 8,
                             hovered ? Color{38, 43, 45, 252}
                                     : Color{25, 29, 31, 248});
        DrawRectangleRoundedLinesEx(bounds, 0.08F, 8,
                                    hovered || locked ? 4.0F : 2.0F,
                                    accent);
        if (locked) {
            DrawRectangleRounded(
                {bounds.x + bounds.width - 69.0F, bounds.y + 16.0F,
                 52.0F, 24.0F},
                0.22F, 6, accent);
            drawUiText("LOCK", {bounds.x + bounds.width - 63.0F,
                                 bounds.y + 20.0F},
                       11.0F, RAYWHITE);
        }
        DrawRectangleRounded(
            {bounds.x + 15.0F, bounds.y + 15.0F, 34.0F, 30.0F},
            0.18F, 6, accent);
        const std::string key = std::to_string(index + 1U);
        const Vector2 keySize = measureUiText(key, 16.0F);
        drawUiText(key,
                   {bounds.x + 32.0F - keySize.x * 0.5F,
                    bounds.y + 21.0F},
                   16.0F, RAYWHITE);

        const std::string_view mark = upgradeMark(effect);
        const Vector2 markSize = measureUiText(mark, 31.0F);
        drawUiText(mark,
                   {bounds.x + bounds.width * 0.5F - markSize.x * 0.5F,
                    bounds.y + 69.0F},
                   31.0F, accent);
        DrawRectangleRec(
            {bounds.x + 24.0F, bounds.y + 119.0F,
             bounds.width - 48.0F, 2.0F},
            Color{accent.r, accent.g, accent.b, 155});

        const float titleSize = fitUiTextSize(
            definition.name, 18.0F, 13.0F, bounds.width - 28.0F);
        const Vector2 titleSizeMeasured = measureUiText(
            definition.name, titleSize);
        drawUiText(definition.name,
                   {bounds.x + bounds.width * 0.5F -
                        titleSizeMeasured.x * 0.5F,
                    bounds.y + 139.0F},
                   titleSize, RAYWHITE);
        const auto lines = wrappedLines(
            definition.description, 14.0F, bounds.width - 34.0F);
        float lineY = bounds.y + 181.0F;
        for (const std::string& line : lines) {
            const Vector2 size = measureUiText(line, 14.0F);
            drawUiText(line,
                       {bounds.x + bounds.width * 0.5F - size.x * 0.5F,
                        lineY},
                       14.0F, {218, 220, 211, 245});
            lineY += 20.0F;
        }
        const int currentLevel = snapshot.runUpgradeStacks[
            runUpgradeIndex(effect)];
        const std::string level = currentLevel == 0
            ? "NEW" : "LEVEL " + std::to_string(currentLevel) +
                "  >  " + std::to_string(currentLevel + 1);
        const Vector2 levelSize = measureUiText(level, 12.0F);
        drawUiText(level,
                   {bounds.x + bounds.width * 0.5F - levelSize.x * 0.5F,
                    bounds.y + bounds.height - 33.0F},
                   12.0F, accent);
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
        drawCenteredUiText(
            controls,
            static_cast<float>(GetScreenHeight()) * 0.5F + 218.0F,
            15.0F, {229, 219, 185, 245});
    }
}

} // namespace ian::app_detail
