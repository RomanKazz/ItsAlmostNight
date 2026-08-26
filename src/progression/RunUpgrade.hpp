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
        {RunUpgradeEffect::Damage, "SHARPENED EDGE",
         "+10% damage from all player attacks."},
        {RunUpgradeEffect::AttackSpeed, "QUICK REFLEXES",
         "+8% melee and rifle attack speed."},
        {RunUpgradeEffect::MoveSpeed, "TRAIL RUNNER",
         "+7% movement speed."},
        {RunUpgradeEffect::MaximumHealth, "HARDY",
         "+12 maximum health and heal 12 health."},
        {RunUpgradeEffect::RecoverableArmor, "IRON SKIN",
         "+8 recoverable armor."},
        {RunUpgradeEffect::BuildingHealth, "SOLID FOUNDATIONS",
         "+12% maximum health for all structures."},
        {RunUpgradeEffect::BuildRadius, "EXPANDED PERIMETER",
         "+3 meters to the building radius around the core."},
        {RunUpgradeEffect::DefenseDamage, "CALIBRATED SIGHTS",
         "+12% damage for defensive structures."},
        {RunUpgradeEffect::DefenseFireRate, "CLOCKWORK SPRINGS",
         "+10% attack speed for defensive structures."},
        {RunUpgradeEffect::ProductionSpeed, "BUSY WORKSHOP",
         "+15% passive production speed."},
        {RunUpgradeEffect::NightlyBomb, "BOMBMAKER",
         "+1 bomb at the start of every future night."},
        {RunUpgradeEffect::WiderChoice, "WIDER CHOICE",
         "+1 upgrade card offered at future level-ups."},
        {RunUpgradeEffect::BloodHarvest, "BLOOD HARVEST",
         "Every 10 kills restore 8 health per level."},
        {RunUpgradeEffect::Overkill, "OVERKILL",
         "Player kills transfer part of the killing blow to a nearby enemy."},
        {RunUpgradeEffect::Ricochet, "RICOCHET",
         "Turret and crossbow shots bounce to one nearby enemy for reduced damage."},
        {RunUpgradeEffect::DoubleDown, "DOUBLE DOWN",
         "Gain no power now, but choose two extra upgrades next level."},
        {RunUpgradeEffect::LockChoice, "LOCK CHOICE",
         "Unlock RMB to preserve one card through rerolls and future nights."},
        {RunUpgradeEffect::RerollToken, "REROLL TOKEN",
         "Gain one free reroll for level-up offers."},
        {RunUpgradeEffect::RiskyInvestment, "RISKY INVESTMENT",
         "Next night: enemies gain +25% health and +15% damage; choose two extra upgrades."},
        {RunUpgradeEffect::Salvager, "SALVAGER",
         "Enemy-destroyed buildings refund 30% of their cost per level, up to 90%."},
        {RunUpgradeEffect::TwinBatteries, "TWIN BATTERIES",
         "Add one sideways shot to turrets and crossbows. Maximum 3; later shots deal less damage."},
        {RunUpgradeEffect::ClusterPayload, "CLUSTER PAYLOAD",
         "Cannon and catapult impacts create two 45% damage secondary explosions."},
        {RunUpgradeEffect::ReactiveTraps, "REACTIVE TRAPS",
         "Every trap activation releases a shockwave dealing 6 damage per level."},
    }};

[[nodiscard]] constexpr std::size_t runUpgradeIndex(
    RunUpgradeEffect effect) {
    return static_cast<std::size_t>(effect);
}

[[nodiscard]] constexpr const RunUpgradeDefinition&
runUpgradeDefinition(RunUpgradeEffect effect) {
    return RunUpgradeDefinitions[runUpgradeIndex(effect)];
}

} // namespace ian
