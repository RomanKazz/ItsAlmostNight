#pragma once

#include "game/LootChestSystem.hpp"
#include "progression/MetaProgression.hpp"
#include "progression/RunUpgrade.hpp"

namespace ian {

struct CardUnlockDefinition {
    RunUpgradeEffect effect;
    std::string_view id;
    bool starter{};
};

// Stable meta IDs are distinct from levels acquired during an individual run.
inline constexpr std::array CardUnlockDefinitions{
    CardUnlockDefinition{RunUpgradeEffect::UnlockGunTurret, "building.gun_turret", true},
    CardUnlockDefinition{RunUpgradeEffect::UnlockWall, "building.wall", true},
    CardUnlockDefinition{RunUpgradeEffect::UnlockLumberMill, "building.lumber_mill", true},
    CardUnlockDefinition{RunUpgradeEffect::UnlockQuarry, "building.quarry", true},
    CardUnlockDefinition{RunUpgradeEffect::UnlockCrystalMine, "building.crystal_mine", true},
    CardUnlockDefinition{RunUpgradeEffect::UnlockTurret, "building.turret", true},
    CardUnlockDefinition{RunUpgradeEffect::UnlockCannon, "building.cannon", true},
    CardUnlockDefinition{RunUpgradeEffect::UnlockCatapult, "building.catapult", true},
    CardUnlockDefinition{RunUpgradeEffect::UnlockSpikeTrap, "building.spike_trap", true},
    CardUnlockDefinition{RunUpgradeEffect::UnlockSlowTrap, "building.slow_trap", true},
    CardUnlockDefinition{RunUpgradeEffect::UnlockFlamethrower, "building.flamethrower", true},
    CardUnlockDefinition{RunUpgradeEffect::UnlockMissileLauncher, "building.missile_launcher"},
    CardUnlockDefinition{RunUpgradeEffect::UnlockTeslaTower, "building.tesla_tower"},
    CardUnlockDefinition{RunUpgradeEffect::UnlockLaserTower, "building.laser"},
    CardUnlockDefinition{RunUpgradeEffect::UnlockMortar, "building.mortar", true},
    CardUnlockDefinition{RunUpgradeEffect::UnlockRepairStation, "building.repair_station", true},
    CardUnlockDefinition{RunUpgradeEffect::UnlockDecoyBeacon, "building.decoy_beacon", true},
    CardUnlockDefinition{RunUpgradeEffect::UnlockWindBlower, "building.wind_blower"},
    CardUnlockDefinition{RunUpgradeEffect::UnlockClub, "weapon.club", true},
    CardUnlockDefinition{RunUpgradeEffect::UnlockRifle, "weapon.rifle", true},
    CardUnlockDefinition{RunUpgradeEffect::UnlockBomb, "weapon.bomb", true},
    CardUnlockDefinition{RunUpgradeEffect::UnlockIceWand, "weapon.ice_wand"},
    CardUnlockDefinition{RunUpgradeEffect::UnlockFireWand, "weapon.fire_wand"},
    CardUnlockDefinition{RunUpgradeEffect::Damage, "tome.damage", true},
    CardUnlockDefinition{RunUpgradeEffect::AttackSpeed, "tome.attack_speed", true},
    CardUnlockDefinition{RunUpgradeEffect::MoveSpeed, "tome.move_speed", true},
    CardUnlockDefinition{RunUpgradeEffect::GatheringSpeed, "tome.gathering_speed", true},
    CardUnlockDefinition{RunUpgradeEffect::BuildingHealth, "tome.building_health", true},
    CardUnlockDefinition{RunUpgradeEffect::ProductionSpeed, "tome.production_speed", true},
    CardUnlockDefinition{RunUpgradeEffect::ResourceCapacity, "tome.resource_capacity", true},
    CardUnlockDefinition{RunUpgradeEffect::Luck, "tome.luck", true},
    CardUnlockDefinition{RunUpgradeEffect::ProjectileCount, "tome.projectile_count", true},
    CardUnlockDefinition{RunUpgradeEffect::ProjectileSpeed, "tome.projectile_speed", true},
    CardUnlockDefinition{RunUpgradeEffect::AttackDuration, "tome.attack_duration"},
    CardUnlockDefinition{RunUpgradeEffect::AttackSize, "tome.attack_size", true},
    CardUnlockDefinition{RunUpgradeEffect::CriticalChance, "tome.critical_chance", true},
    CardUnlockDefinition{RunUpgradeEffect::AttackKnockback, "tome.attack_knockback", true},
    CardUnlockDefinition{RunUpgradeEffect::ExperienceGain, "tome.experience_gain", true},
};

struct ItemUnlockDefinition {
    LootUpgradeEffect effect;
    std::string_view id;
    bool starter{};
};
inline constexpr std::array ItemUnlockDefinitions{
    ItemUnlockDefinition{LootUpgradeEffect::Acorn,"item.acorn", true},
    ItemUnlockDefinition{LootUpgradeEffect::CrystalPan,"item.crystal_pan", true},
    ItemUnlockDefinition{LootUpgradeEffect::RepairNail,"item.repair_nail", true},
    ItemUnlockDefinition{LootUpgradeEffect::LumberBoot,"item.lumber_boot", true},
    ItemUnlockDefinition{LootUpgradeEffect::Abacus,"item.abacus", true},
    ItemUnlockDefinition{LootUpgradeEffect::WeaponToken,"item.weapon_token", true},
    ItemUnlockDefinition{LootUpgradeEffect::IdleSpring,"item.idle_spring", true},
    ItemUnlockDefinition{LootUpgradeEffect::RepairMagnet,"item.repair_magnet", true},
    ItemUnlockDefinition{LootUpgradeEffect::Siren,"item.siren", true},
    ItemUnlockDefinition{LootUpgradeEffect::Clover,"item.clover", true},
    ItemUnlockDefinition{LootUpgradeEffect::MiningHammer,"item.mining_hammer", true},
    ItemUnlockDefinition{LootUpgradeEffect::DawnCoin,"item.dawn_coin", true},
    ItemUnlockDefinition{LootUpgradeEffect::GhostCandle,"item.ghost_candle"},
    ItemUnlockDefinition{LootUpgradeEffect::CoreFuse,"item.core_fuse"},
    ItemUnlockDefinition{LootUpgradeEffect::CrystalSafe,"item.crystal_safe", true},
    ItemUnlockDefinition{LootUpgradeEffect::RebuildBlueprint,"item.rebuild_blueprint", true},
    ItemUnlockDefinition{LootUpgradeEffect::LuckyDie,"item.lucky_die"},
    ItemUnlockDefinition{LootUpgradeEffect::UpgradeMirror,"item.upgrade_mirror"},
};

[[nodiscard]] inline std::string_view cardUnlockId(RunUpgradeEffect effect) {
    for (const auto& d : CardUnlockDefinitions) if (d.effect == effect) return d.id;
    return {};
}
[[nodiscard]] inline std::string_view itemUnlockId(LootUpgradeEffect effect) {
    for (const auto& d : ItemUnlockDefinitions) if (d.effect == effect) return d.id;
    return {};
}

// Starter access is a catalog rule, including for profiles saved before a
// pool expansion. It never spends silver or masquerades as a paid purchase.
[[nodiscard]] inline bool isStarterContentUnlock(std::string_view id) {
    for (const auto& definition : CardUnlockDefinitions)
        if (definition.id == id) return definition.starter;
    for (const auto& definition : ItemUnlockDefinitions)
        if (definition.id == id) return definition.starter;
    return false;
}

struct RunContentAccess {
    // A standalone simulation and sandbox have unrestricted content. The app
    // explicitly snapshots the profile before starting or resuming a real run.
    std::array<bool, RunUpgradeEffectCount> cards = [] {
        std::array<bool, RunUpgradeEffectCount> a{}; a.fill(true); return a;
    }();
    std::array<bool, LootUpgradeEffectCount> items = [] {
        std::array<bool, LootUpgradeEffectCount> a{}; a.fill(true); return a;
    }();
};

[[nodiscard]] inline RunContentAccess runContentAccess(const MetaProgression& profile) {
    RunContentAccess access;
    for (const auto& d : CardUnlockDefinitions)
        access.cards[runUpgradeIndex(d.effect)] = d.starter || profile.quests.ownedUnlocks.contains(d.id);
    for (const auto& d : ItemUnlockDefinitions)
        access.items[lootUpgradeIndex(d.effect)] = d.starter || profile.quests.ownedUnlocks.contains(d.id);
    return access;
}

[[nodiscard]] inline std::string_view contentUnlockName(std::string_view id) {
    for (const auto& d : CardUnlockDefinitions)
        if (d.id == id) return RunUpgradeDefinitions[runUpgradeIndex(d.effect)].name;
    for (const auto& d : ItemUnlockDefinitions)
        if (d.id == id) return lootUpgradeName(d.effect);
    return {};
}

} // namespace ian
