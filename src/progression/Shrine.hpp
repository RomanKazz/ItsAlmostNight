#pragma once

#include "core/Types.hpp"
#include "economy/ResourceCost.hpp"
#include "progression/RunUpgrade.hpp"
#include "progression/DefenseAttackModifiers.hpp"

#include <optional>

namespace ian {

enum class ShrineKind : std::uint8_t { None, Player, Buildings };
inline constexpr int ShrineCountPerType = 12;
inline constexpr int MaximumShrineActivations = ShrineCountPerType * 2;
struct ShrineRunState {
    int activations{};
    std::array<int, RunUpgradeEffectCount> playerBonuses{};
    std::array<int, RunUpgradeEffectCount> buildingBonuses{};
    ShrineKind offer{ShrineKind::None};
    std::optional<EntityId> source;
};
struct ShrineBlessing {
    ShrineKind kind;
    RunUpgradeEffect effect;
    RunUpgradeEffect icon;
    std::string_view name;
    std::string_view description;
    double increment;
};
// Offer metadata comes from the active tome catalog. Damage and speed use
// only the half appropriate to the shrine; no blueprint/unlock/retired cards.
[[nodiscard]] constexpr ShrineBlessing tomeBlessing(ShrineKind kind, RunUpgradeEffect effect,
    double increment, RunUpgradeEffect icon, std::string_view description = {}) {
    return {kind, effect, icon, runTomeDefinition(icon)->name,
        description.empty() ? RunUpgradeDefinitions[static_cast<std::size_t>(icon)].description : description, increment};
}
[[nodiscard]] constexpr ShrineBlessing tomeBlessing(ShrineKind kind, RunUpgradeEffect effect, double increment) {
    return tomeBlessing(kind, effect, increment, effect);
}
inline constexpr std::array ShrineBlessings{
    tomeBlessing(ShrineKind::Player, RunUpgradeEffect::Damage, .10, RunUpgradeEffect::Damage, "+10% player damage"),
    tomeBlessing(ShrineKind::Player, RunUpgradeEffect::AttackSpeed, .15, RunUpgradeEffect::AttackSpeed, "+15% weapon attack speed"),
    tomeBlessing(ShrineKind::Player, RunUpgradeEffect::MoveSpeed, .07),
    tomeBlessing(ShrineKind::Player, RunUpgradeEffect::MaximumHealth, 12),
    tomeBlessing(ShrineKind::Player, RunUpgradeEffect::RecoverableArmor, 8),
    tomeBlessing(ShrineKind::Player, RunUpgradeEffect::Luck, .10),
    tomeBlessing(ShrineKind::Player, RunUpgradeEffect::Regeneration, .5),
    tomeBlessing(ShrineKind::Player, RunUpgradeEffect::BloodHarvest, 8),
    tomeBlessing(ShrineKind::Player, RunUpgradeEffect::GatheringSpeed, .15),
    tomeBlessing(ShrineKind::Player, RunUpgradeEffect::ResourceCapacity, .20),
    tomeBlessing(ShrineKind::Player, RunUpgradeEffect::PoisonChance, .10),
    tomeBlessing(ShrineKind::Buildings, RunUpgradeEffect::DefenseDamage, .12, RunUpgradeEffect::Damage, "+12% defense damage"),
    tomeBlessing(ShrineKind::Buildings, RunUpgradeEffect::DefenseFireRate, .10, RunUpgradeEffect::AttackSpeed, "+10% defense speed"),
    tomeBlessing(ShrineKind::Buildings, RunUpgradeEffect::BuildingHealth, .12),
    tomeBlessing(ShrineKind::Buildings, RunUpgradeEffect::BuildingCapacity, 1),
    tomeBlessing(ShrineKind::Buildings, RunUpgradeEffect::ProductionSpeed, .15),
    tomeBlessing(ShrineKind::Buildings, RunUpgradeEffect::ProjectileCount, 1),
    tomeBlessing(ShrineKind::Buildings, RunUpgradeEffect::Bounces, 1),
    tomeBlessing(ShrineKind::Buildings, RunUpgradeEffect::Pierce, 1),
    tomeBlessing(ShrineKind::Buildings, RunUpgradeEffect::ProjectileSpeed, .20),
    tomeBlessing(ShrineKind::Buildings, RunUpgradeEffect::AttackDuration, .20),
    tomeBlessing(ShrineKind::Buildings, RunUpgradeEffect::AttackSize, .15),
    tomeBlessing(ShrineKind::Buildings, RunUpgradeEffect::CriticalChance, .05),
    tomeBlessing(ShrineKind::Buildings, RunUpgradeEffect::CriticalDamage, .25),
    tomeBlessing(ShrineKind::Buildings, RunUpgradeEffect::AttackKnockback, .20),
    tomeBlessing(ShrineKind::Buildings, RunUpgradeEffect::PoisonChance, .10),
};
[[nodiscard]] constexpr const ShrineBlessing* shrineBlessing(ShrineKind kind, RunUpgradeEffect effect) {
    for (const auto& blessing : ShrineBlessings)
        if (blessing.kind == kind && blessing.effect == effect) return &blessing;
    return nullptr;
}
[[nodiscard]] inline const auto& shrineBonusCounts(const ShrineRunState& state, ShrineKind kind) {
    return kind == ShrineKind::Buildings ? state.buildingBonuses : state.playerBonuses;
}
[[nodiscard]] inline auto& shrineBonusCounts(ShrineRunState& state, ShrineKind kind) {
    return kind == ShrineKind::Buildings ? state.buildingBonuses : state.playerBonuses;
}
[[nodiscard]] inline int shrineBonusCount(const ShrineRunState& state, ShrineKind kind, RunUpgradeEffect effect) {
    return shrineBlessing(kind,effect) ? shrineBonusCounts(state,kind)[runUpgradeIndex(effect)] : 0;
}
[[nodiscard]] inline double shrineMultiplier(const ShrineRunState& state, RunUpgradeEffect effect) {
    for (const auto& blessing : ShrineBlessings)
        if (blessing.effect == effect) return 1.0 + blessing.increment * shrineBonusCount(state,blessing.kind,effect);
    return 1.0;
}
// Keep the shared AttackStats binding/hit flow. Flat counters add, independent
// percentage sources multiply. Tome ownership/power is never changed.
[[nodiscard]] inline AttackModifiers shrineDefenseAttackModifiers(
    const std::array<double, RunUpgradeEffectCount>& powers, const ShrineRunState& state) {
    auto result=defenseAttackCardModifiers(powers);
    std::array<double,RunUpgradeEffectCount> shrinePowers{};
    for (std::size_t i=0;i<shrinePowers.size();++i) shrinePowers[i]=state.buildingBonuses[i];
    const auto extra=defenseAttackCardModifiers(shrinePowers);
    for (std::size_t i=0;i<result.values.size();++i) {
        result.values[i].flat+=extra.values[i].flat;
        result.values[i].multiplier*=extra.values[i].multiplier;
    }
    return result;
}
[[nodiscard]] constexpr ResourceCost shrineActivationCost(ShrineKind kind, int activations) {
    const int spent = std::clamp(activations, 0, MaximumShrineActivations);
    const ResourceCost base = kind == ShrineKind::Buildings ? ResourceCost{25,15,5} : ResourceCost{20,12,4};
    return {base.wood + spent*8, base.stone + spent*5, base.crystals + spent*2};
}
[[nodiscard]] inline bool validShrineRunState(const ShrineRunState& state) {
    if (state.activations < 0 || state.activations > MaximumShrineActivations ||
        static_cast<unsigned>(state.offer) > static_cast<unsigned>(ShrineKind::Buildings) ||
        (state.offer == ShrineKind::None) != !state.source.has_value()) return false;
    int total = 0;
    for (const auto kind : {ShrineKind::Player,ShrineKind::Buildings}) {
        const auto& counts=shrineBonusCounts(state,kind);
        int kindTotal=state.offer==kind ? 1 : 0;
        for (std::size_t i=0; i<counts.size(); ++i) {
            const int count=counts[i];
            if (count<0 || count>MaximumShrineActivations ||
                (count && !shrineBlessing(kind,static_cast<RunUpgradeEffect>(i)))) return false;
            total += count;
            kindTotal += count;
        }
        if (kindTotal>ShrineCountPerType) return false;
    }
    return total + (state.offer != ShrineKind::None ? 1 : 0) == state.activations;
}
} // namespace ian
