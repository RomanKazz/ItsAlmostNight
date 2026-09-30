#include "TestHarness.hpp"
#include "game/GameEvent.hpp"
#include "game/QuestGameplayTracker.hpp"
#include "game/Simulation.hpp"
#include "progression/MetaProgression.hpp"
#include "progression/QuestSystem.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace {

using namespace ian;

QuestDefinition goal(std::string id, QuestMetric metric, double target,
                     QuestScope scope = QuestScope::Lifetime, int silver = 1) {
    return {.id = id, .title = id, .description = id,
        .category = QuestCategory::General, .conditions = {{metric, scope, target, QuestComparison::AtLeast, {}, {}}},
        .reward = {silver, {}, 0}};
}

QuestUpdate send(const QuestSystem& system, MetaProgression& profile,
                 std::initializer_list<QuestSignal> signals) {
    return system.record(profile, {signals.begin(), signals.size()});
}

bool completed(const MetaProgression& profile, std::string_view id) {
    return profile.quests.progress.at(std::string(id)).completed;
}

void lifetimeAndRunProgress() {
    QuestSystem system({goal("lifetime", QuestMetric::EnemiesDefeated, 5),
                        goal("run", QuestMetric::EnemiesDefeated, 4, QuestScope::Run, 2)});
    MetaProgression profile;
    system.beginRun(profile);
    static_cast<void>(send(system, profile, {{QuestMetric::EnemiesDefeated, 3}}));
    system.endRun(profile);
    system.beginRun(profile);
    auto update = send(system, profile, {{QuestMetric::EnemiesDefeated, 2}});
    require(completed(profile, "lifetime") && !completed(profile, "run"),
            "lifetime progress persists, run goals cannot combine failed runs");
    require(profile.silver == 1 && update.completed.size() == 1,
            "completion awards exactly one reward");
    static_cast<void>(send(system, profile, {{QuestMetric::EnemiesDefeated, 2}}));
    require(completed(profile, "run") && profile.silver == 3, "run goal awards when achieved in one run");
    system.endRun(profile);
    system.beginRun(profile);
    require(completed(profile, "run"), "completed run goals remain permanent");
    require(send(system, profile, {{QuestMetric::EnemiesDefeated, 999}}).completed.empty() &&
            profile.silver == 3, "completed quest cannot pay twice");
}

void filtersAndMaximums() {
    auto defense = goal("gun", QuestMetric::EnemiesDefeated, 2);
    defense.conditions[0].subject = "gun_turret";
    defense.conditions[0].playerClass = "engineer";
    QuestSystem system({defense, goal("level", QuestMetric::PlayerLevel, 4, QuestScope::Run)});
    MetaProgression profile;
    system.beginRun(profile);
    static_cast<void>(send(system, profile, {
        {QuestMetric::EnemiesDefeated, 20, "player", "engineer"},
        {QuestMetric::EnemiesDefeated, 20, "gun_turret", "ranger"},
        {QuestMetric::EnemiesDefeated, 1, "gun_turret", "engineer"},
        {QuestMetric::PlayerLevel, 3}, {QuestMetric::PlayerLevel, 3},
    }));
    require(!completed(profile, "gun") && !completed(profile, "level"),
            "subject and class filters apply; level observations use maximum rather than sum");
    static_cast<void>(send(system, profile, {
        {QuestMetric::EnemiesDefeated, 1, "gun_turret", "engineer"}, {QuestMetric::PlayerLevel, 4},
    }));
    require(completed(profile, "gun") && completed(profile, "level"), "matching signals complete goals");
}

void combinedConditionsAndDependencies() {
    auto restricted = goal("no_chests", QuestMetric::StageClears, 1, QuestScope::Run, 4);
    restricted.conditions.push_back({QuestMetric::ChestsOpened, QuestScope::Run, 0,
                                    QuestComparison::AtMost, {}, {}});
    auto chain = goal("chain", QuestMetric::EnemiesDefeated, 2);
    chain.prerequisites = {"no_chests"};
    auto mastery = goal("mastery", QuestMetric::QuestsCompleted, 2);
    QuestSystem system({mastery, chain, restricted});
    MetaProgression profile;
    system.beginRun(profile);
    static_cast<void>(send(system, profile, {
        {QuestMetric::StageClears}, {QuestMetric::ChestsOpened}, {QuestMetric::EnemiesDefeated, 2},
    }));
    require(!completed(profile, "no_chests") && !completed(profile, "chain"),
            "all conditions in batch are checked before awards and prerequisites gate completion");
    system.beginRun(profile);
    const auto update = send(system, profile, {{QuestMetric::StageClears}});
    require(update.completed.size() == 3 && completed(profile, "mastery") && profile.silver == 6,
            "prerequisites and quest-count milestones resolve cascading completions once");
    require(system.categoryProgress(profile, QuestCategory::General).completed == 3,
            "category counters derive from actual completed definitions");
}

