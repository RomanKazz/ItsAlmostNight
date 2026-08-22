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
    std::array<std::string_view, 5> startingNodeIds;
    std::string_view startingNodesLabel;
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
            .role = "Stand your ground",
            .traits = {"+35% MAX HEALTH", "+20% DAMAGE RESIST", "-10% DAMAGE"},
            .startingNodeIds = {"combat_training", "club", ""},
            .startingNodesLabel = "COMBAT TRAINING · CLUB",
            .damageMultiplier = 0.90,
            .maxHealthMultiplier = 1.35,
            .armorMultiplier = 1.25,
        },
        {
            .type = PlayerClass::Ranger,
            .name = "RANGER",
            .role = "Move fast, kill first",
            .traits = {"+25% DAMAGE", "+12% MOVE SPEED", "-20% MAX HEALTH"},
            .startingNodeIds = {"combat_training", "rifle", ""},
            .startingNodesLabel = "COMBAT TRAINING · RIFLE",
            .damageMultiplier = 1.25,
            .moveSpeedMultiplier = 1.12,
            .maxHealthMultiplier = 0.80,
        },
        {
            .type = PlayerClass::Engineer,
            .name = "ENGINEER",
            .role = "Let the fort fight",
            .traits = {"+35% BUILDING HEALTH", "+25% DEFENSE DAMAGE", "-10% DAMAGE"},
            .startingNodeIds = {"hammer", "defense_engineering", ""},
            .startingNodesLabel = "HAMMER · DEFENSE ENGINEERING",
            .damageMultiplier = 0.90,
            .buildingHealthMultiplier = 1.35,
            .defenseDamageMultiplier = 1.25,
        },
        {
            .type = PlayerClass::Prospector,
            .name = "PROSPECTOR",
            .role = "Strip the land, feed the fort",
            .traits = {"AXE + PICKAXE", "+20% PRODUCTION SPEED", "-15% DAMAGE"},
            .startingNodeIds = {"axe", "pickaxe", "efficient_strikes"},
            .startingNodesLabel = "AXE · PICKAXE · EFFICIENT STRIKES",
            .damageMultiplier = 0.85,
            .productionSpeedMultiplier = 1.20,
        },
        {
            .type = PlayerClass::Berserker,
            .name = "BERSERKER",
            .role = "Pain becomes power",
            .traits = {"LOST HP: UP TO +75% DAMAGE", "CLUB + DASH", "-15% MAX HEALTH"},
            .startingNodeIds = {
                "combat_training", "club", "light_footwork", "sprinter", "dash"},
            .startingNodesLabel = "CLUB · DASH · RAGE",
            .maxHealthMultiplier = 0.85,
        },
        {
            .type = PlayerClass::Vampire,
            .name = "VAMPIRE",
            .role = "Feed on the horde",
            .traits = {"EVERY KILL RESTORES 2 HP", "+10% DAMAGE", "-25% MAX HEALTH"},
            .startingNodeIds = {"combat_training", "rifle", ""},
            .startingNodesLabel = "RIFLE · BLOOD FEAST",
            .damageMultiplier = 1.10,
            .maxHealthMultiplier = 0.75,
        },
        {
            .type = PlayerClass::Alchemist,
            .name = "ALCHEMIST",
            .role = "Turn danger into medicine",
            .traits = {"STARTS WITH APPLE + POTION", "+10% ATTACK SPEED", "-10% MOVE SPEED"},
            .startingNodeIds = {"combat_training", "club", ""},
            .startingNodesLabel = "CLUB · APPLE · POTION",
            .attackSpeedMultiplier = 1.10,
            .moveSpeedMultiplier = 0.90,
        },
        {
            .type = PlayerClass::Chronomancer,
            .name = "CHRONOMANCER",
            .role = "Make every second dangerous",
            .traits = {"STARTS WITH HOURGLASS + ROPE", "+10% MOVE SPEED", "-15% MAX HEALTH"},
            .startingNodeIds = {"combat_training", "rifle", ""},
            .startingNodesLabel = "RIFLE · HOURGLASS · ROPE",
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
