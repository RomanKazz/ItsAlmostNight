#pragma once

#include "TestHarness.hpp"
#include "game/Simulation.hpp"
#include "game/QuestGameplayTracker.hpp"
#include "progression/ContentUnlocks.hpp"
#include "progression/QuestSystem.hpp"
#include "waves/NightEncounter.hpp"

#include <memory>
#include <set>

inline void checkQuestContentProgression() {
    using namespace ian;
    QuestSystem quests;
    constexpr std::array<std::string_view, GameBalance::EnemyTypeCount> enemyIds{
        "basic", "fast", "heavy", "boss", "ranged", "sapper", "flying", "splitter", "splitling", "bee", "air_tank"};
    std::set<std::string_view> naturalEnemies;
    WaveDirector director;
    for (int night = 1; night <= 60; ++night) {
        const auto counts = waveEnemyCounts(director.composition(night));
        for (std::size_t i = 0; i < counts.size(); ++i) if (counts[i] > 0) naturalEnemies.insert(enemyIds[i]);
    }
    for (const auto& quest : quests.definitions()) for (const auto& condition : quest.conditions)
        if (condition.metric == QuestMetric::EnemyTypeDefeated && !condition.subject.empty())
            require(naturalEnemies.contains(condition.subject), "every enemy-specific quest targets a role that actually spawns in normal waves");
    MetaProgression profile;
    quests.beginRun(profile);
    MetaProgression legacy;
    legacy.quests.initialized = true;
    legacy.quests.runActive = true;
    legacy.enemiesDefeated = 200;
    legacy.silver = 17;
    legacy.quests.ownedUnlocks.insert("future.saved_unlock");
    legacy.quests.ownedUnlocks.insert("building.catapult");
    legacy.quests.purchasedUnlocks.insert("building.catapult");
    legacy.quests.progress["unlock_building_catapult"].values={1};
    quests.initialize(legacy);
    require(legacy.silver == 17 && legacy.quests.ownedUnlocks.contains("future.saved_unlock") &&
        legacy.quests.progress.at("milestone_enemies_defeated_100").completed &&
        legacy.quests.progress.at("milestone_enemies_defeated_100").rewardClaimed,
        "catalog migration preserves ownership and seeds known lifetime counters without paying silver twice");
    require(legacy.quests.progress.at("challenge_clean_night").values[1] > 0,
        "a newly introduced restricted goal cannot assume the existing run had no damage");
    require(legacy.quests.progress.at("unlock_building_catapult").values[0]==1,
        "catalog expansion preserves existing partial mastery progress before a new run");
    quests.beginRun(legacy);
    require(legacy.quests.progress.at("challenge_clean_night").values[1] == 0,
        "a new run makes migrated restricted goals attemptable again");
    const std::array openingActions{
        QuestSignal{QuestMetric::NodesDepleted, 1}, QuestSignal{QuestMetric::BuildingsPlaced, 1},
        QuestSignal{QuestMetric::EnemiesDefeated, 10}, QuestSignal{QuestMetric::ChestsOpened, 1},
        QuestSignal{QuestMetric::StructuresRepaired, 1}, QuestSignal{QuestMetric::WavesCompleted, 1}};
    require(quests.record(profile, openingActions).completed.empty(),
        "tutorial actions and the first night do not flood the player with completed quests");
    require(legacy.quests.progress.at("milestone_enemies_defeated_100").completed,
        "rebalancing uses stable quest IDs and retains already earned progress");
    for (const auto& q : quests.definitions()) require(!q.notification.empty(),
        "every authored quest supplies a short completion result");
    for(const auto& d:CardUnlockDefinitions) if(d.starter)
        require(quests.ownsUnlock(legacy,d.id),"expanded starter cards are immediately owned by existing profiles");
    for(const auto& d:ItemUnlockDefinitions) if(d.starter)
        require(quests.ownsUnlock(legacy,d.id),"expanded starter items are immediately owned by existing profiles");
    require(legacy.silver==17 && legacy.quests.purchasedUnlocks.size()==1,
        "starter expansion preserves silver, paid history and partial quest progress");
    require(quests.purchaseUnlock(profile,"building.catapult")==QuestPurchaseResult::AlreadyOwned && profile.silver==0,
        "free starter content cannot charge an unnecessary discovery purchase");
    const auto countStarters=[](std::string_view category) {
        return std::ranges::count_if(CardUnlockDefinitions,[category](const auto& d){return d.starter && d.id.starts_with(category);});
    };
    require(countStarters("building.")==13 && countStarters("tome.")==14 && countStarters("weapon.")==5 &&
        std::ranges::count_if(ItemUnlockDefinitions,[](const auto& d){return d.starter;})==14,
        "first-run pool contains thirteen buildings, fourteen tomes and fourteen items");
    const auto paid=std::ranges::count_if(quests.definitions(),[](const auto& q){return q.reward.purchaseCost>0;});
    for(const auto& q:quests.definitions()) for(const auto& c:q.conditions)
        if(c.metric==QuestMetric::UnlocksPurchased) require(c.target<=static_cast<double>(paid),
            "purchase milestones remain reachable with the smaller exotic unlock pool");
    auto veteran=legacy;
    for(const auto id:{"building.catapult","building.spike_trap","building.slow_trap"}) {
        veteran.quests.ownedUnlocks.insert(id); veteran.quests.purchasedUnlocks.insert(id);
    }
    static_cast<void>(quests.refresh(veteran));
    require(veteran.quests.progress.at("milestone_unlocks_purchased_5").completed &&
        !veteran.quests.progress.at("milestone_unlocks_purchased_5").rewardClaimed && veteran.silver==17,
        "startup refresh makes rebalanced purchase goals claimable without automatically changing the wallet");
    const auto initial = runContentAccess(profile);
    require(initial.cards[runUpgradeIndex(RunUpgradeEffect::UnlockMissileLauncher)] &&
        initial.cards[runUpgradeIndex(RunUpgradeEffect::UnlockTeslaTower)] &&
        !initial.cards[runUpgradeIndex(RunUpgradeEffect::UnlockMortar)] &&
        !initial.cards[runUpgradeIndex(RunUpgradeEffect::UnlockRepairStation)] &&
        !initial.cards[runUpgradeIndex(RunUpgradeEffect::UnlockSlowTrap)],
        "rockets and Tesla start available; mortar, repair station and slow trap require their discoveries");
    require(initial.cards[runUpgradeIndex(RunUpgradeEffect::UnlockGunTurret)] &&
        initial.cards[runUpgradeIndex(RunUpgradeEffect::UnlockTurret)] &&
        initial.cards[runUpgradeIndex(RunUpgradeEffect::UnlockCannon)],
        "gun turret, crossbow and cannon enter fresh profiles without a discovery purchase");
    require(initial.cards[runUpgradeIndex(RunUpgradeEffect::UnlockIceWand)] &&
        initial.cards[runUpgradeIndex(RunUpgradeEffect::UnlockFireWand)] &&
        quests.purchaseUnlock(profile,"weapon.ice_wand")==QuestPurchaseResult::AlreadyOwned &&
        quests.purchaseUnlock(profile,"weapon.fire_wand")==QuestPurchaseResult::AlreadyOwned,
        "both wands are free on new profiles and never charge discovery silver");
    std::set<std::string_view> ids;
    const auto validateDiscovery = [&](std::string_view id, bool starter) {
        require(ids.insert(id).second, "meta content IDs are unique");
        const auto definition = std::ranges::find_if(quests.definitions(), [id](const auto& q) {
            return q.reward.unlockId == id;
        });
        require(starter ? definition == quests.definitions().end() :
            definition != quests.definitions().end() && definition->reward.purchaseCost > 0,
            "every locked content entry has one real purchasable quest; starters need no quest");
    };
    for (const auto& d : CardUnlockDefinitions) {
        require(!retiredRunUpgrade(d.effect), "unlock catalog contains only active cards");
        validateDiscovery(d.id, d.starter);
        require(initial.cards[runUpgradeIndex(d.effect)] == d.starter, "fresh profile only has starter cards");
    }
    for (const auto& d : ItemUnlockDefinitions) {
        validateDiscovery(d.id, d.starter);
        require(initial.items[lootUpgradeIndex(d.effect)] == d.starter, "fresh profile only has starter loot");
    }
    for (const auto& tome : RunTomeDefinitions) if (!retiredRunUpgrade(tome.effect))
        require(!cardUnlockId(tome.effect).empty(), "every active tome participates in permanent progression");
    for (const auto type : RunBuildingTypes)
        require(!cardUnlockId(*buildingRunUpgrade(type)).empty(), "every run building participates in permanent progression");

    auto world = WorldConfig::defaults();
    world.terrainResolution = 33;
    world.terrainAmplitude = world.terrainSurfaceNoiseAmplitude = 0;
    world.pondMinimumCount = world.pondMaximumCount = 0;
    auto map = MapDefinition::defaults(); map.resources.clear(); map.obstacles.clear();
    auto run = std::make_unique<Simulation>(GameBalance::defaults(), map, world);
    run->configureNextRunContent(initial);
    run->startRunFromSeed(PlayerClass::Vanguard, 83621);
    for (const auto& chest : run->snapshot().lootChests)
        require(initial.items[lootUpgradeIndex(chest.loot.effect)],
            "fresh exploration chests draw from the expanded starter pool without locked exotic items");
    PlayerCommand gain; gain.grantInsight = GrantInsightCommand{1000};
    run->tick(0, gain);
    auto snapshot = run->snapshot();
    require(snapshot.runUpgradeChoicePending && snapshot.runUpgradeChoiceCount >= 3,
        "a starter profile has enough real level-up choices, including the paid fourth slot");
    for (std::size_t i = 0; i < snapshot.runUpgradeChoiceCount; ++i)
        require(initial.cards[runUpgradeIndex(snapshot.runUpgradeChoices[i])], "all free and paid offered cards respect the profile");

    auto closedOffer = run->saveSuspendedRunState();
    closedOffer.runUpgradeChoicePending = true;
    closedOffer.runUpgradeChoiceCount = 1;
    closedOffer.runUpgradeSelectionsRemaining = 1;
    closedOffer.runUpgradeChoices[0] = RunUpgradeEffect::AttackDuration;
    run->returnToMainMenu();
    require(run->loadSuspendedRunState(closedOffer), "legacy pending offer checkpoint can still be loaded");
    const auto resumed = run->snapshot();
    require(std::find(resumed.runUpgradeChoices.begin(), resumed.runUpgradeChoices.begin() +
        static_cast<std::ptrdiff_t>(resumed.runUpgradeChoiceCount), RunUpgradeEffect::AttackDuration) ==
        resumed.runUpgradeChoices.begin() + static_cast<std::ptrdiff_t>(resumed.runUpgradeChoiceCount) &&
        resumed.runUpgradeStacks[runUpgradeIndex(RunUpgradeEffect::AttackDuration)] == 0,
        "resume replaces a stale locked offer with eligible cards without granting a locked tome");
    const QuestSignal level{QuestMetric::UpgradeLevel, 3, "unlock_ice_wand"};
    static_cast<void>(quests.record(profile, std::span{&level, 1}));
    require(!runContentAccess(profile).cards[runUpgradeIndex(RunUpgradeEffect::AttackDuration)],
        "quest completion alone does not place its tome in the pool");
    require(quests.purchaseUnlock(profile, "tome.attack_duration") == QuestPurchaseResult::InsufficientSilver,
        "eligibility does not bypass the permanent wallet");
    profile.silver = 4;
    require(quests.purchaseUnlock(profile, "tome.attack_duration") == QuestPurchaseResult::Purchased && profile.silver == 0,
        "eligible tome purchase charges the actual price");
    const auto purchased = runContentAccess(profile);
    require(purchased.cards[runUpgradeIndex(RunUpgradeEffect::AttackDuration)], "bought tome enters the future run pool");
    run->configureNextRunContent(purchased);
    run->returnToMainMenu();
    require(run->loadSuspendedRunState(closedOffer) && run->selectRunUpgrade(0),
        "purchased tome can be selected in a resumed real run");

    TerrainHeightfield terrain(world); terrain.generate(12);
    LootChestSystem drops;
    drops.setAllowedLoot(initial.items);
    drops.reset(12, 150, terrain, {}, {});
    for (const auto rarity : {LootRarity::Common, LootRarity::Rare, LootRarity::Epic, LootRarity::Legendary})
        for (std::uint64_t seed = 0; seed < 20; ++seed) {
            drops.spawnLooseLoot({}, rarity, seed);
            const auto& loot=drops.chests().back().loot;
            require(initial.items[lootUpgradeIndex(loot.effect)] && loot.rarity==rarity,
                "fresh profiles have a real available item pool at every chest rarity");
        }
    std::array<bool,LootUpgradeEffectCount> candleOnly{};
    candleOnly[lootUpgradeIndex(LootUpgradeEffect::GhostCandle)]=true;
    drops.setAllowedLoot(candleOnly);
    drops.spawnLooseLoot({}, LootRarity::Epic, 99);
    require(drops.chests().back().loot.effect == LootUpgradeEffect::GhostCandle && drops.chests().back().loot.rarity == LootRarity::Epic,
        "a purchased exotic item becomes available at its intended rarity");
    run->returnToMainMenu();
    run->configureNextRunContent(initial);
    run->startSandboxRun(PlayerClass::Vanguard);
    require(run->grantDebugRunUpgradeBatch(RunUpgradeEffect::AttackDuration, 1), "sandbox retains unrestricted content for testing");
}

