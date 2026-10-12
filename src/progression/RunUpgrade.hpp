#pragma once

#include "buildings/BuildingSystem.hpp"
#include "progression/MetaShop.hpp"
#include "combat/CannonAttack.hpp"
#include "combat/PlayerWeaponSystem.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <optional>
#include <limits>

namespace ian {

enum class RunUpgradeEffect {
    Damage,
    AttackSpeed,
    MoveSpeed,
    MaximumHealth,
    RecoverableArmor,
    BuildingHealth,
    BuildRadius,
    DefenseDamage,
    DefenseFireRate,
    ProductionSpeed,
    NightlyBomb,
    WiderChoice,
    BloodHarvest,
    Overkill,
    Ricochet,
    DoubleDown,
    LockChoice,
    RerollToken,
    RiskyInvestment,
    Salvager,
    TwinBatteries,
    ClusterPayload,
    ReactiveTraps,
    GunPiercing,
    GunShrapnel,
    GatheringSpeed,
    LongerDay,
    RichResources,
    Luck,
    // Append IDs: existing suspended runs store upgrade indices.
    UnlockGunTurret,
    UnlockTurret,
    UnlockCannon,
    UnlockCatapult,
    UnlockSpikeTrap,
    UnlockSlowTrap,
    CannonHeavyShell,
    CannonScattershot,
    CannonClusterShell,
    UnlockLumberMill,
    UnlockQuarry,
    UnlockCrystalMine,
    LumberMillProduction,
    QuarryProduction,
    CrystalMineProduction,
    Regeneration,
    UnlockClub,
    UnlockRifle,
    UnlockBomb,
    UnlockIceWand,
    UnlockFireWand,
    ProjectileSpeed,
    AttackDuration,
    AttackSize,
    CriticalChance,
    CriticalDamage,
    AttackKnockback,
    PoisonChance,
};

inline constexpr std::size_t RunUpgradeEffectCount = 57U;
// Poison deals 25% of the triggering hit per second for four seconds.
// Reapplication refreshes duration and retains the strongest dose.
inline constexpr double PoisonDuration = 4.0;
inline constexpr double PoisonDamageFraction = 0.25;
[[nodiscard]] inline double runPoisonChance(double power) {
    return std::clamp(0.10 * power, 0.0, 1.0);
}

inline constexpr std::size_t MinimumRunUpgradeChoices = 3U;
inline constexpr std::size_t MaximumRunUpgradeChoices = 5U;

struct RunUpgradeDefinition {
    RunUpgradeEffect effect;
    std::string_view name;
    std::string_view description;
};

// A tome is a persistent run stat item. Its level/power use the existing
// stable upgrade ID, so a renamed card never applies the bonus a second time.
struct RunTomeDefinition {
    RunUpgradeEffect effect;
    std::string_view name;
    std::string_view mark;
    std::uint32_t colorRgb;
};

inline constexpr std::array<RunTomeDefinition, 27> RunTomeDefinitions{{
    {RunUpgradeEffect::BuildingHealth, "FORTITUDE TOME", "DEF", 0x78A7FF},
    {RunUpgradeEffect::ProductionSpeed, "INDUSTRY TOME", "PROD", 0x72DA92},
    {RunUpgradeEffect::Luck, "FORTUNE TOME", "LUCK", 0xA36CEF},
    {RunUpgradeEffect::BuildRadius, "REACH TOME", "AREA", 0x58C2BB},
    {RunUpgradeEffect::RichResources, "HARVEST TOME", "YIELD", 0xD3B061},
    {RunUpgradeEffect::MaximumHealth, "VITALITY TOME", "HP", 0xFF647F},
    {RunUpgradeEffect::RecoverableArmor, "GUARD TOME", "SHLD", 0x68DBFF},
    {RunUpgradeEffect::MoveSpeed, "SWIFT TOME", "MOVE", 0x73D1CC},
    {RunUpgradeEffect::AttackSpeed, "RAPID TOME", "SPD", 0x4DDCF2},
    {RunUpgradeEffect::Damage, "MIGHT TOME", "ATK", 0xFFAD43},
    {RunUpgradeEffect::Regeneration, "REGEN TOME", "REGEN", 0x8BDA66},
    {RunUpgradeEffect::Salvager, "SALVAGE TOME", "SCRAP", 0xD8B072},
    {RunUpgradeEffect::LumberMillProduction, "SAWMILL TOME", "WOOD", 0xA9D96D},
    {RunUpgradeEffect::QuarryProduction, "QUARRY TOME", "ROCK", 0x9DB7D6},
    {RunUpgradeEffect::CrystalMineProduction, "CRYSTAL TOME", "GEM", 0xCD87F6},
    {RunUpgradeEffect::BloodHarvest, "VAMPIRIC TOME", "HEAL", 0xF56891},
    {RunUpgradeEffect::Ricochet, runTomeDefinition(RunUpgradeEffect::Ricochet)->name, "BOUNCE", 0x75C9FF},
    {RunUpgradeEffect::TwinBatteries, runTomeDefinition(RunUpgradeEffect::TwinBatteries)->name, "MULTI", 0xFFE078},
    {RunUpgradeEffect::GunPiercing, runTomeDefinition(RunUpgradeEffect::GunPiercing)->name, "PIERCE", 0x73D1CC},
    {RunUpgradeEffect::GatheringSpeed, "GATHERING TOME", "TOOL", 0xA9D96D},
    {RunUpgradeEffect::ProjectileSpeed, "VELOCITY TOME", "VEL", 0x4DDCF2},
    {RunUpgradeEffect::AttackDuration, "DURATION TOME", "TIME", 0x8DAFFF},
    {RunUpgradeEffect::AttackSize, "SIZE TOME", "SIZE", 0xF69ED6},
    {RunUpgradeEffect::CriticalChance, "PRECISION TOME", "CRIT", 0xFFCF65},
    {RunUpgradeEffect::CriticalDamage, "FEROCITY TOME", "CRIT", 0xFF805D},
    {RunUpgradeEffect::AttackKnockback, "IMPACT TOME", "PUSH", 0x9DB7D6},
    {RunUpgradeEffect::PoisonChance, "VENOM TOME", "TOXIC", 0x8BDA66},
}};

[[nodiscard]] constexpr const RunTomeDefinition* runTomeDefinition(RunUpgradeEffect effect) {
    for (const auto& hammer : RunTomeDefinitions) if (hammer.effect == effect) return &hammer;
    return nullptr;
}

// Reserved effects retain saved bonuses and explicit debug access, but no normal offers.
[[nodiscard]] constexpr bool legacyRunTomeUpgrade(RunUpgradeEffect effect) {
    return effect == RunUpgradeEffect::DefenseDamage || effect == RunUpgradeEffect::DefenseFireRate ||
        effect == RunUpgradeEffect::LongerDay || effect == RunUpgradeEffect::Overkill;
}
[[nodiscard]] constexpr bool retiredRunUpgrade(RunUpgradeEffect effect);
[[nodiscard]] inline int runTomeLevel(RunUpgradeEffect effect, const std::array<int, RunUpgradeEffectCount>& stacks) {
    std::int64_t level = stacks[static_cast<std::size_t>(effect)];
    if (effect == RunUpgradeEffect::Damage) level += stacks[static_cast<std::size_t>(RunUpgradeEffect::DefenseDamage)];
    if (effect == RunUpgradeEffect::AttackSpeed) level += stacks[static_cast<std::size_t>(RunUpgradeEffect::DefenseFireRate)];
    return static_cast<int>(std::min(level, static_cast<std::int64_t>(std::numeric_limits<int>::max())));
}

// Ownership derives from stable upgrade stacks; old saves retain every bonus.
[[nodiscard]] inline int runTomeCount(const std::array<int, RunUpgradeEffectCount>& stacks) {
    return static_cast<int>(std::ranges::count_if(RunTomeDefinitions, [&stacks](const auto& tome) {
        return !retiredRunUpgrade(tome.effect) && runTomeLevel(tome.effect, stacks) > 0;
    }));
}
[[nodiscard]] inline int runTomeSlotLimit(const MetaRunBonuses& bonuses) {
    return BaseRunTomeSlots + std::clamp(bonuses.additionalTomeSlots, 0,
        MaximumRunTomeSlots - BaseRunTomeSlots);
}
// Source compatibility for existing HUD integrations; these resolve tome metadata.
using RunHammerDefinition = RunTomeDefinition;
inline constexpr const auto& RunHammerDefinitions = RunTomeDefinitions;
[[nodiscard]] constexpr const RunTomeDefinition* runHammerDefinition(RunUpgradeEffect effect) {
    return runTomeDefinition(effect);
}

inline constexpr std::array<RunUpgradeDefinition,
                            RunUpgradeEffectCount>
    RunUpgradeDefinitions{{
        {RunUpgradeEffect::Damage, runTomeDefinition(RunUpgradeEffect::Damage)->name,
         "+10% player damage and +12% defense damage"},
        {RunUpgradeEffect::AttackSpeed, runTomeDefinition(RunUpgradeEffect::AttackSpeed)->name,
         "+15% weapon attack speed and +10% defense speed"},
        {RunUpgradeEffect::MoveSpeed, runTomeDefinition(RunUpgradeEffect::MoveSpeed)->name,
         "+7% move speed"},
        {RunUpgradeEffect::MaximumHealth, runTomeDefinition(RunUpgradeEffect::MaximumHealth)->name,
         "+12 max HP"},
        {RunUpgradeEffect::RecoverableArmor, runTomeDefinition(RunUpgradeEffect::RecoverableArmor)->name,
         "+8 armor"},
        {RunUpgradeEffect::BuildingHealth, runTomeDefinition(RunUpgradeEffect::BuildingHealth)->name,
         "+12% building HP"},
        {RunUpgradeEffect::BuildRadius, runTomeDefinition(RunUpgradeEffect::BuildRadius)->name,
         "+3m build range"},
        {RunUpgradeEffect::DefenseDamage, "LEGACY DEFENSE DAMAGE",
         "+12% defense damage"},
        {RunUpgradeEffect::DefenseFireRate, "LEGACY DEFENSE SPEED",
         "+10% defense speed"},
        {RunUpgradeEffect::ProductionSpeed, runTomeDefinition(RunUpgradeEffect::ProductionSpeed)->name,
         "+15% production speed"},
        {RunUpgradeEffect::NightlyBomb, "BOMB SUPPLY",
         "+1 bomb each night"},
        {RunUpgradeEffect::WiderChoice, "MORE CHOICES",
         "+1 card choice"},
        {RunUpgradeEffect::BloodHarvest, runTomeDefinition(RunUpgradeEffect::BloodHarvest)->name,
         "+8 HP per 10 kills"},
        {RunUpgradeEffect::Overkill, "LEGACY OVERKILL",
         "+1 damage transfer"},
        {RunUpgradeEffect::Ricochet, runTomeDefinition(RunUpgradeEffect::Ricochet)->name,
         "+1 shot bounce"},
        {RunUpgradeEffect::DoubleDown, "DOUBLE DOWN",
         "+2 next-level picks"},
        {RunUpgradeEffect::LockChoice, "CARD LOCK",
         "+1 lock ability"},
        {RunUpgradeEffect::RerollToken, "REROLL",
         "+1 free reroll"},
        {RunUpgradeEffect::RiskyInvestment, "HIGH STAKES",
         "+2 picks, stronger enemies"},
        {RunUpgradeEffect::Salvager, runTomeDefinition(RunUpgradeEffect::Salvager)->name,
         "+30% sold or destroyed refund"},
        {RunUpgradeEffect::TwinBatteries, runTomeDefinition(RunUpgradeEffect::TwinBatteries)->name,
         "+1 side shot"},
        {RunUpgradeEffect::ClusterPayload, "CLUSTER PAYLOAD",
         "+2 mini explosions"},
        {RunUpgradeEffect::ReactiveTraps, "TRAP PULSE",
         "+6 shockwave damage"},
        {RunUpgradeEffect::GunPiercing, runTomeDefinition(RunUpgradeEffect::GunPiercing)->name,
         "+1 gun turret pierce target"},
        {RunUpgradeEffect::GunShrapnel, "TURRET SHRAPNEL",
         "+3 gun turret kill shards"},
        {RunUpgradeEffect::GatheringSpeed, runTomeDefinition(RunUpgradeEffect::GatheringSpeed)->name,
         "+15% gathering speed"},
        {RunUpgradeEffect::LongerDay, "LEGACY DAYLIGHT",
         "+5s build phase"},
        {RunUpgradeEffect::RichResources, runTomeDefinition(RunUpgradeEffect::RichResources)->name,
         "+15% resource node yield"},
        {RunUpgradeEffect::Luck, runTomeDefinition(RunUpgradeEffect::Luck)->name,
         "+10% upgrade luck"},
        {RunUpgradeEffect::UnlockGunTurret, "GUN TURRET", "Acquire defense / upgrade all copies"},
        {RunUpgradeEffect::UnlockTurret, "CROSSBOW", "Acquire defense / upgrade all copies"},
        {RunUpgradeEffect::UnlockCannon, "CANNON", "Acquire defense / upgrade all copies"},
        {RunUpgradeEffect::UnlockCatapult, "CATAPULT", "Acquire defense / upgrade all copies"},
        {RunUpgradeEffect::UnlockSpikeTrap, "SPIKES", "Acquire defense / upgrade all copies"},
        {RunUpgradeEffect::UnlockSlowTrap, "SLOW TRAP", "Acquire defense / upgrade all copies"},
        {RunUpgradeEffect::CannonHeavyShell, "HEAVY SHELL", "Cannon: x5 damage, fires 3x slower"},
        {RunUpgradeEffect::CannonScattershot, "SCATTERSHOT", "Cannon: 5 shells at 40% damage, smaller blasts"},
        {RunUpgradeEffect::CannonClusterShell, "CLUSTER SHELL", "Cannon: 80% damage, +3 mini explosions"},
        {RunUpgradeEffect::UnlockLumberMill, "LUMBER MILL", "Unlock lumber mill construction"},
        {RunUpgradeEffect::UnlockQuarry, "QUARRY", "Unlock quarry construction"},
        {RunUpgradeEffect::UnlockCrystalMine, "CRYSTAL MINE", "Unlock crystal mine construction"},
        {RunUpgradeEffect::LumberMillProduction, runTomeDefinition(RunUpgradeEffect::LumberMillProduction)->name, "+20% lumber mill production speed"},
        {RunUpgradeEffect::QuarryProduction, runTomeDefinition(RunUpgradeEffect::QuarryProduction)->name, "+20% quarry production speed"},
        {RunUpgradeEffect::CrystalMineProduction, runTomeDefinition(RunUpgradeEffect::CrystalMineProduction)->name, "+20% crystal mine production speed"},
        {RunUpgradeEffect::Regeneration, runTomeDefinition(RunUpgradeEffect::Regeneration)->name, "+0.5 HP per second"},
        {RunUpgradeEffect::UnlockClub, "CLUB", "Unlock club for this run"},
        {RunUpgradeEffect::UnlockRifle, "RIFLE", "Unlock rifle for this run"},
        {RunUpgradeEffect::UnlockBomb, "BOMBS", "Unlock bombs for this run"},
        {RunUpgradeEffect::UnlockIceWand, "ICE WAND", "Unlock ice wand for this run"},
        {RunUpgradeEffect::UnlockFireWand, "FIRE WAND", "Unlock fire wand for this run"},
        {RunUpgradeEffect::ProjectileSpeed, runTomeDefinition(RunUpgradeEffect::ProjectileSpeed)->name, "+20% defense projectile speed"},
        {RunUpgradeEffect::AttackDuration, runTomeDefinition(RunUpgradeEffect::AttackDuration)->name, "+20% defense attack duration"},
        {RunUpgradeEffect::AttackSize, runTomeDefinition(RunUpgradeEffect::AttackSize)->name, "+15% defense attack size"},
        {RunUpgradeEffect::CriticalChance, runTomeDefinition(RunUpgradeEffect::CriticalChance)->name, "+5% defense critical chance"},
        {RunUpgradeEffect::CriticalDamage, runTomeDefinition(RunUpgradeEffect::CriticalDamage)->name, "+25% defense critical damage"},
        {RunUpgradeEffect::AttackKnockback, runTomeDefinition(RunUpgradeEffect::AttackKnockback)->name, "+20% defense knockback"},
        {RunUpgradeEffect::PoisonChance, runTomeDefinition(RunUpgradeEffect::PoisonChance)->name, "+10% chance to poison on hit (25% hit damage/s, 4s)"},
    }};

[[nodiscard]] constexpr std::size_t runUpgradeIndex(
    RunUpgradeEffect effect) {
    return static_cast<std::size_t>(effect);
}

inline constexpr std::size_t RunBuildingSlotCount = 4U;
inline constexpr std::array<BuildingType, 6> RunBuildingTypes{
    BuildingType::GunTurret, BuildingType::Turret, BuildingType::Cannon,
    BuildingType::Catapult, BuildingType::SpikeTrap, BuildingType::SlowTrap};

inline constexpr std::array<BuildingType, 3> RunEconomyBuildingTypes{
    BuildingType::LumberMill, BuildingType::Quarry, BuildingType::CrystalMine};

[[nodiscard]] constexpr bool runCombatBuilding(BuildingType type) {
    return std::find(RunBuildingTypes.begin(), RunBuildingTypes.end(), type) != RunBuildingTypes.end();
}

[[nodiscard]] constexpr std::optional<BuildingType> runUpgradeProductionBuilding(RunUpgradeEffect effect) {
    switch (effect) {
    case RunUpgradeEffect::LumberMillProduction: return BuildingType::LumberMill;
    case RunUpgradeEffect::QuarryProduction: return BuildingType::Quarry;
    case RunUpgradeEffect::CrystalMineProduction: return BuildingType::CrystalMine;
    default: return std::nullopt;
    }
}

[[nodiscard]] constexpr std::optional<BuildingType> runUpgradeBuilding(RunUpgradeEffect effect) {
    switch (effect) {
    case RunUpgradeEffect::UnlockGunTurret: return BuildingType::GunTurret;
    case RunUpgradeEffect::UnlockTurret: return BuildingType::Turret;
    case RunUpgradeEffect::UnlockCannon: return BuildingType::Cannon;
    case RunUpgradeEffect::UnlockCatapult: return BuildingType::Catapult;
    case RunUpgradeEffect::UnlockSpikeTrap: return BuildingType::SpikeTrap;
    case RunUpgradeEffect::UnlockSlowTrap: return BuildingType::SlowTrap;
    case RunUpgradeEffect::UnlockLumberMill: return BuildingType::LumberMill;
    case RunUpgradeEffect::UnlockQuarry: return BuildingType::Quarry;
    case RunUpgradeEffect::UnlockCrystalMine: return BuildingType::CrystalMine;
    default: return std::nullopt;
    }
}

// Combat cards acquire a type at level one, then raise its shared level.
[[nodiscard]] constexpr bool runDefenseCard(RunUpgradeEffect effect) {
    const auto building = runUpgradeBuilding(effect);
    return building && runCombatBuilding(*building);
}

[[nodiscard]] constexpr int runDefenseLevelLimit(std::uint8_t coreLevel) {
    return std::clamp<int>(coreLevel, 1, MaxBuildingLevel);
}

[[nodiscard]] constexpr std::optional<RunUpgradeEffect> buildingRunUpgrade(BuildingType type) {
    for (std::size_t index = runUpgradeIndex(RunUpgradeEffect::UnlockGunTurret);
         index < RunUpgradeEffectCount; ++index) {
        const auto effect = static_cast<RunUpgradeEffect>(index);
        if (runUpgradeBuilding(effect) == type) return effect;
    }
    return std::nullopt;
}

[[nodiscard]] constexpr std::optional<PlayerWeapon> runUpgradeWeapon(RunUpgradeEffect effect) {
    switch (effect) {
    case RunUpgradeEffect::UnlockClub: return PlayerWeapon::Club;
    case RunUpgradeEffect::UnlockRifle: return PlayerWeapon::Rifle;
    case RunUpgradeEffect::UnlockBomb: return PlayerWeapon::Bomb;
    case RunUpgradeEffect::UnlockIceWand: return PlayerWeapon::IceWand;
    case RunUpgradeEffect::UnlockFireWand: return PlayerWeapon::FireWand;
    default: return std::nullopt;
    }
}

[[nodiscard]] constexpr std::optional<RunUpgradeEffect> weaponRunUpgrade(PlayerWeapon weapon) {
    for (const auto& definition : RunUpgradeDefinitions) {
        if (runUpgradeWeapon(definition.effect) == weapon) return definition.effect;
    }
    return std::nullopt;
}

[[nodiscard]] constexpr std::optional<CannonAttackStyle> runUpgradeCannonStyle(RunUpgradeEffect effect) {
    switch (effect) {
    case RunUpgradeEffect::CannonHeavyShell: return CannonAttackStyle::Heavy;
    case RunUpgradeEffect::CannonScattershot: return CannonAttackStyle::Scatter;
    case RunUpgradeEffect::CannonClusterShell: return CannonAttackStyle::Cluster;
    default: return std::nullopt;
    }
}

[[nodiscard]] constexpr CannonAttackStyle runCannonAttackStyle(
    const std::array<int, RunUpgradeEffectCount>& stacks) {
    for (const auto& definition : RunUpgradeDefinitions) {
        if (const auto style = runUpgradeCannonStyle(definition.effect);
            style && stacks[runUpgradeIndex(definition.effect)] > 0) return *style;
    }
    return CannonAttackStyle::Standard;
}

// These IDs remain reserved so suspended runs keep their card indices.
[[nodiscard]] constexpr bool retiredRunUpgrade(
    RunUpgradeEffect effect) {
    return legacyRunTomeUpgrade(effect) || effect == RunUpgradeEffect::BuildRadius ||
           effect == RunUpgradeEffect::Salvager ||
           effect == RunUpgradeEffect::LockChoice ||
           effect == RunUpgradeEffect::LumberMillProduction ||
           effect == RunUpgradeEffect::QuarryProduction ||
           effect == RunUpgradeEffect::CrystalMineProduction ||
           effect == RunUpgradeEffect::RerollToken ||
           effect == RunUpgradeEffect::DoubleDown;
}

[[nodiscard]] constexpr double runGatheringSpeedMultiplier(int stacks) {
    return 1.0 + 0.15 * static_cast<double>(stacks > 0 ? stacks : 0);
}

[[nodiscard]] constexpr const RunUpgradeDefinition&
runUpgradeDefinition(RunUpgradeEffect effect) {
    return RunUpgradeDefinitions[runUpgradeIndex(effect)];
}

enum class RunUpgradeRarity : std::uint8_t {
    Common, Uncommon, Rare, Epic, Legendary,
};

inline constexpr std::size_t RunUpgradeRarityCount = 5;

[[nodiscard]] constexpr std::string_view runUpgradeRarityName(RunUpgradeRarity rarity) {
    constexpr std::array<std::string_view, RunUpgradeRarityCount> Names{
        "COMMON", "UNCOMMON", "RARE", "EPIC", "LEGENDARY"};
    return Names[static_cast<std::size_t>(rarity)];
}

[[nodiscard]] constexpr double runUpgradeRarityMultiplier(RunUpgradeRarity rarity) {
    constexpr std::array<double, RunUpgradeRarityCount> Values{1.0, 1.5, 2.0, 2.5, 3.0};
    return Values[static_cast<std::size_t>(rarity)];
}

// Luck is a fraction: 0.5 means +50%. Higher tiers gain weight with
// diminishing returns; every rarity remains possible.
[[nodiscard]] inline std::array<double, RunUpgradeRarityCount>
runUpgradeRarityProbabilities(double luck) {
    std::array<double, RunUpgradeRarityCount> weights{60.0, 25.0, 10.0, 4.0, 1.0};
    const double strength = std::log1p(std::clamp(luck, 0.0, 100.0));
    double total = 0.0;
    for (std::size_t index = 0; index < weights.size(); ++index) {
        weights[index] *= std::exp(0.65 * static_cast<double>(index) * strength);
        total += weights[index];
    }
    for (double& weight : weights) weight /= total;
    return weights;
}

[[nodiscard]] inline RunUpgradeRarity rollRunUpgradeRarity(double luck, double roll) {
    const auto probabilities = runUpgradeRarityProbabilities(luck);
    double cumulative = 0.0;
    for (std::size_t index = 0; index < probabilities.size(); ++index) {
        cumulative += probabilities[index];
        if (roll < cumulative) return static_cast<RunUpgradeRarity>(index);
    }
    return RunUpgradeRarity::Legendary;
}

[[nodiscard]] constexpr bool fixedRunUpgradePower(RunUpgradeEffect effect) {
    return runUpgradeBuilding(effect).has_value() || runUpgradeWeapon(effect).has_value() || runUpgradeCannonStyle(effect).has_value() || effect == RunUpgradeEffect::DoubleDown ||
        effect == RunUpgradeEffect::RiskyInvestment ||
        effect == RunUpgradeEffect::RerollToken;
}

[[nodiscard]] constexpr bool discreteRunUpgradePower(RunUpgradeEffect effect) {
    return effect == RunUpgradeEffect::Overkill || effect == RunUpgradeEffect::Ricochet ||
        effect == RunUpgradeEffect::TwinBatteries || effect == RunUpgradeEffect::ClusterPayload ||
        effect == RunUpgradeEffect::GunPiercing || effect == RunUpgradeEffect::GunShrapnel ||
        effect == RunUpgradeEffect::NightlyBomb;
}

// Power is stored in common-card units. Round the actual count, not those
// units: +2 explosions at 1.5x must grant +3 explosions, not +4.
[[nodiscard]] constexpr int runUpgradeCountPerPower(RunUpgradeEffect effect) {
    if (effect == RunUpgradeEffect::ClusterPayload) return 2;
    if (effect == RunUpgradeEffect::GunShrapnel) return 3;
    return 1;
}

[[nodiscard]] inline int runUpgradeDiscreteAmount(RunUpgradeEffect effect, double power) {
    return static_cast<int>(std::round(runUpgradeCountPerPower(effect) * power));
}

[[nodiscard]] inline double runUpgradePowerGain(RunUpgradeEffect effect, RunUpgradeRarity rarity) {
    if (fixedRunUpgradePower(effect)) return 1.0;
    const double multiplier = runUpgradeRarityMultiplier(rarity);
    return discreteRunUpgradePower(effect)
        ? static_cast<double>(runUpgradeDiscreteAmount(effect, multiplier)) /
            runUpgradeCountPerPower(effect)
        : multiplier;
}

[[nodiscard]] constexpr RunUpgradeRarity fixedRunUpgradeRarity(RunUpgradeEffect effect) {
    if (runUpgradeBuilding(effect) || runUpgradeWeapon(effect)) return RunUpgradeRarity::Common;
    if (runUpgradeCannonStyle(effect)) return RunUpgradeRarity::Rare;
    return effect == RunUpgradeEffect::RerollToken ? RunUpgradeRarity::Rare : RunUpgradeRarity::Epic;
}

[[nodiscard]] constexpr int runUpgradeMaximumLevel(
    RunUpgradeEffect effect) {
    if (runDefenseCard(effect)) return MaxBuildingLevel;
    if (runUpgradeBuilding(effect) || runUpgradeWeapon(effect) || runUpgradeCannonStyle(effect)) return 1;
    switch (effect) {
    case RunUpgradeEffect::WiderChoice:
        return 2;
    case RunUpgradeEffect::Salvager:
    case RunUpgradeEffect::TwinBatteries:
    case RunUpgradeEffect::DoubleDown:
    case RunUpgradeEffect::RiskyInvestment:
        return 3;
    case RunUpgradeEffect::ClusterPayload:
    case RunUpgradeEffect::GunShrapnel:
        return 2;
    case RunUpgradeEffect::GunPiercing:
        return 3;
    case RunUpgradeEffect::LockChoice:
        return 1;
    default:
        return 5;
    }
}

[[nodiscard]] constexpr double runUpgradePowerLimit(RunUpgradeEffect effect) {
    if (effect == RunUpgradeEffect::PoisonChance) return 10.0;
    if (effect == RunUpgradeEffect::Salvager) return 3.0;
    const double maximum = static_cast<double>(runUpgradeMaximumLevel(effect));
    return discreteRunUpgradePower(effect) || fixedRunUpgradePower(effect)
        ? maximum : maximum * runUpgradeRarityMultiplier(RunUpgradeRarity::Legendary);
}

// Both the card preview and the simulation apply this remaining-cap check.
[[nodiscard]] inline double runUpgradeAppliedPowerGain(
    RunUpgradeEffect effect, RunUpgradeRarity rarity, double currentPower) {
    return std::min(runUpgradePowerGain(effect, rarity),
        std::max(0.0, runUpgradePowerLimit(effect) - currentPower));
}

} // namespace ian
