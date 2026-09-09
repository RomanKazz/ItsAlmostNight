#pragma once

#include <array>
#include <cstddef>
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
};

inline constexpr std::size_t RunUpgradeEffectCount = 23U;
inline constexpr std::size_t MinimumRunUpgradeChoices = 3U;
inline constexpr std::size_t MaximumRunUpgradeChoices = 5U;

// Unified progression offers use a compact, save-friendly integer id. Values
// below SkillProgressionCardOffset retain the legacy run-upgrade encoding;
// skill definitions occupy the separate upper range.
using ProgressionCardId = int;
inline constexpr ProgressionCardId SkillProgressionCardOffset = 1000;

[[nodiscard]] constexpr ProgressionCardId progressionCardId(
    RunUpgradeEffect effect) {
    return static_cast<ProgressionCardId>(effect);
}

[[nodiscard]] constexpr ProgressionCardId skillProgressionCardId(
    std::size_t skillIndex) {
    return SkillProgressionCardOffset +
        static_cast<ProgressionCardId>(skillIndex);
}

[[nodiscard]] constexpr bool isSkillProgressionCard(
    ProgressionCardId card) {
    return card >= SkillProgressionCardOffset;
}

[[nodiscard]] constexpr std::size_t progressionCardSkillIndex(
    ProgressionCardId card) {
    return static_cast<std::size_t>(
        card - SkillProgressionCardOffset);
}

[[nodiscard]] constexpr RunUpgradeEffect progressionCardRunUpgrade(
    ProgressionCardId card) {
    return static_cast<RunUpgradeEffect>(card);
}

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
         "+8% attack speed"},
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
         "+30% building refund"},
        {RunUpgradeEffect::TwinBatteries, "TWIN SHOT",
         "+1 side shot"},
        {RunUpgradeEffect::ClusterPayload, "CLUSTER",
         "+2 mini explosions"},
        {RunUpgradeEffect::ReactiveTraps, "PULSE",
         "+6 shockwave damage"},
    }};

[[nodiscard]] constexpr std::size_t runUpgradeIndex(
    RunUpgradeEffect effect) {
    return static_cast<std::size_t>(effect);
}

[[nodiscard]] constexpr const RunUpgradeDefinition&
runUpgradeDefinition(RunUpgradeEffect effect) {
    return RunUpgradeDefinitions[runUpgradeIndex(effect)];
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
        return 2;
    case RunUpgradeEffect::LockChoice:
        return 1;
    default:
        return 5;
    }
}

} // namespace ian
