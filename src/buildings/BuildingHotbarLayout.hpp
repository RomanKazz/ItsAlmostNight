#pragma once

#include "buildings/BuildingSystem.hpp"
#include "game/GameBalance.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <span>

namespace ian {

// UI and input share this order so a visible key can never select a different
// building. Newly researched defenses simply occupy the next visible slot.
inline constexpr std::array<BuildingType, GameBalance::BuildingTypeCount>
    BuildingHotbarOrder{
        BuildingType::Core,
        BuildingType::Wall,
        BuildingType::GunTurret,
        BuildingType::Turret,
        BuildingType::Cannon,
        BuildingType::Catapult,
        BuildingType::SlowTrap,
        BuildingType::Gate,
        BuildingType::LumberMill,
        BuildingType::Quarry,
        BuildingType::CrystalMine,
        BuildingType::SpikeTrap,
        BuildingType::WoodStorage,
        BuildingType::StoneStorage,
        BuildingType::CrystalStorage,
    };

struct BuildingHotbarLayout {
    std::array<BuildingType, GameBalance::BuildingTypeCount> types{};
    std::size_t count{};

    [[nodiscard]] std::span<const BuildingType> visible() const {
        return {types.data(), count};
    }

    [[nodiscard]] std::optional<std::size_t> indexOf(
        BuildingType type) const {
        for (std::size_t index = 0; index < count; ++index) {
            if (types[index] == type) return index;
        }
        return std::nullopt;
    }
};

[[nodiscard]] inline BuildingHotbarLayout makeBuildingHotbarLayout(
    const std::array<bool, GameBalance::BuildingTypeCount>& unlocked) {
    BuildingHotbarLayout layout;
    for (const BuildingType type : BuildingHotbarOrder) {
        if (!unlocked[static_cast<std::size_t>(type)]) continue;
        layout.types[layout.count++] = type;
    }
    return layout;
}

} // namespace ian
