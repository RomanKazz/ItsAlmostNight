#include "game/QuestGameplayTracker.hpp"

#include "core/SaturatingArithmetic.hpp"
#include "game/GameEvent.hpp"
#include "game/Simulation.hpp"
#include "progression/MetaProgression.hpp"

#include <algorithm>
#include <iterator>

namespace ian {
namespace {

std::string_view classId(PlayerClass type) {
    switch (type) {
    case PlayerClass::Lumberjack: return "lumberjack";
    case PlayerClass::Sharpshooter: return "sharpshooter";
    case PlayerClass::Mechanic: return "engineer"; // Stable quest role ID across roster replacement.
    case PlayerClass::Miner: return "miner";
    case PlayerClass::Stump: return "stump";
    case PlayerClass::Vanguard: return "vanguard";
    case PlayerClass::Ranger: return "ranger";
    case PlayerClass::Engineer: return "engineer";
    case PlayerClass::Prospector: return "prospector";
    case PlayerClass::Berserker: return "berserker";
    case PlayerClass::Vampire: return "vampire";
    case PlayerClass::Alchemist: return "alchemist";
    case PlayerClass::Chronomancer: return "chronomancer";
    case PlayerClass::None: return "none";
    }
    return "none";
}

std::string_view buildingId(BuildingType type) {
    switch (type) {
    case BuildingType::Core: return "core";
    case BuildingType::Wall: return "wall";
    case BuildingType::Turret: return "turret";
    case BuildingType::CrystalMine: return "crystal_mine";
    case BuildingType::Cannon: return "cannon";
    case BuildingType::SlowTrap: return "slow_trap";
    case BuildingType::Gate: return "gate";
    case BuildingType::LumberMill: return "lumber_mill";
    case BuildingType::Quarry: return "quarry";
    case BuildingType::SpikeTrap: return "spike_trap";
    case BuildingType::WoodStorage: return "wood_storage";
    case BuildingType::StoneStorage: return "stone_storage";
    case BuildingType::CrystalStorage: return "crystal_storage";
    case BuildingType::GunTurret: return "gun_turret";
    case BuildingType::Catapult: return "catapult";
    case BuildingType::TeslaTower: return "tesla_tower";
    case BuildingType::LaserTower: return "laser";
    case BuildingType::Mortar: return "mortar";
    case BuildingType::DecoyBeacon: return "decoy_beacon";
    case BuildingType::WindBlower: return "wind_blower";
    case BuildingType::RepairStation: return "repair_station";
    case BuildingType::MissileLauncher: return "missile_launcher";
    case BuildingType::Flamethrower: return "flamethrower";
    }
    return {};
}

std::string_view enemyId(EnemyType type) {
    switch (type) {
    case EnemyType::Basic: return "basic";
    case EnemyType::Fast: return "fast";
    case EnemyType::Heavy: return "heavy";
    case EnemyType::Boss: return "boss";
    case EnemyType::Ranged: return "ranged";
    case EnemyType::Sapper: return "sapper";
    case EnemyType::AirTank: return "air_tank";
    case EnemyType::Bee: return "bee";
    case EnemyType::Flying: return "flying";
    case EnemyType::Splitter: return "splitter";
    case EnemyType::Splitling: return "splitling";
    }
    return {};
}

constexpr std::array<std::string_view, RunUpgradeEffectCount> UpgradeIds{{
    "damage", "attack_speed", "move_speed", "maximum_health", "recoverable_armor", "building_health", "build_radius", "defense_damage", "defense_fire_rate", "production_speed", "nightly_bomb", "wider_choice", "blood_harvest", "overkill", "bounces", "double_down", "lock_choice", "reroll_token", "risky_investment", "salvager", "projectile_count", "cluster_payload", "reactive_traps", "pierce", "gun_shrapnel", "gathering_speed", "longer_day", "rich_resources", "luck", "unlock_gun_turret", "unlock_turret", "unlock_cannon", "unlock_catapult", "unlock_spike_trap", "unlock_slow_trap", "cannon_heavy_shell", "cannon_scattershot", "cannon_cluster_shell", "unlock_lumber_mill", "unlock_quarry", "unlock_crystal_mine", "lumber_mill_production", "quarry_production", "crystal_mine_production", "regeneration", "unlock_club", "unlock_rifle", "unlock_bomb", "unlock_ice_wand", "unlock_fire_wand", "projectile_speed", "attack_duration", "attack_size", "critical_chance", "critical_damage", "attack_knockback", "poison_chance", "upgrade_axe", "upgrade_pickaxe", "unlock_flamethrower", "unlock_wall", "unlock_missile_launcher", "unlock_tesla_tower", "unlock_laser_tower", "unlock_mortar", "unlock_repair_station", "unlock_decoy_beacon", "unlock_wind_blower", "building_capacity", "extra_building", "upgrade_core", "experience_gain"
}};

} // namespace

void QuestGameplayTracker::observeStats(const SimulationSnapshot& snapshot) {
    const auto currentClass = classId(snapshot.playerClass);
    const auto observe = [&](std::size_t index, QuestMetric metric, double value,
                             std::string_view subject = {}) {
        if (observedStats_[index] == value) return;
        observedStats_[index] = value;
        if (value > 0.0) signals_.push_back({metric, value, subject, currentClass});
    };
    observe(0, QuestMetric::PlayerLevel, snapshot.playerLevel);
    observe(1, QuestMetric::MaximumHealth, snapshot.playerMaxHealth);
    observe(2, QuestMetric::DefenseDamage, snapshot.upgradeStats.defenseDamage);
    for (std::size_t index = 0; index < RunUpgradeEffectCount; ++index) {
        observe(index + 3, QuestMetric::UpgradeLevel,
                snapshot.runUpgradeStacks[index], UpgradeIds[index]);
    }
}

QuestUpdate QuestGameplayTracker::record(
    const QuestSystem& system, MetaProgression& progression,
    std::span<const GameEvent> events, const SimulationSnapshot& snapshot) {
    if (snapshot.sandboxMode || snapshot.unlimitedResources) return {};
    if (!progression.quests.initialized) system.initialize(progression);
    signals_.clear();
    QuestUpdate result;
    const auto flush = [&] {
        auto update = system.record(progression, signals_);
        result.changed = result.changed || update.changed;
        result.completed.insert(result.completed.end(),
            std::make_move_iterator(update.completed.begin()),
            std::make_move_iterator(update.completed.end()));
        signals_.clear();
    };
    const auto currentClass = classId(snapshot.playerClass);
    for (const auto& event : events) {
        if (event.type == GameEventType::RunStarted ||
            event.type == GameEventType::RunRestarted) {
            flush();
            system.beginRun(progression);
            observedStats_.fill(-1.0);
            progression.runsPlayed = saturatingAdd(progression.runsPlayed, 1);
            signals_.push_back({QuestMetric::RunsPlayed, 1.0, {}, currentClass});
            result.changed = true;
            continue;
        }
        if (!progression.quests.runActive) continue;
        if (event.type == GameEventType::MetaSilverCollected && event.amount > 0) {
            progression.silver = saturatingAdd(progression.silver,event.amount);
            result.changed = true;
            continue;
        }
        if (event.type == GameEventType::RunEnded) {
            observeStats(snapshot);
            flush();
            system.endRun(progression);
            result.changed = true;
            continue;
        }
        const auto subject = event.buildingType ? buildingId(*event.buildingType) : std::string_view{};
        const auto signal = [&](QuestMetric metric, double amount = 1.0,
                                std::string_view filter = {}) {
            if (amount > 0.0) signals_.push_back({metric, amount, filter, currentClass});
        };
        switch (event.type) {
        case GameEventType::EnemyKilled:
            progression.enemiesDefeated = saturatingAdd(progression.enemiesDefeated, 1);
            signal(QuestMetric::EnemiesDefeated, 1.0, subject.empty() ? "player" : subject);
            if (event.enemyType) signal(QuestMetric::EnemyTypeDefeated, 1.0, enemyId(*event.enemyType));
            break;
        case GameEventType::BuildingPlaced:
            signal(QuestMetric::BuildingsPlaced, 1.0, subject);
            if (event.buildingType && std::ranges::find(RunEconomyBuildingTypes, *event.buildingType) != RunEconomyBuildingTypes.end())
                signal(QuestMetric::ProducersBuilt);
            break;
        case GameEventType::ModularBuildingPlaced:
            signal(QuestMetric::BuildingsPlaced, 1.0, "modular");
            break;
        case GameEventType::BuildingUpgraded:
            signal(QuestMetric::BuildingsUpgraded, 1.0, subject);
            break;
        case GameEventType::BuildingRepaired:
        case GameEventType::ModularBuildingRepaired:
            signal(QuestMetric::StructuresRepaired, 1.0, subject);
            break;
        case GameEventType::ChestOpened:
            signal(QuestMetric::ChestsOpened);
            break;
        case GameEventType::LootCollected:
            progression.lootCollected = saturatingAdd(progression.lootCollected, 1);
            signal(QuestMetric::ItemsCollected);
            break;
        case GameEventType::CoinCollected:
            signal(QuestMetric::CoinsCollected, event.amount);
            break;
        case GameEventType::ResourceCollected:
            if (event.resourceType && event.entityId)
                signal(QuestMetric::NodesDepleted, 1.0, *event.resourceType == ResourceType::Wood ? "wood" :
                    *event.resourceType == ResourceType::Stone ? "stone" : "crystal");
            break;
        case GameEventType::ResourceGranted:
            progression.resourcesGathered = saturatingAdd(
                progression.resourcesGathered, std::max(0, event.amount));
            signal(QuestMetric::ResourcesGathered, event.amount);
            break;
        case GameEventType::WaveStarted:
        case GameEventType::WaveCompleted:
            progression.bestWave = std::max(progression.bestWave, event.amount);
            signal(QuestMetric::WaveReached, event.amount);
            if (event.type == GameEventType::WaveCompleted) {
                signal(QuestMetric::WavesCompleted);
                signal(QuestMetric::DawnReserve, snapshot.crystals, "crystal");
            }
            break;
        case GameEventType::WorldLandmarkActivated:
            if (event.entityId) {
                const auto landmark = std::ranges::find(snapshot.worldLandmarks, *event.entityId, &WorldLandmarkInstance::id);
                if (landmark != snapshot.worldLandmarks.end() && landmarkShrineKind(landmark->type) != ShrineKind::None)
                    signal(QuestMetric::ShrinesActivated);
            }
            break;
        case GameEventType::TowerFired:
        case GameEventType::CannonFired:
            signal(QuestMetric::DefenseShots);
            break;
        case GameEventType::PlayerDamaged:
            signal(QuestMetric::DamageTaken, std::max(1, event.amount));
            break;
        case GameEventType::CoreDamaged:
            signal(QuestMetric::CoreDamageTaken, std::max(1, event.amount));
            break;
        case GameEventType::BuildingDestroyed:
        case GameEventType::ModularBuildingDestroyed:
            signal(QuestMetric::StructuresLost, 1.0,
                event.buildingType && runCombatBuilding(*event.buildingType) && *event.buildingType != BuildingType::Wall ? "defense" : subject);
            break;
        case GameEventType::PlayerDied:
            signal(QuestMetric::PlayerDeaths);
            break;
        case GameEventType::StageCleared:
            progression.stageClears = saturatingAdd(progression.stageClears, 1);
            signal(QuestMetric::StageClears);
            break;
        case GameEventType::ChallengeCompleted:
            signal(QuestMetric::ChallengesCompleted);
            break;
        default: break;
        }
    }
    if (progression.quests.runActive) observeStats(snapshot);
    flush();
    return result;
}

} // namespace ian
