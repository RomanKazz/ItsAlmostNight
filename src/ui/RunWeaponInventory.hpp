#pragma once

#include "ui/RunHammerUi.hpp"
#include "combat/PlayerWeaponSystem.hpp"
#include "ui/UiItemDescriptions.hpp"
#include "ui/UiMenuStyle.hpp"

namespace ian {

inline const char* weaponLabel(PlayerWeapon weapon) {
    switch (weapon) {
    case PlayerWeapon::LegacyBareHands:
    case PlayerWeapon::LegacyHammer: return "REMOVED";
    case PlayerWeapon::Axe: return "AXE";
    case PlayerWeapon::Pickaxe: return "PICKAXE";
    case PlayerWeapon::Club: return "CLUB";
    case PlayerWeapon::IceWand: return "ICE WAND";
    case PlayerWeapon::FireWand: return "FIRE WAND";
    case PlayerWeapon::Rifle: return "RIFLE";
    case PlayerWeapon::Bomb: return "BOMBS";
    }
    return "WEAPON";
}

inline void drawRunWeaponInventory(const GameUi& ui, const SimulationSnapshot& snapshot,
    Rectangle bounds, std::optional<Vector2> pointer = std::nullopt) {
    const auto count = static_cast<std::size_t>(std::ranges::count_if(PlayerWeaponHotbarOrder,
        [&](PlayerWeapon weapon) { return snapshot.unlockedWeapons[static_cast<std::size_t>(weapon)]; }));
    const auto slots = std::max(count, static_cast<std::size_t>(std::max(1, snapshot.weaponSlotLimit)));
    const float pitch = std::min(bounds.height, bounds.width / static_cast<float>(slots));
    const Vector2 mouse = pointer.value_or(uiMousePosition());
    std::optional<PlayerWeapon> hovered;
    std::size_t position = 0;
    for (const auto weapon : PlayerWeaponHotbarOrder) {
        const auto index = static_cast<std::size_t>(weapon);
        if (!snapshot.unlockedWeapons[index]) continue;
        const Rectangle cell{bounds.x + static_cast<float>(position) * pitch, bounds.y, pitch - 3, pitch - 3};
        DrawRectangleRounded(cell, .18F, 4, {24, 29, 27, 160});
        const auto texture = ui.weaponTexture(weapon);
        if (IsTextureValid(texture)) DrawTexturePro(texture,
            {0, 0, static_cast<float>(texture.width), static_cast<float>(texture.height)},
            {cell.x + 3, cell.y + 2, cell.width - 6, cell.height - 6}, {}, 0, WHITE);
        const float size = pitch * .19F;
        const std::string badge = std::to_string(std::max(1, snapshot.weaponLevels[index]));
        const auto measured = measureUiText(badge, size);
        drawStyledSdfUiText(badge, {cell.x + cell.width - measured.x - 2,
            cell.y + cell.height - measured.y - 1}, size,
            {.fillColor = UiWhite, .outlineColor = {20, 22, 20, 255}, .outlinePixels = 1.5F});
        if ((pointer || !IsCursorHidden()) && CheckCollisionPointRec(mouse, cell)) hovered = weapon;
        ++position;
    }
    for (; position < slots; ++position) {
        const Rectangle cell{bounds.x + static_cast<float>(position) * pitch, bounds.y, pitch - 3, pitch - 3};
        DrawRectangleRounded(cell, .18F, 4, {24, 29, 27, 120});
        const float size = pitch * .40F;
        const auto measured = measureUiText("+", size);
        drawUiText("+", {cell.x + (cell.width - measured.x) * .5F,
            cell.y + (cell.height - measured.y) * .5F}, size, {190, 202, 197, 115});
    }
    if (hovered) drawItemTooltip(weaponLabel(*hovered), weaponDescription(*hovered), mouse,
        {static_cast<float>(uiScreenWidth()), static_cast<float>(uiScreenHeight())}, .85F);
}

} // namespace ian
