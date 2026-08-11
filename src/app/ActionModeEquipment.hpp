#pragma once

#include "app/App.hpp"

#include <algorithm>
#include <optional>
#include <span>

namespace ian::app_detail {

[[nodiscard]] inline std::span<const PlayerWeapon> equipmentOrder(
    ActionMode mode) {
    if (mode == ActionMode::Tools) {
        return PlayerToolHotbarOrder;
    }
    if (mode == ActionMode::Weapons) {
        return PlayerCombatHotbarOrder;
    }
    return {};
}

[[nodiscard]] inline std::optional<PlayerWeapon> availableEquipment(
    PlayerWeapon remembered, ActionMode mode,
    const SimulationSnapshot& snapshot) {
    const auto order = equipmentOrder(mode);
    const auto unlocked = [&snapshot](PlayerWeapon weapon) {
        return snapshot.unlockedWeapons[
            static_cast<std::size_t>(weapon)];
    };
    if (std::ranges::find(order, remembered) != order.end() &&
        unlocked(remembered)) {
        return remembered;
    }
    const auto first = std::ranges::find_if(order, unlocked);
    return first != order.end()
        ? std::optional<PlayerWeapon>{*first}
        : std::nullopt;
}

} // namespace ian::app_detail
