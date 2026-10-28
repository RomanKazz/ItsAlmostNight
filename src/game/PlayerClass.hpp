#pragma once

#include "buildings/BuildingSystem.hpp"
#include "combat/PlayerWeaponSystem.hpp"
#include "game/LootChestSystem.hpp"

#include <optional>

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
    Stump, // Reserved legacy IDs remain valid for suspended runs.
    Lumberjack,
    Sharpshooter,
    Mechanic,
    Miner,
};

inline constexpr PlayerClass DefaultPlayerClass = PlayerClass::Miner;

struct PlayerClassDefinition {
    PlayerClass type{PlayerClass::None};
    std::string_view name;
    std::string_view role;
    std::array<std::string_view, 3> traits;
    std::string_view startingLoadoutLabel;
    std::string_view passiveName;
    std::array<std::string_view, 3> passiveDescription{};
    std::array<std::string_view, 2> levelGrowth{};
    std::string_view drawback;
    PlayerWeapon startingWeapon{PlayerWeapon::Axe};
    std::array<std::optional<LootUpgradeEffect>, 2> startingItems{};
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

inline constexpr PlayerClassDefinition LegacyVanguardDefinition{
            .type = PlayerClass::Vanguard,
            .name = "VANGUARD",
            .role = "Hold the line",
            .traits = {"25 RECHARGING ARMOR", "ARMOR BREAK PUSHES ENEMIES", "-10% DAMAGE"},
            .startingLoadoutLabel = "GUN TURRET: +15% FIRE RATE",
            .passiveName = "IRON WILL",
            .passiveDescription = {"25 recharging armor protects you from hits.", "Breaking the armor repels nearby enemies.", "Base: +35% HP, 20% damage reduction; -10% damage."},
            .levelGrowth = {"+2 recharging armor.", "+0.05m shockwave radius (up to 8m)."},
            .drawback = "-10% player damage.",
            .startingWeapon = PlayerWeapon::Axe,
            .damageMultiplier = 0.90,
            .maxHealthMultiplier = 1.35,
            .armorMultiplier = 1.25,
            .startingBuilding = BuildingType::GunTurret,
            .startingBuildingDamageMultiplier = 1.0,
            .startingBuildingFireRateMultiplier = 1.15,
        };

inline constexpr std::array<PlayerClassDefinition, 8>
    LegacyPlayerClassDefinitions{{
        {
            .type = PlayerClass::Stump,
            .name = "STUMP",
            .role = "Wake the forest beneath them",
            .traits = {"EVERY 4TH MELEE HIT: ROOT WAVE", "+25% MAX HEALTH", "-10% MOVE SPEED"},
            .startingLoadoutLabel = "CLUB + SPIKES BLUEPRINT",
            .passiveName = "ROOTS AWAKEN",
            .passiveDescription = {"Every fourth successful melee swing awakens a wave of roots.", "Roots deal 70% of the hit and slow grounded enemies by 40% for 1.5s.", "Base: +25% HP. One charge per swing; misses and trees do not count."},
            .levelGrowth = {"+2% melee damage.", "+0.5% root wave size."},
            .drawback = "-10% movement speed.",
            .startingWeapon = PlayerWeapon::Club,
            .moveSpeedMultiplier = 0.90,
            .maxHealthMultiplier = 1.25,
            .startingBuilding = BuildingType::SpikeTrap,
        },
        {
            .type = PlayerClass::Ranger,
            .name = "RANGER",
            .role = "Fight on the move",
            .traits = {"MOVING: +20% DAMAGE", "+25% BASE DAMAGE", "-20% MAX HEALTH"},
            .startingLoadoutLabel = "CROSSBOW: +25% FIRE RATE",
            .passiveName = "RUN AND GUN",
            .passiveDescription = {"Moving grants +20% damage.", "Keep moving to reload your rifle normally.", "Base: +25% damage, +12% movement speed; -20% HP."},
            .levelGrowth = {"+1% moving damage (up to +70%).", "+0.5% movement speed (up to +50%)."},
            .drawback = "Rifle reload pauses when you stop moving.",
            .startingWeapon = PlayerWeapon::Rifle,
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
            .passiveName = "FIELD WORKSHOP",
            .passiveDescription = {"Repairs fortify structures for longer.", "Repair cooldown is halved.", "Base: +35% building HP, +25% defense damage; -10% damage."},
            .levelGrowth = {"+2% defense damage.", "+0.25s fortification duration."},
            .drawback = "-10% player damage.",
            .startingWeapon = PlayerWeapon::Bomb,
            .startingItems = {LootUpgradeEffect::Blueprint},
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
            .passiveName = "CHAIN HARVEST",
            .passiveDescription = {"Depleted deposits hit 2 nearby deposits of the same resource.", "Chained hits deal 50% harvest damage within 4m.", "Base: +35% yield, +25% capacity, +20% production; -15% damage."},
            .levelGrowth = {"+0.03m chain radius (up to 7m); +2% gathering yield.", "+2% carry capacity."},
            .drawback = "-15% player damage.",
            .startingWeapon = PlayerWeapon::Pickaxe,
            .startingItems = {LootUpgradeEffect::HealthAid},
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
            .passiveName = "BLOOD RAGE",
            .passiveDescription = {"Taking HP damage grants 4s of rage: wider, faster melee hits.", "Rage kills heal 4 HP and extend rage by 0.4s.", "Missing HP: up to +75% damage and melee speed; -15% max HP."},
            .levelGrowth = {"+0.04s rage duration (up to 6s); +1% melee speed.", "+2% maximum missing-health damage."},
            .drawback = "After rage, 2s exhaustion prevents another rage.",
            .startingWeapon = PlayerWeapon::Club,
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
            .passiveName = "BLOOD FEAST",
            .passiveDescription = {"Kills restore health.", "Excess healing builds a blood shield.", "Base: +10% damage; -25% max HP."},
            .levelGrowth = {"+0.1 HP healed per kill.", "+1 HP blood shield capacity."},
            .drawback = "-25% max health.",
            .startingWeapon = PlayerWeapon::Axe,
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
            .passiveName = "UNSTABLE BREW",
            .passiveDescription = {"Apples heal 50% more.", "Battle potions last 3s longer.", "Base: +10% attack speed; -10% movement speed."},
            .levelGrowth = {"Rearms your apple; +2% apple healing.", "+0.1s potion duration."},
            .drawback = "-10% movement speed.",
            .startingWeapon = PlayerWeapon::FireWand,
            .startingItems = {LootUpgradeEffect::Apple, LootUpgradeEffect::Potion},
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
            .traits = {"STARTS WITH ICE WAND", "HOURGLASS + ROPE", "-15% MAX HEALTH"},
            .startingLoadoutLabel = "SLOW TRAP: +35% TRIGGER RATE",
            .passiveName = "BORROWED TIME",
            .passiveDescription = {"Starts with Ice Wand, Hourglass and Safety Rope.", "Slow Trap starts with +35% trigger rate.", "Base: +10% movement speed; -15% max HP."},
            .levelGrowth = {"+0.5% movement speed (up to +50%).", ""},
            .drawback = "-15% max health.",
            .startingWeapon = PlayerWeapon::IceWand,
            .startingItems = {LootUpgradeEffect::Hourglass, LootUpgradeEffect::Rope},
            .moveSpeedMultiplier = 1.10,
            .maxHealthMultiplier = 0.85,
            .startingBuilding = BuildingType::SlowTrap,
            .startingBuildingDamageMultiplier = 1.0,
            .startingBuildingFireRateMultiplier = 1.35,
        },
    }};

// The selectable test roster. Legacy definitions are used only when resuming
// existing runs; new characters never inherit their special abilities.
inline constexpr std::array<PlayerClassDefinition, 4> PlayerClassDefinitions{{
    {
        .type = PlayerClass::Lumberjack,
        .name = "LUMBERJACK",
        .role = "Chop wood and fight up close",
        .traits = {"+20% MELEE DAMAGE", "+20% WOOD FROM TREES", "-10% MOVE SPEED"},
        .startingLoadoutLabel = "CLUB + SPIKES BLUEPRINT",
        .passiveName = "HEAVY SWING",
        .passiveDescription = {"+20% melee damage.", "+20% wood from manual harvesting.", "All bonuses are always active."},
        .levelGrowth = {"+2% base melee damage.", "+3 maximum HP."},
        .drawback = "-10% movement speed.",
        .startingWeapon = PlayerWeapon::Club,
        .moveSpeedMultiplier = .90,
        .startingBuilding = BuildingType::SpikeTrap,
    },
    {
        .type = PlayerClass::Sharpshooter,
        .name = "SHARPSHOOTER",
        .role = "Pick off dangerous targets",
        .traits = {"+15% RANGED ATTACK SPEED", "+2% RANGED DAMAGE PER LEVEL", "-15% MAX HEALTH"},
        .startingLoadoutLabel = "RIFLE + CROSSBOW BLUEPRINT",
        .passiveName = "QUICK HANDS",
        .passiveDescription = {"+15% attack speed with ranged weapons.", "Works with the rifle, wands and bombs.", "Reload normally even while standing still."},
        .levelGrowth = {"+2% base ranged damage.", "+2 maximum HP."},
        .drawback = "-15% maximum health.",
        .startingWeapon = PlayerWeapon::Rifle,
        .maxHealthMultiplier = .85,
        .startingBuilding = BuildingType::Turret,
    },
    {
        .type = PlayerClass::Mechanic,
        .name = "MECHANIC",
        .role = "Build a fort that lasts",
        .traits = {"+20% BUILDING HEALTH", "REPAIR COST -30%", "-15% PLAYER DAMAGE"},
        .startingLoadoutLabel = "CLUB + CANNON BLUEPRINT",
        .passiveName = "FIELD REPAIRS",
        .passiveDescription = {"+20% health for buildings and platforms.", "Manual repairs cost 30% fewer resources.", "Repair restores full health as usual."},
        .levelGrowth = {"+2% base defense damage.", "+2 maximum HP."},
        .drawback = "-15% player weapon damage.",
        .startingWeapon = PlayerWeapon::Club,
        .damageMultiplier = .85,
        .buildingHealthMultiplier = 1.20,
        .startingBuilding = BuildingType::Cannon,
    },
    {
        .type = PlayerClass::Miner,
        .name = "MINER",
        .role = "Gather more and grow your economy",
        .traits = {"+20% MANUAL RESOURCE YIELD", "+2% PRODUCTION PER LEVEL", "-10% DEFENSE DAMAGE"},
        .startingLoadoutLabel = "GUN TURRET BLUEPRINT + PROSPECTOR PAN",
        .passiveName = "RICH DEPOSITS",
        .passiveDescription = {"+20% resources from manual harvesting.", "Starts with a Prospector Pan.", "No chain harvesting or special attacks."},
        .levelGrowth = {"+2% base resource production speed.", "+2 maximum HP."},
        .drawback = "-10% defense damage.",
        .startingWeapon = PlayerWeapon::Axe,
        .startingItems = {LootUpgradeEffect::CrystalPan},
        .defenseDamageMultiplier = .90,
        .startingBuilding = BuildingType::GunTurret,
    },
}};

[[nodiscard]] inline constexpr bool isSelectablePlayerClass(PlayerClass type) {
    for (const auto& definition : PlayerClassDefinitions) if (definition.type == type) return true;
    return false;
}

[[nodiscard]] inline constexpr const PlayerClassDefinition*
playerClassDefinition(PlayerClass type) {
    for (const auto& definition : PlayerClassDefinitions) if (definition.type == type) return &definition;
    for (const auto& definition : LegacyPlayerClassDefinitions) if (definition.type == type) return &definition;
    return type == PlayerClass::Vanguard ? &LegacyVanguardDefinition : nullptr;
}

} // namespace ian
