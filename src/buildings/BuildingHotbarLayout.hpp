#pragma once

#include "buildings/BuildingSystem.hpp"
#include "game/GameBalance.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace ian {

enum class BuildingHotbarCategory : std::uint8_t {
    Base,
    Defense,
    Economy,
    Modular,
};

inline constexpr std::size_t BuildingHotbarCategoryCount = 4U;
inline constexpr std::size_t MaximumBuildingHotbarSlots = 6U;

inline constexpr std::array<BuildingType, 3> BaseBuildingOrder{
        BuildingType::Core,
        BuildingType::Wall,
        BuildingType::Gate,
    };
inline constexpr std::array<BuildingType, 6> DefenseBuildingOrder{
        BuildingType::GunTurret,
        BuildingType::Turret,
        BuildingType::Cannon,
        BuildingType::Catapult,
        BuildingType::SlowTrap,
        BuildingType::SpikeTrap,
    };
inline constexpr std::array<BuildingType, 6> EconomyBuildingOrder{
    BuildingType::LumberMill,
    BuildingType::Quarry,
    BuildingType::CrystalMine,
    BuildingType::WoodStorage,
    BuildingType::StoneStorage,
    BuildingType::CrystalStorage,
    };

[[nodiscard]] constexpr BuildingHotbarCategory buildingHotbarCategory(
    BuildingType type) {
    switch (type) {
    case BuildingType::Core:
    case BuildingType::Wall:
    case BuildingType::Gate:
        return BuildingHotbarCategory::Base;
    case BuildingType::LumberMill:
    case BuildingType::Quarry:
    case BuildingType::CrystalMine:
    case BuildingType::WoodStorage:
    case BuildingType::StoneStorage:
    case BuildingType::CrystalStorage:
        return BuildingHotbarCategory::Economy;
    default:
        return BuildingHotbarCategory::Defense;
    }
}

[[nodiscard]] constexpr std::string_view buildingHotbarCategoryName(
    BuildingHotbarCategory category) {
    switch (category) {
    case BuildingHotbarCategory::Base: return "BASE";
    case BuildingHotbarCategory::Defense: return "DEFENSE";
    case BuildingHotbarCategory::Economy: return "ECONOMY";
    case BuildingHotbarCategory::Modular: return "MODULAR";
    }
    return "BASE";
}

struct BuildingHotbarLayout {
    std::array<BuildingType, MaximumBuildingHotbarSlots> types{};
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
    const std::array<bool, GameBalance::BuildingTypeCount>& unlocked,
    BuildingHotbarCategory category,
    bool corePlaced,
    std::uint8_t coreLevel,
    bool bypassCoreRestrictions = false) {
    BuildingHotbarLayout layout;
    if (!corePlaced) {
        const auto coreIndex = static_cast<std::size_t>(BuildingType::Core);
        if (unlocked[coreIndex]) {
            layout.types[layout.count++] = BuildingType::Core;
        }
        return layout;
    }

    const auto appendUnlocked = [&](std::span<const BuildingType> order) {
        for (const BuildingType type : order) {
            if (type == BuildingType::Core) continue;
            const std::size_t index = static_cast<std::size_t>(type);
            if (!unlocked[index]) continue;
            if (!bypassCoreRestrictions &&
                (type == BuildingType::WoodStorage ||
                 type == BuildingType::StoneStorage ||
                 type == BuildingType::CrystalStorage)) {
                continue;
            }
            if (!bypassCoreRestrictions && coreLevel < 2U &&
                (type == BuildingType::SlowTrap ||
                 type == BuildingType::SpikeTrap ||
                 type == BuildingType::LumberMill ||
                 type == BuildingType::Quarry)) {
                continue;
            }
            layout.types[layout.count++] = type;
        }
    };
    switch (category) {
    case BuildingHotbarCategory::Base:
        appendUnlocked(BaseBuildingOrder);
        break;
    case BuildingHotbarCategory::Defense:
        appendUnlocked(DefenseBuildingOrder);
        break;
    case BuildingHotbarCategory::Economy:
        appendUnlocked(EconomyBuildingOrder);
        break;
    case BuildingHotbarCategory::Modular:
        break;
    }
    return layout;
}

} // namespace ian
