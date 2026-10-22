#pragma once

#include "buildings/BuildingSystem.hpp"
#include "buildings/CoreProgression.hpp"
#include "buildings/ModularBuildingConstants.hpp"
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
inline constexpr std::size_t MaximumBuildingHotbarSlots = 21U;

inline constexpr std::array<BuildingType, 1> BaseBuildingOrder{
        BuildingType::Core,
    };
inline constexpr std::array<BuildingType, 14> DefenseBuildingOrder{
        BuildingType::GunTurret,
        BuildingType::Turret,
        BuildingType::Cannon,
        BuildingType::Catapult,
        BuildingType::Flamethrower,
        BuildingType::MissileLauncher,
        BuildingType::TeslaTower,
        BuildingType::LaserTower,
        BuildingType::Mortar,
        BuildingType::RepairStation,
        BuildingType::DecoyBeacon,
        BuildingType::WindBlower,
        BuildingType::SpikeTrap,
        BuildingType::SlowTrap,
    };
inline constexpr std::array<BuildingType, 3> EconomyBuildingOrder{
    BuildingType::CrystalMine,
    BuildingType::Quarry,
    BuildingType::LumberMill,
};
inline constexpr std::array<BuildingType, 1> ModularBuildingOrder{
    BuildingType::Wall,
};

[[nodiscard]] constexpr BuildingHotbarCategory buildingHotbarCategory(
    BuildingType type) {
    switch (type) {
    case BuildingType::Core:
        return BuildingHotbarCategory::Base;
    case BuildingType::Gate:
    case BuildingType::Wall:
        return BuildingHotbarCategory::Modular;
    case BuildingType::LumberMill:
    case BuildingType::Quarry:
    case BuildingType::CrystalMine:
        return BuildingHotbarCategory::Economy;
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
                coreLevel < buildingRequiredCoreLevel(type)) {
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
        appendUnlocked(ModularBuildingOrder);
        break;
    }
    return layout;
}

struct ConstructionHotbarItem {
    std::optional<ModularBuildPiece> modularPiece;
    std::optional<BuildingType> buildingType;
    bool gapBefore{};
};

struct ConstructionHotbarLayout {
    std::array<ConstructionHotbarItem,
               MaximumBuildingHotbarSlots> items{};
    std::size_t count{};

    [[nodiscard]] std::optional<std::size_t> indexOf(
        ModularBuildPiece piece) const {
        if (piece == ModularBuildPiece::FloorPlatform) {
            piece = ModularBuildPiece::Foundation;
        }
        for (std::size_t index = 0; index < count; ++index) {
            if (items[index].modularPiece == piece) return index;
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::size_t> indexOf(
        BuildingType type) const {
        for (std::size_t index = 0; index < count; ++index) {
            if (items[index].buildingType == type) return index;
        }
        return std::nullopt;
    }
};

[[nodiscard]] inline ConstructionHotbarLayout
makeConstructionHotbarLayout(
    const std::array<bool, GameBalance::BuildingTypeCount>& unlocked,
    bool corePlaced, std::uint8_t coreLevel,
    bool bypassCoreRestrictions = false) {
    ConstructionHotbarLayout result;
    if (!corePlaced) {
        if (unlocked[static_cast<std::size_t>(BuildingType::Core)]) {
            result.items[result.count++].buildingType = BuildingType::Core;
        }
        return result;
    }
    for (const auto category : {BuildingHotbarCategory::Defense,
                               BuildingHotbarCategory::Economy,
                               BuildingHotbarCategory::Modular}) {
        const BuildingHotbarLayout buildings = makeBuildingHotbarLayout(
            unlocked, category, true, coreLevel, bypassCoreRestrictions);
        for (const BuildingType type : buildings.visible()) {
            auto& item = result.items[result.count];
            item.buildingType = type;
            item.gapBefore = result.count > 0U &&
                result.items[result.count - 1U].buildingType &&
                buildingHotbarCategory(*result.items[result.count - 1U].buildingType) != category;
            ++result.count;
        }
    }
    for (const ModularBuildPiece piece : ModularBuildHotbarOrder) {
        if (bypassCoreRestrictions ||
            coreLevel >= modularBuildPieceRequiredCoreLevel(piece)) {
            auto& item = result.items[result.count];
            item.modularPiece = piece;
            item.gapBefore = result.count > 0U &&
                !result.items[result.count - 1U].modularPiece &&
                result.items[result.count - 1U].buildingType != BuildingType::Wall;
            ++result.count;
        }
    }
    return result;
}

} // namespace ian
