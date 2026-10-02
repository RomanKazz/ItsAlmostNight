#pragma once

#include "buildings/BuildingSystem.hpp"
#include "combat/CannonAttack.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <optional>

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
};

inline constexpr std::size_t RunUpgradeEffectCount = 45U;
inline constexpr std::size_t MinimumRunUpgradeChoices = 3U;
inline constexpr std::size_t MaximumRunUpgradeChoices = 5U;

struct RunUpgradeDefinition {
    RunUpgradeEffect effect;
    std::string_view name;
    std::string_view description;
};

// A hammer is a persistent run stat item. Its level/power use the existing
// stable upgrade ID, so a renamed card never applies the bonus a second time.
struct RunHammerDefinition {
    RunUpgradeEffect effect;
    std::string_view name;
    std::string_view mark;
    std::uint32_t colorRgb;
};

inline constexpr std::array<RunHammerDefinition, 13> RunHammerDefinitions{{
    {RunUpgradeEffect::DefenseDamage, "MIGHT HAMMER", "DMG", 0xFFAD43},
    {RunUpgradeEffect::DefenseFireRate, "RAPID HAMMER", "RATE", 0x4DDCF2},
    {RunUpgradeEffect::BuildingHealth, "FORTITUDE HAMMER", "DEF", 0x78A7FF},
    {RunUpgradeEffect::ProductionSpeed, "INDUSTRY HAMMER", "PROD", 0x72DA92},
    {RunUpgradeEffect::Luck, "FORTUNE HAMMER", "LUCK", 0xA36CEF},
    {RunUpgradeEffect::BuildRadius, "REACH HAMMER", "AREA", 0x58C2BB},
    {RunUpgradeEffect::RichResources, "HARVEST HAMMER", "YIELD", 0xD3B061},
    {RunUpgradeEffect::MaximumHealth, "VITALITY HAMMER", "HP", 0xFF647F},
    {RunUpgradeEffect::RecoverableArmor, "GUARD HAMMER", "SHLD", 0x68DBFF},
    {RunUpgradeEffect::MoveSpeed, "SWIFT HAMMER", "MOVE", 0x73D1CC},
    {RunUpgradeEffect::AttackSpeed, "HASTE HAMMER", "SPD", 0xF69ED6},
    {RunUpgradeEffect::Damage, "POWER HAMMER", "ATK", 0xF77E56},
    {RunUpgradeEffect::Regeneration, "REGEN HAMMER", "REGEN", 0x8BDA66},
}};

[[nodiscard]] constexpr const RunHammerDefinition* runHammerDefinition(RunUpgradeEffect effect) {
    for (const auto& hammer : RunHammerDefinitions) if (hammer.effect == effect) return &hammer;
    return nullptr;
}