inline void checkQuestActionSignals() {
    using namespace ian;
    const auto goal = [](std::string id, QuestMetric metric, double target, std::string subject = "") {
        return QuestDefinition{.id = id, .title = id, .description = id,
            .conditions = {{metric, QuestScope::Lifetime, target, QuestComparison::AtLeast, subject, {}}}};
    };
    auto clean = goal("clean", QuestMetric::WavesCompleted, 1);
    clean.conditions[0].scope = QuestScope::Run;
    clean.conditions.push_back({QuestMetric::DamageTaken, QuestScope::Run, 0, QuestComparison::AtMost, {}, {}});
    QuestSystem quests({goal("nodes", QuestMetric::NodesDepleted, 2, "stone"),
        goal("producers", QuestMetric::ProducersBuilt, 1), goal("shrines", QuestMetric::ShrinesActivated, 1),
        goal("shots", QuestMetric::DefenseShots, 2), goal("bank", QuestMetric::DawnReserve, 75, "crystal"),
        goal("lost", QuestMetric::StructuresLost, 1, "defense"), goal("core", QuestMetric::CoreDamageTaken, 1), clean});
    MetaProgression profile;
    QuestGameplayTracker tracker;
    SimulationSnapshot snapshot{}; snapshot.playerClass = PlayerClass::Engineer;
    const std::array landmarks{
        WorldLandmarkInstance{.id={1,1}, .type=WorldLandmarkType::Mine},
        WorldLandmarkInstance{.id={2,1}, .type=WorldLandmarkType::BuildingShrine}};
    snapshot.worldLandmarks = landmarks;
    const std::array start{
        GameEvent{.type=GameEventType::RunStarted},
        GameEvent{.type=GameEventType::ResourceHit, .entityId=EntityId{1,1}, .resourceType=ResourceType::Stone},
        GameEvent{.type=GameEventType::ResourceGranted, .amount=100},
        GameEvent{.type=GameEventType::BuildingPlaced, .buildingType=BuildingType::LumberMill},
        GameEvent{.type=GameEventType::WorldLandmarkActivated, .entityId=EntityId{1,1}}};
    static_cast<void>(tracker.record(quests, profile, start, snapshot));
    require(profile.quests.progress.at("nodes").values[0] == 0 &&
        profile.quests.progress.at("producers").completed && !profile.quests.progress.at("shrines").completed,
        "partial harvest, resource grants and mine activation cannot impersonate node depletion or shrine use");
    snapshot.crystals = 80;
    const std::array actions{
        GameEvent{.type=GameEventType::ResourceCollected, .entityId=EntityId{1,1}, .resourceType=ResourceType::Stone},
        GameEvent{.type=GameEventType::ResourceCollected, .entityId=EntityId{2,1}, .resourceType=ResourceType::Stone},
        GameEvent{.type=GameEventType::WorldLandmarkActivated, .entityId=EntityId{2,1}},
        GameEvent{.type=GameEventType::TowerFired}, GameEvent{.type=GameEventType::CannonFired},
        GameEvent{.type=GameEventType::PlayerDamaged, .amount=7},
        GameEvent{.type=GameEventType::CoreDamaged, .amount=2},
        GameEvent{.type=GameEventType::BuildingDestroyed, .buildingType=BuildingType::GunTurret},
        GameEvent{.type=GameEventType::WaveCompleted, .amount=1}};
    static_cast<void>(tracker.record(quests, profile, actions, snapshot));
    for (const auto id : {"nodes", "shrines", "shots", "bank", "lost", "core"})
        require(profile.quests.progress.at(id).completed, "authoritative actions advance their specific quest metric");
    require(!profile.quests.progress.at("clean").completed,
        "damage events use amount, not the unset damage field, and invalidate no-damage challenges");
    const std::array restart{GameEvent{.type=GameEventType::RunRestarted}, GameEvent{.type=GameEventType::WaveCompleted, .amount=1}};
    static_cast<void>(tracker.record(quests, profile, restart, snapshot));
    require(profile.quests.progress.at("clean").completed, "a new run resets failed restrictions and allows another attempt");
    const auto before = profile;
    snapshot.unlimitedResources = true;
    require(!tracker.record(quests, profile, restart, snapshot).changed && profile == before,
        "god mode cannot advance quests or create fake run boundaries");
}
