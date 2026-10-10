#pragma once

#include "economy/ResourceCost.hpp"
#include "ui/GameUi.hpp"
#include "ui/UiText.hpp"

#include <array>
#include <string>
#include <utility>

namespace ian::hud_detail {

[[nodiscard]] inline std::string costText(const ResourceCost& cost) {
    std::string result;
    const auto append = [&result](const char* prefix, int amount) {
        if (amount <= 0) {
            return;
        }
        if (!result.empty()) {
            result += "  ";
        }
        result += prefix;
        result += std::to_string(amount);
    };
    append("W:", cost.wood);
    append("S:", cost.stone);
    append("C:", cost.crystals);
    return result.empty() ? "FREE" : result;
}

[[nodiscard]] inline float resourceCostRowWidth(
    const ResourceCost& cost, float fontSize = 14.0F,
    float iconSize = 22.0F) {
    constexpr float IconTextGap = 4.0F;
    constexpr float GroupGap = 13.0F;
    const std::array<int, 3> amounts{
        cost.wood, cost.stone, cost.crystals};
    float width = 0.0F;
    bool hasEntry = false;
    for (const int amount : amounts) {
        if (amount <= 0) continue;
        if (hasEntry) width += GroupGap;
        width += iconSize + IconTextGap +
            measureUiText(std::to_string(amount), fontSize).x;
        hasEntry = true;
    }
    return hasEntry
        ? width
        : measureUiText("FREE", fontSize).x;
}

inline void drawResourceCostRow(
    GameUi& ui, const ResourceCost& cost, Vector2 position,
    float fontSize = 14.0F, float iconSize = 22.0F,
    Color textColor = Color{226, 218, 197, 245},
    const ResourceCost* availableResources = nullptr) {
    constexpr float IconTextGap = 4.0F;
    constexpr float GroupGap = 13.0F;
    const std::array<std::pair<UiResourceIcon, int>, 3> entries{{
        {UiResourceIcon::Wood, cost.wood},
        {UiResourceIcon::Stone, cost.stone},
        {UiResourceIcon::Crystal, cost.crystals},
    }};
    SdfUiTextStyle style{};
    style.fillColor = textColor;
    style.outlineColor = {49, 49, 49, textColor.a};
    style.shadowColor = {49, 35, 28,
                         static_cast<unsigned char>(textColor.a * 0.78F)};
    style.shadowOffsetPixels = {0.0F, 2.0F};
    style.outlinePixels = 1.5F;

    const std::array<int, 3> stock{
        availableResources ? availableResources->wood : 0,
        availableResources ? availableResources->stone : 0,
        availableResources ? availableResources->crystals : 0};
    std::size_t resourceIndex = 0;
    float x = position.x;
    bool hasEntry = false;
    for (const auto& [icon, amount] : entries) {
        const int available = stock[resourceIndex++];
        if (amount <= 0) continue;
        if (availableResources) {
            style.fillColor = available >= amount
                ? Color{245, 238, 220, 255} : Color{242, 103, 83, 255};
        }
        if (hasEntry) x += GroupGap;
        ui.drawResourceIcon(
            {x, position.y - 2.0F, iconSize, iconSize}, icon);
        x += iconSize + IconTextGap;
        const std::string amountText = std::to_string(amount);
        drawStyledSdfUiText(
            amountText, {x, position.y}, fontSize, style);
        x += measureUiText(amountText, fontSize).x;
        hasEntry = true;
    }
    if (!hasEntry) {
        drawStyledSdfUiText("FREE", position, fontSize, style);
    }
}

} // namespace ian::hud_detail
