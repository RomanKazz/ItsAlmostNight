#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string_view>

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
};

inline constexpr std::size_t RunUpgradeEffectCount = 29U;
inline constexpr std::size_t MinimumRunUpgradeChoices = 3U;
inline constexpr std::size_t MaximumRunUpgradeChoices = 5U;

struct RunUpgradeDefinition {
    RunUpgradeEffect effect;
    std::string_view name;
    std::string_view description;
};

inline constexpr std::array<RunUpgradeDefinition,
                            RunUpgradeEffectCount>
    RunUpgradeDefinitions{{
        {RunUpgradeEffect::Damage, "POWER",
         "+10% attack damage"},
        {RunUpgradeEffect::AttackSpeed, "HASTE",
         "+15% weapon and tool speed"},
        {RunUpgradeEffect::MoveSpeed, "MOVE",
         "+7% move speed"},
        {RunUpgradeEffect::MaximumHealth, "HP",
         "+12 max HP"},
        {RunUpgradeEffect::RecoverableArmor, "DEF",
         "+8 armor"},
        {RunUpgradeEffect::BuildingHealth, "FORTIFY",
         "+12% building HP"},
        {RunUpgradeEffect::BuildRadius, "BUILD AREA",
         "+3m build range"},
        {RunUpgradeEffect::DefenseDamage, "TOWER POWER",
         "+12% defense damage"},
        {RunUpgradeEffect::DefenseFireRate, "RAPID FIRE",
         "+10% defense speed"},
        {RunUpgradeEffect::ProductionSpeed, "WORKSHOP",
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
        {RunUpgradeEffect::RichResources, "RICH NODES",
         "+15% resource node yield"},
        {RunUpgradeEffect::Luck, "LUCK",
         "+10% upgrade luck"},
    }};

[[nodiscard]] constexpr std::size_t runUpgradeIndex(
    RunUpgradeEffect effect) {
    return static_cast<std::size_t>(effect);
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
    constexpr std::array<double, RunUpgradeRarityCount> Values{1.0, 1.2, 1.4, 1.6, 2.0};
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
    return effect == RunUpgradeEffect::DoubleDown ||
        effect == RunUpgradeEffect::RiskyInvestment ||
        effect == RunUpgradeEffect::RerollToken;
}

[[nodiscard]] constexpr bool discreteRunUpgradePower(RunUpgradeEffect effect) {
    return effect == RunUpgradeEffect::Overkill || effect == RunUpgradeEffect::Ricochet ||
        effect == RunUpgradeEffect::TwinBatteries || effect == RunUpgradeEffect::ClusterPayload ||
        effect == RunUpgradeEffect::GunPiercing || effect == RunUpgradeEffect::GunShrapnel ||
        effect == RunUpgradeEffect::NightlyBomb;
}

[[nodiscard]] inline double runUpgradePowerGain(RunUpgradeEffect effect, RunUpgradeRarity rarity) {
    if (fixedRunUpgradePower(effect)) return 1.0;
    const double multiplier = runUpgradeRarityMultiplier(rarity);
    return discreteRunUpgradePower(effect) ? std::round(multiplier) : multiplier;
}

[[nodiscard]] constexpr RunUpgradeRarity fixedRunUpgradeRarity(RunUpgradeEffect effect) {
    return effect == RunUpgradeEffect::RerollToken ? RunUpgradeRarity::Rare : RunUpgradeRarity::Epic;
}

[[nodiscard]] constexpr int runUpgradeMaximumLevel(
    RunUpgradeEffect effect) {
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
        ? maximum : maximum * 2.0;
}

} // namespace ian