void purchasesAreSeparate() {
    auto unlock = goal("unlock", QuestMetric::EnemiesDefeated, 2);
    unlock.reward = {1, "meta.test", 5};
    auto buyer = goal("buyer", QuestMetric::UnlocksPurchased, 1, QuestScope::Lifetime, 2);
    QuestSystem system({unlock, buyer});
    MetaProgression profile;
    system.initialize(profile);
    require(system.purchaseUnlock(profile, "") == QuestPurchaseResult::UnknownUnlock,
            "empty unlock cannot purchase a silver-only reward");
    require(system.purchaseUnlock(profile, "meta.test") == QuestPurchaseResult::RequirementsNotMet,
            "locked purchase rejects before quest completion");
    static_cast<void>(send(system, profile, {{QuestMetric::EnemiesDefeated, 2}}));
    require(!system.ownsUnlock(profile, "meta.test") && system.statuses(profile)[1].completed,
            "quest completion grants eligibility rather than buying an unlock");
    require(system.purchaseUnlock(profile, "meta.test") == QuestPurchaseResult::InsufficientSilver &&
            profile.silver == 1, "insufficient purchase leaves balance unchanged");
    profile.silver = 10;
    require(system.purchaseUnlock(profile, "meta.test") == QuestPurchaseResult::Purchased &&
            profile.silver == 7 && completed(profile, "buyer"),
            "purchase deducts cost, grants ownership and resolves purchase-count quest");
    require(system.purchaseUnlock(profile, "meta.test") == QuestPurchaseResult::AlreadyOwned &&
            profile.silver == 7, "repeated purchase cannot deduct or award again");
}

void migrationKeepsExistingUnlocks() {
    MetaProgression profile;
    profile.silver = 137;
    profile.bestWave = 6;
    profile.enemiesDefeated = 250;
    profile.lootCollected = 20;
    profile.stageClears = 1;
    QuestSystem system;
    system.initialize(profile);
    require(profile.silver == 137 && completed(profile, "class_vampire"),
            "legacy counters migrate without retroactively paying silver");
    require(isPlayerClassUnlocked(PlayerClass::Berserker, profile) &&
            isPlayerClassUnlocked(PlayerClass::Vampire, profile) &&
            isPlayerClassUnlocked(PlayerClass::Alchemist, profile) &&
            isPlayerClassUnlocked(PlayerClass::Chronomancer, profile),
            "migration preserves every existing class unlock");
    const auto previous = profile;
    system.initialize(profile);
    require(profile == previous && system.refresh(profile).completed.empty(),
            "initialization and migration are idempotent");
    MetaProgression newcomer;
    system.initialize(newcomer);
    system.beginRun(newcomer);
    static_cast<void>(send(system, newcomer, {{QuestMetric::WaveReached, 6}}));
    require(newcomer.silver == 2 && isPlayerClassUnlocked(PlayerClass::Berserker, newcomer),
            "newly achieved class quest pays silver and unlocks class through registry");
}

void validationAndBounds() {
    auto definition = goal("a", QuestMetric::EnemiesDefeated, 2);
    bool rejected = false;
    try { const QuestSystem invalid({definition, definition}); }
    catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "duplicate quest IDs are rejected");
    auto second = goal("b", QuestMetric::EnemiesDefeated, 3);
    definition.prerequisites = {"b"};
    second.prerequisites = {"a"};
    rejected = false;
    try { const QuestSystem invalid({definition, second}); }
    catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "prerequisite cycles are rejected");
    definition.prerequisites.clear();
    definition.conditions[0].target = std::numeric_limits<double>::quiet_NaN();
    rejected = false;
    try { const QuestSystem invalid({definition}); }
    catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "invalid numeric targets are rejected");
    QuestSystem system({goal("bounded", QuestMetric::EnemiesDefeated, 2)});
    MetaProgression profile;
    system.initialize(profile);
    require(!send(system, profile, {{QuestMetric::EnemiesDefeated, -1},
        {QuestMetric::EnemiesDefeated, std::numeric_limits<double>::infinity()},
        {QuestMetric::Count, 999}}).changed, "invalid signals do not alter progress");
    profile.silver = std::numeric_limits<int>::max();
    static_cast<void>(send(system, profile, {{QuestMetric::EnemiesDefeated, 1e100}}));
    require(profile.silver == std::numeric_limits<int>::max() &&
            profile.quests.progress.at("bounded").values[0] == 2,
            "large values saturate progress and currency without overflow");
}

