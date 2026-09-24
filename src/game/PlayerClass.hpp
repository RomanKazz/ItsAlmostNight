#pragma once

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
};

inline constexpr std::array<PlayerClassDefinition, 8>
    PlayerClassDefinitions{{
        {
            .type = PlayerClass::Vanguard,
            .name = "VANGUARD",
            .role = "Hold the line",
            .traits = {"25 RECHARGING ARMOR", "ARMOR BREAK PUSHES ENEMIES", "-10% DAMAGE"},
            .startingLoadoutLabel = "RECHARGING ARMOR",
            .damageMultiplier = 0.90,
            .maxHealthMultiplier = 1.35,
            .armorMultiplier = 1.25,
        },
        {
            .type = PlayerClass::Ranger,
            .name = "RANGER",
            .role = "Fight on the move",
            .traits = {"MOVING: +20% DAMAGE", "+25% BASE DAMAGE", "-20% MAX HEALTH"},
            .startingLoadoutLabel = "MOMENTUM DAMAGE",
            .damageMultiplier = 1.25,
            .moveSpeedMultiplier = 1.12,
            .maxHealthMultiplier = 0.80,
        },
        {
            .type = PlayerClass::Engineer,
            .name = "ENGINEER",
            .role = "Keep the fort alive",
            .traits = {"REPAIR FORTIFIES LONGER", "REPAIR COOLDOWN -50%", "-10% PLAYER DAMAGE"},
            .startingLoadoutLabel = "LONGER FORTIFICATION",
            .damageMultiplier = 0.90,
            .buildingHealthMultiplier = 1.35,
            .defenseDamageMultiplier = 1.25,
        },
        {
            .type = PlayerClass::Prospector,
            .name = "PROSPECTOR",
            .role = "Harvest and build",
            .traits = {"+35% GATHERING YIELD", "+25% CARRY CAPACITY", "-15% PLAYER DAMAGE"},
            .startingLoadoutLabel = "LARGER RESOURCE PACK",
            .damageMultiplier = 0.85,
            .productionSpeedMultiplier = 1.20,
        },
        {
            .type = PlayerClass::Berserker,
            .name = "BERSERKER",
            .role = "Pain becomes power",
            .traits = {"LOST HP: UP TO +75% DAMAGE", "LOW HP: FASTER MELEE", "-15% MAX HEALTH"},
            .startingLoadoutLabel = "MISSING HEALTH FUELS RAGE",
            .maxHealthMultiplier = 0.85,
        },
        {
            .type = PlayerClass::Vampire,
            .name = "VAMPIRE",
            .role = "Feed on the horde",
            .traits = {"KILLS HEAL OR BUILD A SHIELD", "+10% DAMAGE", "-25% MAX HEALTH"},
            .startingLoadoutLabel = "OVERHEAL SHIELD",
            .damageMultiplier = 1.10,
            .maxHealthMultiplier = 0.75,
        },
        {
            .type = PlayerClass::Alchemist,
            .name = "ALCHEMIST",
            .role = "Turn danger into medicine",
            .traits = {"APPLE +50%, POTION +3S", "STARTS WITH FIRE WAND", "-10% MOVE SPEED"},
            .startingLoadoutLabel = "FIRE WAND + APPLE + POTION",
            .attackSpeedMultiplier = 1.10,
            .moveSpeedMultiplier = 0.90,
        },
        {
            .type = PlayerClass::Chronomancer,
            .name = "CHRONOMANCER",
            .role = "Make every second dangerous",
            .traits = {"DASH SLOWS NEARBY ENEMIES", "HOURGLASS + ROPE", "-15% MAX HEALTH"},
            .startingLoadoutLabel = "SLOWING DASH",
            .moveSpeedMultiplier = 1.10,
            .maxHealthMultiplier = 0.85,
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
