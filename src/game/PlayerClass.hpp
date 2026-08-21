#pragma once

#include <array>
#include <string_view>

namespace ian {

enum class PlayerClass {
    None,
    Vanguard,
    Ranger,
    Engineer,
};

struct PlayerClassDefinition {
    PlayerClass type{PlayerClass::None};
    std::string_view name;
    std::string_view role;
    std::array<std::string_view, 3> traits;
    double damageMultiplier{1.0};
    double attackSpeedMultiplier{1.0};
    double moveSpeedMultiplier{1.0};
    double maxHealthMultiplier{1.0};
    double armorMultiplier{1.0};
    double buildingHealthMultiplier{1.0};
    double defenseDamageMultiplier{1.0};
};

inline constexpr std::array<PlayerClassDefinition, 3>
    PlayerClassDefinitions{{
        {
            .type = PlayerClass::Vanguard,
            .name = "VANGUARD",
            .role = "Stand your ground",
            .traits = {"+35% MAX HEALTH", "+20% DAMAGE RESIST", "-10% DAMAGE"},
            .damageMultiplier = 0.90,
            .maxHealthMultiplier = 1.35,
            .armorMultiplier = 1.25,
        },
        {
            .type = PlayerClass::Ranger,
            .name = "RANGER",
            .role = "Move fast, kill first",
            .traits = {"+25% DAMAGE", "+12% MOVE SPEED", "-20% MAX HEALTH"},
            .damageMultiplier = 1.25,
            .moveSpeedMultiplier = 1.12,
            .maxHealthMultiplier = 0.80,
        },
        {
            .type = PlayerClass::Engineer,
            .name = "ENGINEER",
            .role = "Let the fort fight",
            .traits = {"+35% BUILDING HEALTH", "+25% DEFENSE DAMAGE", "-10% DAMAGE"},
            .damageMultiplier = 0.90,
            .buildingHealthMultiplier = 1.35,
            .defenseDamageMultiplier = 1.25,
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
