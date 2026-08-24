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
};

inline constexpr std::size_t RunUpgradeEffectCount = 20U;
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
        {RunUpgradeEffect::Damage, "SHARPENED EDGE",
         "+10% damage from all player attacks."},
        {RunUpgradeEffect::AttackSpeed, "QUICK HANDS",
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
         "+1 upgrade card offered after future nights."},
        {RunUpgradeEffect::BloodHarvest, "BLOOD HARVEST",
         "Every 10 kills restore 8 health per level."},
        {RunUpgradeEffect::Overkill, "OVERKILL",
         "Player kills transfer part of the killing blow to a nearby enemy."},
        {RunUpgradeEffect::Ricochet, "RICOCHET",
         "Turret and crossbow shots bounce to one nearby enemy for reduced damage."},
        {RunUpgradeEffect::DoubleDown, "DOUBLE DOWN",
         "Gain no power now, but choose one extra upgrade next night."},
        {RunUpgradeEffect::LockChoice, "LOCK CHOICE",
         "Unlock RMB to preserve one card through rerolls and future nights."},
        {RunUpgradeEffect::RerollToken, "REROLL TOKEN",
         "Gain one free reroll for night upgrade offers."},
        {RunUpgradeEffect::RiskyInvestment, "RISKY INVESTMENT",
         "Next night: enemies gain +25% health and +15% damage; choose one extra upgrade."},
        {RunUpgradeEffect::Salvager, "SALVAGER",
         "Enemy-destroyed buildings refund 30% of their cost per level, up to 90%."},
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