void profileRoundTripAndCatalog() {
    const auto directory = std::filesystem::temp_directory_path() /
        ("ian_quest_tests_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto path = directory / "profile.json";
    QuestSystem system;
    MetaProgression profile;
    system.beginRun(profile);
    static_cast<void>(send(system, profile, {{QuestMetric::EnemiesDefeated, 7},
        {QuestMetric::PlayerLevel, 5}, {QuestMetric::WaveReached, 6}}));
    profile.quests.ownedUnlocks.insert("future.unlock");
    profile.quests.purchasedUnlocks.insert("future.unlock");
    static_cast<void>(system.refresh(profile));
    require(saveMetaProgression(path.string(), profile), "quest profile saves");
    MetaProgression loaded;
    require(loadMetaProgression(path.string(), loaded) && loaded == profile,
            "active run progress, completed flags, balance and ownership round trip together");
    require(system.refresh(loaded).completed.empty() && loaded.silver == profile.silver,
            "reload cannot replay a completion reward");
    system.resumeRun(loaded);
    require(loaded == profile, "resuming preserves recorded run progress and ownership");
    loaded.silver = 88;
    require(saveMetaProgression(path.string(), loaded), "atomic save replaces an existing profile");
    MetaProgression replaced;
    require(loadMetaProgression(path.string(), replaced) && replaced.silver == 88,
            "replacement is readable and complete");
    std::filesystem::rename(path, path.string() + ".bak");
    require(loadMetaProgression(path.string(), replaced) && replaced == loaded,
            "interrupted Windows replacement recovers backup");
    std::filesystem::remove(path.string() + ".bak");
    { std::ofstream file(path); file << R"({"version":3,"quests":{"version":1}})"; }
    const auto unchanged = replaced;
    require(!loadMetaProgression(path.string(), replaced) && replaced == unchanged,
            "malformed save does not partially overwrite an existing profile");
    { std::ofstream file(path); file << R"({"version":2,"silver":9,"bestWave":6,"enemiesDefeated":250})"; }
    require(loadMetaProgression(path.string(), replaced), "version 2 remains readable");
    system.initialize(replaced);
    require(replaced.silver == 9 && isPlayerClassUnlocked(PlayerClass::Vampire, replaced),
            "old persisted data migrates without losing currency or class access");
    auto catalog = loadQuestDefinitions(std::filesystem::path(IAN_SOURCE_DIR) / "assets/data/meta_quests.json");
    const auto defaults = system.definitions();
    require(catalog.size() == defaults.size() && catalog.size() == 36, "data catalog loads all 36 quests");
    require(catalog == std::vector<QuestDefinition>(defaults.begin(), defaults.end()),
            "fallback catalog matches all shipped conditions, prerequisites and rewards");
    std::filesystem::remove_all(directory);
}

void legacyResumeAndTrapAttribution() {
    auto restricted = goal("restricted", QuestMetric::StageClears, 1, QuestScope::Run);
    restricted.conditions.push_back({QuestMetric::ChestsOpened, QuestScope::Run, 0,
                                    QuestComparison::AtMost, {}, {}});
    QuestSystem system({restricted, goal("kills", QuestMetric::EnemiesDefeated, 1)});
    MetaProgression profile;
    system.resumeRun(profile);
    static_cast<void>(send(system, profile, {{QuestMetric::StageClears}, {QuestMetric::EnemiesDefeated}}));
    require(!completed(profile, "restricted") && completed(profile, "kills"),
            "old suspended run can progress but cannot claim unrecorded absence of chest openings");
    system.beginRun(profile);
    static_cast<void>(send(system, profile, {{QuestMetric::StageClears}}));
    require(completed(profile, "restricted"), "new run clears unknown history restriction");

    const std::vector<BuildingInstance> buildings{{
        .id = {11, 0}, .type = BuildingType::SlowTrap, .gridPosition = {0, 0},
    }};
    EnemySystem enemies;
    const std::array<Vec3, 1> spawn{{{0, 0.8, 0}}};
    enemies.spawnWave(spawn);
    TrapSystem traps;
    traps.setModifiers(1, 1, 1, 1, 100);
    traps.syncBuildings(buildings);
    static_cast<void>(traps.tick(1.0 / 60.0, buildings, enemies));
    require(!traps.hits().empty() && traps.hits().front().result.killed &&
            traps.hits().front().type == BuildingType::SlowTrap,
            "trap payload preserves actual type even for reactive slow-trap kills");
}

void gameplayAdapter() {
    auto gun = goal("gun", QuestMetric::EnemiesDefeated, 2);
    gun.conditions[0].subject = "gun_turret";
    auto engineer = goal("repair", QuestMetric::StructuresRepaired, 1, QuestScope::Run);
    engineer.conditions[0].playerClass = "engineer";
    auto upgrade = goal("upgrade", QuestMetric::UpgradeLevel, 2, QuestScope::Run);
    upgrade.conditions[0].subject = "defense_damage";
    QuestSystem system({gun, engineer, upgrade,
        goal("challenge", QuestMetric::ChallengesCompleted, 1),
        goal("resources", QuestMetric::ResourcesGathered, 5),
        goal("coins", QuestMetric::CoinsCollected, 5)});
    QuestGameplayTracker tracker;
    MetaProgression profile;
    SimulationSnapshot snapshot{};
    snapshot.playerClass = PlayerClass::Engineer;
    snapshot.playerMaxHealth = 100;
    snapshot.playerLevel = 1;
    snapshot.sandboxMode = true;
    const std::array events{
        GameEvent{.type = GameEventType::RunStarted},
        GameEvent{.type = GameEventType::EnemyKilled, .buildingType = BuildingType::GunTurret},
        GameEvent{.type = GameEventType::ResourceGranted, .amount = 5},
        GameEvent{.type = GameEventType::CoinCollected, .amount = 5},
    };
    require(!tracker.record(system, profile, events, snapshot).changed && profile == MetaProgression{},
            "sandbox cannot initialize quests, earn currency or alter lifetime statistics");
    snapshot.sandboxMode = false;
    static_cast<void>(tracker.record(system, profile, events, snapshot));
    require(profile.runsPlayed == 1 && profile.enemiesDefeated == 1 && profile.resourcesGathered == 5 &&
            completed(profile, "resources") && completed(profile, "coins") && !completed(profile, "gun"),
            "authoritative events advance quests and existing counters once");
    snapshot.runUpgradeStacks[runUpgradeIndex(RunUpgradeEffect::DefenseDamage)] = 2;
    const std::array next{
        GameEvent{.type = GameEventType::ProjectileHit, .buildingType = BuildingType::GunTurret},
        GameEvent{.type = GameEventType::EnemyKilled},
        GameEvent{.type = GameEventType::BuildingRepaired},
        GameEvent{.type = GameEventType::ChallengeCompleted},
    };
    static_cast<void>(tracker.record(system, profile, next, snapshot));
    require(!completed(profile, "gun") && completed(profile, "repair") && completed(profile, "upgrade") &&
            completed(profile, "challenge"), "player kills and visual hits do not count as turret kills; stats and challenges count");
    const std::array finish{
        GameEvent{.type = GameEventType::EnemyKilled, .buildingType = BuildingType::GunTurret},
        GameEvent{.type = GameEventType::RunEnded},
    };
    static_cast<void>(tracker.record(system, profile, finish, snapshot));
    require(completed(profile, "gun") && !profile.quests.runActive, "last kill is recorded before ending run");
    require(!tracker.record(system, profile, {}, snapshot).changed,
            "unchanged frames produce no quest updates");
}

} // namespace

void runQuestSystemTests() {
    lifetimeAndRunProgress();
    filtersAndMaximums();
    combinedConditionsAndDependencies();
    purchasesAreSeparate();
    migrationKeepsExistingUnlocks();
    validationAndBounds();
    profileRoundTripAndCatalog();
    gameplayAdapter();
    legacyResumeAndTrapAttribution();
}
