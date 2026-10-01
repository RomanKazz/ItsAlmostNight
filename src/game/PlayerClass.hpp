#pragma once

#include "buildings/BuildingSystem.hpp"

#include <array>
#include <string_view>

namespace ian {

enum class PlayerClass {
    None,
    Vanguard,
    Ranger,
    Engineer,
    Prospector,
    Berserker,
    Vampire,
    Alchemist,
    Chronomancer,
};

struct PlayerClassDefinition {
    PlayerClass type{PlayerClass::None};
    std::string_view name;
    std::string_view role;
    std::array<std::string_view, 3> traits;
    std::string_view startingLoadoutLabel;
    double damageMultiplier{1.0};
    double attackSpeedMultiplier{1.0};
    double moveSpeedMultiplier{1.0};
    double maxHealthMultiplier{1.0};
    double armorMultiplier{1.0};
    double buildingHealthMultiplier{1.0};
    double defenseDamageMultiplier{1.0};
    double productionSpeedMultiplier{1.0};
    BuildingType startingBuilding{BuildingType::GunTurret};
    double startingBuildingDamageMultiplier{1.0};
    double startingBuildingFireRateMultiplier{1.0};
};

inline constexpr std::array<PlayerClassDefinition, 8>
    PlayerClassDefinitions{{
        {
            .type = PlayerClass::Vanguard,
            .name = "VANGUARD",
            .role = "Hold the line",
            .traits = {"25 RECHARGING ARMOR", "ARMOR BREAK PUSHES ENEMIES", "-10% DAMAGE"},
            .startingLoadoutLabel = "GUN TURRET: +15% FIRE RATE",
            .damageMultiplier = 0.90,
            .maxHealthMultiplier = 1.35,
            .armorMultiplier = 1.25,
            .startingBuilding = BuildingType::GunTurret,
            .startingBuildingDamageMultiplier = 1.0,
            .startingBuildingFireRateMultiplier = 1.15,
        },
        {
            .type = PlayerClass::Ranger,
            .name = "RANGER",
            .role = "Fight on the move",
            .traits = {"MOVING: +20% DAMAGE", "+25% BASE DAMAGE", "-20% MAX HEALTH"},
            .startingLoadoutLabel = "CROSSBOW: +25% FIRE RATE",
            .damageMultiplier = 1.25,
            .moveSpeedMultiplier = 1.12,
            .maxHealthMultiplier = 0.80,
            .startingBuilding = BuildingType::Turret,
            .startingBuildingDamageMultiplier = 1.0,
            .startingBuildingFireRateMultiplier = 1.25,
        },
        {
            .type = PlayerClass::Engineer,
            .name = "ENGINEER",
            .role = "Keep the fort alive",
            .traits = {"REPAIR FORTIFIES LONGER", "REPAIR COOLDOWN -50%", "-10% PLAYER DAMAGE"},
            .startingLoadoutLabel = "CANNON: +30% DAMAGE",
            .damageMultiplier = 0.90,
            .buildingHealthMultiplier = 1.35,
            .defenseDamageMultiplier = 1.25,
            .startingBuilding = BuildingType::Cannon,
            .startingBuildingDamageMultiplier = 1.3,
            .startingBuildingFireRateMultiplier = 1.0,
        },
        {
            .type = PlayerClass::Prospector,
            .name = "PROSPECTOR",
            .role = "Harvest and build",
            .traits = {"+35% GATHERING YIELD", "+25% CARRY CAPACITY", "-15% PLAYER DAMAGE"},
            .startingLoadoutLabel = "CROSSBOW: +15% DAMAGE",
            .damageMultiplier = 0.85,
            .productionSpeedMultiplier = 1.20,
            .startingBuilding = BuildingType::Turret,
            .startingBuildingDamageMultiplier = 1.15,
            .startingBuildingFireRateMultiplier = 1.0,
        },
        {
            .type = PlayerClass::Berserker,
            .name = "BERSERKER",
            .role = "Pain becomes power",
            .traits = {"LOST HP: UP TO +75% DAMAGE", "LOW HP: FASTER MELEE", "-15% MAX HEALTH"},
            .startingLoadoutLabel = "SPIKES: +50% DAMAGE",
            .maxHealthMultiplier = 0.85,
            .startingBuilding = BuildingType::SpikeTrap,
            .startingBuildingDamageMultiplier = 1.5,
            .startingBuildingFireRateMultiplier = 1.0,
        },
        {
            .type = PlayerClass::Vampire,
            .name = "VAMPIRE",
            .role = "Feed on the horde",
            .traits = {"KILLS HEAL OR BUILD A SHIELD", "+10% DAMAGE", "-25% MAX HEALTH"},
            .startingLoadoutLabel = "GUN TURRET: +25% DAMAGE",
            .damageMultiplier = 1.10,
            .maxHealthMultiplier = 0.75,
            .startingBuilding = BuildingType::GunTurret,
            .startingBuildingDamageMultiplier = 1.25,
            .startingBuildingFireRateMultiplier = 1.0,
        },
        {
            .type = PlayerClass::Alchemist,
            .name = "ALCHEMIST",
            .role = "Turn danger into medicine",
            .traits = {"APPLE +50%, POTION +3S", "STARTS WITH FIRE WAND", "-10% MOVE SPEED"},
            .startingLoadoutLabel = "CATAPULT: +25% DAMAGE",
            .attackSpeedMultiplier = 1.10,
            .moveSpeedMultiplier = 0.90,
            .startingBuilding = BuildingType::Catapult,
            .startingBuildingDamageMultiplier = 1.25,
            .startingBuildingFireRateMultiplier = 1.0,
        },
        {
            .type = PlayerClass::Chronomancer,
            .name = "CHRONOMANCER",
            .role = "Make every second dangerous",
            .traits = {"DASH SLOWS NEARBY ENEMIES", "HOURGLASS + ROPE", "-15% MAX HEALTH"},
            .startingLoadoutLabel = "SLOW TRAP: +35% TRIGGER RATE",
            .moveSpeedMultiplier = 1.10,
            .maxHealthMultiplier = 0.85,
            .startingBuilding = BuildingType::SlowTrap,
            .startingBuildingDamageMultiplier = 1.0,
            .startingBuildingFireRateMultiplier = 1.35,
        },
    }};

[[nodiscard]] inline constexpr const PlayerClassDefinition*
playerClassDefinition(PlayerClass type) {
    for (const PlayerClassDefinition& definition :
         PlayerClassDefinitions) {
        if (definition.type == type) {
            return &definition;
        }
    }
    return nullptr;
}

} // namespace ian