inline constexpr std::array<RunUpgradeDefinition,
                            RunUpgradeEffectCount>
    RunUpgradeDefinitions{{
        {RunUpgradeEffect::Damage, runHammerDefinition(RunUpgradeEffect::Damage)->name,
         "+10% attack damage"},
        {RunUpgradeEffect::AttackSpeed, runHammerDefinition(RunUpgradeEffect::AttackSpeed)->name,
         "+15% weapon and tool speed"},
        {RunUpgradeEffect::MoveSpeed, runHammerDefinition(RunUpgradeEffect::MoveSpeed)->name,
         "+7% move speed"},
        {RunUpgradeEffect::MaximumHealth, runHammerDefinition(RunUpgradeEffect::MaximumHealth)->name,
         "+12 max HP"},
        {RunUpgradeEffect::RecoverableArmor, runHammerDefinition(RunUpgradeEffect::RecoverableArmor)->name,
         "+8 armor"},
        {RunUpgradeEffect::BuildingHealth, runHammerDefinition(RunUpgradeEffect::BuildingHealth)->name,
         "+12% building HP"},
        {RunUpgradeEffect::BuildRadius, runHammerDefinition(RunUpgradeEffect::BuildRadius)->name,
         "+3m build range"},
        {RunUpgradeEffect::DefenseDamage, runHammerDefinition(RunUpgradeEffect::DefenseDamage)->name,
         "+12% defense damage"},
        {RunUpgradeEffect::DefenseFireRate, runHammerDefinition(RunUpgradeEffect::DefenseFireRate)->name,
         "+10% defense speed"},
        {RunUpgradeEffect::ProductionSpeed, runHammerDefinition(RunUpgradeEffect::ProductionSpeed)->name,
         "+15% production speed"},
        {RunUpgradeEffect::NightlyBomb, "BOMBS",
         "+1 bomb each night"},
        {RunUpgradeEffect::WiderChoice, "MORE CHOICES",
         "+1 card choice"},
        {RunUpgradeEffect::BloodHarvest, "LIFESTEAL",
         "+8 HP per 10 kills"},
        {RunUpgradeEffect::Overkill, "PIERCE",
         "+1 damage transfer"},
        {RunUpgradeEffect::Ricochet, "RICOCHET",
         "+1 shot bounce"},
        {RunUpgradeEffect::DoubleDown, "DOUBLE DOWN",
         "+2 next-level picks"},
        {RunUpgradeEffect::LockChoice, "CARD LOCK",
         "+1 lock ability"},
        {RunUpgradeEffect::RerollToken, "REROLL",
         "+1 free reroll"},
        {RunUpgradeEffect::RiskyInvestment, "HIGH STAKES",
         "+2 picks, stronger enemies"},
        {RunUpgradeEffect::Salvager, "SALVAGE",
         "+30% sold or destroyed refund"},
        {RunUpgradeEffect::TwinBatteries, "TWIN SHOT",
         "+1 side shot"},
        {RunUpgradeEffect::ClusterPayload, "CLUSTER",
         "+2 mini explosions"},
        {RunUpgradeEffect::ReactiveTraps, "PULSE",
         "+6 shockwave damage"},
        {RunUpgradeEffect::GunPiercing, "ARMOR PIERCER",
         "+1 gun turret pierce target"},
        {RunUpgradeEffect::GunShrapnel, "SHRAPNEL",
         "+3 gun turret kill shards"},
        {RunUpgradeEffect::GatheringSpeed, "QUICK TOOLS",
         "+15% gathering speed"},
        {RunUpgradeEffect::LongerDay, "LONG DAY",
         "+5s build phase"},
        {RunUpgradeEffect::RichResources, runHammerDefinition(RunUpgradeEffect::RichResources)->name,
         "+15% resource node yield"},
        {RunUpgradeEffect::Luck, runHammerDefinition(RunUpgradeEffect::Luck)->name,
         "+10% upgrade luck"},
        {RunUpgradeEffect::UnlockGunTurret, "GUN TURRET", "Unlock gun turret construction"},
        {RunUpgradeEffect::UnlockTurret, "CROSSBOW", "Unlock crossbow construction"},
        {RunUpgradeEffect::UnlockCannon, "CANNON", "Unlock cannon construction"},
        {RunUpgradeEffect::UnlockCatapult, "CATAPULT", "Unlock catapult construction"},
        {RunUpgradeEffect::UnlockSpikeTrap, "SPIKES", "Unlock spike trap construction"},
        {RunUpgradeEffect::UnlockSlowTrap, "SLOW TRAP", "Unlock slow trap construction"},
        {RunUpgradeEffect::CannonHeavyShell, "HEAVY SHELL", "Cannon: x5 damage, fires 3x slower"},
        {RunUpgradeEffect::CannonScattershot, "SCATTERSHOT", "Cannon: 5 shells at 40% damage, smaller blasts"},
        {RunUpgradeEffect::CannonClusterShell, "CLUSTER SHELL", "Cannon: 80% damage, +3 mini explosions"},
        {RunUpgradeEffect::UnlockLumberMill, "LUMBER MILL", "Unlock lumber mill construction"},
        {RunUpgradeEffect::UnlockQuarry, "QUARRY", "Unlock quarry construction"},
        {RunUpgradeEffect::UnlockCrystalMine, "CRYSTAL MINE", "Unlock crystal mine construction"},
        {RunUpgradeEffect::LumberMillProduction, "SAW POWER", "+20% lumber mill production speed"},
        {RunUpgradeEffect::QuarryProduction, "DEEP CUT", "+20% quarry production speed"},
        {RunUpgradeEffect::CrystalMineProduction, "CRYSTAL FLOW", "+20% crystal mine production speed"},
        {RunUpgradeEffect::Regeneration, runHammerDefinition(RunUpgradeEffect::Regeneration)->name, "+0.5 HP per second"},
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

[[nodiscard]] constexpr std::optional<RunUpgradeEffect> buildingRunUpgrade(BuildingType type) {
    for (std::size_t index = runUpgradeIndex(RunUpgradeEffect::UnlockGunTurret);
         index < RunUpgradeEffectCount; ++index) {
        const auto effect = static_cast<RunUpgradeEffect>(index);
        if (runUpgradeBuilding(effect) == type) return effect;
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
    return effect == RunUpgradeEffect::LockChoice ||
           effect == RunUpgradeEffect::GatheringSpeed;
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
    return runUpgradeBuilding(effect).has_value() || runUpgradeCannonStyle(effect).has_value() || effect == RunUpgradeEffect::DoubleDown ||
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
    if (runUpgradeBuilding(effect)) return RunUpgradeRarity::Common;
    if (runUpgradeCannonStyle(effect)) return RunUpgradeRarity::Rare;
    return effect == RunUpgradeEffect::RerollToken ? RunUpgradeRarity::Rare : RunUpgradeRarity::Epic;
}

[[nodiscard]] constexpr int runUpgradeMaximumLevel(
    RunUpgradeEffect effect) {
    if (runUpgradeBuilding(effect) || runUpgradeCannonStyle(effect)) return 1;
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
