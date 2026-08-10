#include "game/Simulation.hpp"

#include <algorithm>
#include <cmath>

namespace ian {
namespace {

constexpr std::uint64_t InsightHashSeed = 0x9e3779b97f4a7c15ULL;

std::uint64_t insightEntityEvent(std::uint64_t tag, EntityId id) {
    std::uint64_t value = (static_cast<std::uint64_t>(id.generation) << 32U) |
                          static_cast<std::uint64_t>(id.index);
    value ^= tag + InsightHashSeed + (value << 6U) + (value >> 2U);
    return value == 0 ? tag + 1 : value;
}

std::uint64_t insightIntegerEvent(std::uint64_t tag, std::uint64_t value) {
    value ^= tag + InsightHashSeed + (value << 6U) + (value >> 2U);
    return value == 0 ? tag + 1 : value;
}

std::uint64_t insightStringEvent(std::uint64_t tag, std::string_view text, int cycle) {
    std::uint64_t value = tag ^ static_cast<std::uint64_t>(std::max(0, cycle));
    for (const char character : text)
        value = (value ^ static_cast<unsigned char>(character)) * 1099511628211ULL;
    return value == 0 ? tag + 1 : value;
}

} // namespace

const SkillTree& Simulation::skillTree() const { return skillTree_; }
const InsightSystem& Simulation::insightSystem() const { return insight_; }
const ObjectiveSystem& Simulation::objectiveSystem() const { return objectives_; }

std::uint64_t Simulation::structuralRevision() const {
    return structuralRevision_;
}

SkillPurchaseError Simulation::purchaseSkill(std::size_t index) {
    invalidateSnapshotCache();
    const SkillPurchaseError result = skillTree_.purchase(
        index, !unlimitedResources_);
    if (result != SkillPurchaseError::None) return result;
    const SkillEffect effect = skillTree_.nodes()[index].effect;
    switch (effect) {
    case SkillEffect::UnlockAxe: playerWeapons_.selectWeapon(PlayerWeapon::Axe); break;
    case SkillEffect::UnlockPickaxe: playerWeapons_.selectWeapon(PlayerWeapon::Pickaxe); break;
    case SkillEffect::UnlockClub: playerWeapons_.selectWeapon(PlayerWeapon::Club); break;
    case SkillEffect::UnlockIceWand: playerWeapons_.selectWeapon(PlayerWeapon::IceWand); break;
    case SkillEffect::UnlockFireWand: playerWeapons_.selectWeapon(PlayerWeapon::FireWand); break;
    case SkillEffect::UnlockHammer: playerWeapons_.selectWeapon(PlayerWeapon::Hammer); break;
    case SkillEffect::UnlockRifle: playerWeapons_.selectWeapon(PlayerWeapon::Rifle); break;
    case SkillEffect::AutoSwitchTools: break;
    case SkillEffect::HoldToGather: break;
    case SkillEffect::NightlyChest: break;
    case SkillEffect::PowerSwing: break;
    case SkillEffect::SafeDelivery: break;
    case SkillEffect::FieldRepairs: break;
    case SkillEffect::UnlockBombs: break;
    case SkillEffect::LightFootwork: break;
    case SkillEffect::Dash: break;
    case SkillEffect::BareHands: break;
    }
    selectedBuilding_.reset();
    buildingPreview_.reset();
    events_.push_back({.type = GameEventType::SkillUnlocked,
                       .amount = static_cast<int>(index)});
    return result;
}

void Simulation::grantSkillPoints(int amount, SkillPointSource source) {
    if (amount <= 0) return;
    invalidateSnapshotCache();
    skillTree_.grantPoints(amount);
    events_.push_back({.type = GameEventType::SkillPointsGranted,
                       .amount = amount,
                       .intensity = static_cast<double>(source)});
}

SkillTreeRunState Simulation::saveSkillTreeState() const { return skillTree_.saveState(); }

bool Simulation::loadSkillTreeState(const SkillTreeRunState& state) {
    if (!skillTree_.loadState(state)) return false;
    invalidateSnapshotCache();
    playerWeapons_.selectWeapon(PlayerWeapon::BareHands);
    return true;
}

ProgressionRunState Simulation::saveProgressionState() const {
    return {.skillTree = skillTree_.saveState(), .insight = insight_.saveState(),
            .objectives = objectives_.saveState()};
}

bool Simulation::loadProgressionState(const ProgressionRunState& state) {
    SkillTree loadedTree = skillTree_;
    InsightSystem loadedInsight = insight_;
    ObjectiveSystem loadedObjectives = objectives_;
    if (!loadedTree.loadState(state.skillTree) ||
        !loadedInsight.loadState(state.insight) ||
        !loadedObjectives.loadState(state.objectives)) return false;
    skillTree_ = std::move(loadedTree);
    insight_ = std::move(loadedInsight);
    objectives_ = std::move(loadedObjectives);
    insightRewardedEnemyIds_.clear();
    invalidateSnapshotCache();
    playerWeapons_.selectWeapon(PlayerWeapon::BareHands);
    return true;
}

void Simulation::grantConfiguredInsight(
    double amount, InsightSource source, InsightCategory category,
    const InsightGrantContext& context) {
    const InsightGrantResult result = insight_.grantInsight(
        amount, source, category, context);
    if (!result.accepted) return;
    invalidateSnapshotCache();
    if (result.treePointsGranted > 0) {
        grantSkillPoints(result.treePointsGranted,
            source == InsightSource::BossKilled ? SkillPointSource::Boss
                                                : SkillPointSource::Event);
    }
    events_.push_back({
        .type = GameEventType::InsightGranted,
        .amount = static_cast<int>(std::lround(result.finalAmount)),
        .intensity = result.finalAmount,
        .insightSource = source,
        .insightAmount = result.finalAmount,
        .insightBefore = result.insightBefore,
        .insightAfter = result.insightAfter,
        .insightRequirement = result.requirement,
        .insightDiminishingMultiplier = result.diminishingMultiplier,
        .treePointsGranted = result.treePointsGranted,
    });
}

void Simulation::processInsightEvent(const GameEvent& event) {
    const InsightConfig& config = insight_.config();
    switch (event.type) {
    case GameEventType::IntroSkillObjectiveCompleted:
        grantConfiguredInsight(config.introGatherObjective, InsightSource::Objective,
            InsightCategory::Exploration,
            {.eventId = insightIntegerEvent(0x100U, 1U), .oneTime = true,
             .bypassDiminishing = true});
        break;
    case GameEventType::WaveCompleted: {
        const bool boss = event.critical;
        const bool milestone = !boss && config.milestoneWaveInterval > 0 &&
            event.amount % config.milestoneWaveInterval == 0;
        grantConfiguredInsight(boss ? config.bossWave
                                      : milestone ? config.milestoneWave : config.normalWave,
            InsightSource::WaveCompleted, InsightCategory::Combat,
            {.eventId = insightIntegerEvent(0x200U,
                 static_cast<std::uint64_t>(std::max(0, event.amount))),
             .oneTime = true, .bypassDiminishing = boss || milestone});
        insight_.beginNewDiminishingCycle();
        break;
    }
    case GameEventType::EnemyKilled:
        if (event.entityId) {
            const auto found = std::ranges::find_if(enemies_.enemies(),
                [&event](const EnemyInstance& enemy) { return enemy.id == *event.entityId; });
            const EnemyType type = found != enemies_.enemies().end()
                ? found->type : EnemyType::Basic;
            const bool boss = type == EnemyType::Boss;
            grantConfiguredInsight(config.enemy[static_cast<std::size_t>(type)],
                boss ? InsightSource::BossKilled : InsightSource::EnemyKilled,
                InsightCategory::Combat,
                {.eventId = insightEntityEvent(0x300U, *event.entityId),
                 .oneTime = true, .bypassDiminishing = boss});
        }
        break;
    case GameEventType::ResourceHit:
    case GameEventType::ResourceCollected:
        if (!unlimitedResources_ && event.resourceType && event.amount > 0) {
            const auto type = static_cast<std::size_t>(*event.resourceType);
            grantConfiguredInsight(config.resourcePerUnit[type] * event.amount,
                InsightSource::ResourceGathered, InsightCategory::Gathering, {});
            if (event.type == GameEventType::ResourceCollected && event.entityId) {
                grantConfiguredInsight(config.resourceDepleted[type],
                    InsightSource::ResourceDepleted, InsightCategory::Gathering,
                    {.eventId = insightEntityEvent(0x400U, *event.entityId),
                     .oneTime = true});
            }
        }
        break;
    case GameEventType::BuildingPlaced:
        if (!unlimitedResources_ && event.entityId && event.buildingType) {
            const auto type = static_cast<std::size_t>(*event.buildingType);
            grantConfiguredInsight(config.building[type], InsightSource::StructureBuilt,
                InsightCategory::Building,
                {.eventId = insightEntityEvent(0x500U, *event.entityId), .oneTime = true});
            grantConfiguredInsight(config.firstBuildingTypeBonus,
                InsightSource::StructureBuilt, InsightCategory::Building,
                {.eventId = insightIntegerEvent(0x501U, type + 1U), .oneTime = true});
            if (*event.buildingType == BuildingType::Core) {
                grantConfiguredInsight(config.introCoreObjective, InsightSource::Objective,
                    InsightCategory::Exploration,
                    {.eventId = insightIntegerEvent(0x502U, 1U), .oneTime = true,
                     .bypassDiminishing = true});
            }
        }
        break;
    case GameEventType::ModularBuildingPlaced:
        if (!unlimitedResources_ && event.entityId && event.amount >= 0 && event.amount < 4) {
            grantConfiguredInsight(config.modularBuilding[static_cast<std::size_t>(event.amount)],
                InsightSource::StructureBuilt, InsightCategory::Building,
                {.eventId = insightEntityEvent(0x510U, *event.entityId), .oneTime = true});
        }
        break;
    case GameEventType::BuildingRepaired:
    case GameEventType::ModularBuildingRepaired:
        if (!unlimitedResources_ && event.amount > 0)
            grantConfiguredInsight(config.repairPerHealth * event.amount,
                InsightSource::StructureRepaired, InsightCategory::Repair, {});
        break;
    case GameEventType::ChestOpened:
        if (!unlimitedResources_ && event.entityId) {
            const auto chest = std::ranges::find_if(lootChests_.chests(),
                [&event](const LootChestInstance& value) { return value.id == *event.entityId; });
            const std::size_t type = chest != lootChests_.chests().end()
                ? static_cast<std::size_t>(chest->type) : 0U;
            grantConfiguredInsight(config.chest[type], InsightSource::ChestOpened,
                InsightCategory::Exploration,
                {.eventId = insightEntityEvent(0x600U, *event.entityId),
                 .oneTime = true, .bypassDiminishing = true});
        }
        break;
    case GameEventType::ObjectiveCompleted:
        if (event.objectiveId && event.intensity > 0.0) {
            grantConfiguredInsight(
                event.intensity, InsightSource::Objective,
                InsightCategory::Exploration,
                {.eventId = insightStringEvent(0x700U, *event.objectiveId, event.amount),
                 .oneTime = true, .bypassDiminishing = true});
        }
        break;
    default:
        break;
    }
}

void Simulation::processObjectiveEvents(std::size_t firstEvent) {
    if (unlimitedResources_) return;
    const std::size_t lastGameplayEvent = events_.size();
    const bool nightNow = state_ == RunState::Sunset || state_ == RunState::Wave;
    const auto core = buildings_.core();
    const Vec3 corePosition = core ? buildingWorldPosition(*core) : Vec3{};
    const auto emitCompletions = [this](std::vector<ObjectiveCompletion> completions) {
        for (const auto& completion : completions) {
            events_.push_back({
                .type = GameEventType::ObjectiveCompleted,
                .position = playerPosition_,
                .amount = completion.cycle,
                .intensity = completion.insightReward,
                .objectiveId = completion.id,
            });
        }
    };
    for (std::size_t index = firstEvent; index < lastGameplayEvent; ++index) {
        const GameEvent& event = events_[index];
        if ((event.type == GameEventType::ResourceHit ||
             event.type == GameEventType::ResourceCollected) &&
            event.resourceType) {
            const double distance = core
                ? std::hypot(event.position.x - corePosition.x,
                             event.position.z - corePosition.z) : 0.0;
            emitCompletions(objectives_.onResourceEvent({
                .wood = *event.resourceType == ResourceType::Wood,
                .amount = std::max(0, event.amount),
                .depleted = event.type == GameEventType::ResourceCollected,
                .largeDeposit = event.largeDeposit,
                .bareHands = event.bareHands,
                .night = event.night || nightNow,
                .hasCore = core.has_value(),
                .distanceFromCore = distance,
                .elapsedSeconds = elapsedSeconds_,
            }));
        } else if (event.type == GameEventType::GoldProduced) {
            emitCompletions(objectives_.onCrystalsGathered(
                std::max(0, event.amount), elapsedSeconds_, event.night || nightNow));
        } else if (event.type == GameEventType::ResourceGatherMissed) {
            objectives_.onGatheringMiss();
        } else if (event.type == GameEventType::WaveCompleted) {
            static_cast<void>(objectives_.beginNewDay());
        }
    }
}

void Simulation::processInsightEvents(
    std::size_t firstEvent, bool suppressEnemyRewards) {
    const std::size_t lastGameplayEvent = events_.size();
    for (std::size_t index = firstEvent; index < lastGameplayEvent; ++index) {
        const GameEvent event = events_[index];
        if (event.type == GameEventType::EnemyKilled && event.entityId) {
            const std::uint64_t id = insightEntityEvent(0x300U, *event.entityId);
            insightRewardedEnemyIds_.insert(id);
            if (suppressEnemyRewards) {
                insight_.markEventConsumed(id);
                continue;
            }
        }
        processInsightEvent(event);
    }
    for (const EnemyInstance& enemy : enemies_.enemies()) {
        if (enemy.active || enemy.state != EnemyState::Dead) continue;
        const std::uint64_t id = insightEntityEvent(0x300U, enemy.id);
        if (!insightRewardedEnemyIds_.insert(id).second) continue;
        if (suppressEnemyRewards) {
            insight_.markEventConsumed(id);
            continue;
        }
        processInsightEvent({.type = GameEventType::EnemyKilled,
                             .entityId = enemy.id, .position = enemy.position});
    }
}

void Simulation::cycleUnlockedTool() {
    std::vector<PlayerWeapon> tools{PlayerWeapon::BareHands};
    if (unlimitedResources_ || skillTree_.hasEffect(SkillEffect::UnlockAxe))
        tools.push_back(PlayerWeapon::Axe);
    if (unlimitedResources_ || skillTree_.hasEffect(SkillEffect::UnlockPickaxe))
        tools.push_back(PlayerWeapon::Pickaxe);
    if (unlimitedResources_ || skillTree_.hasEffect(SkillEffect::UnlockClub))
        tools.push_back(PlayerWeapon::Club);
    if (unlimitedResources_ || skillTree_.hasEffect(SkillEffect::UnlockIceWand))
        tools.push_back(PlayerWeapon::IceWand);
    if (unlimitedResources_ || skillTree_.hasEffect(SkillEffect::UnlockFireWand))
        tools.push_back(PlayerWeapon::FireWand);
    if (unlimitedResources_ || skillTree_.hasEffect(SkillEffect::UnlockHammer))
        tools.push_back(PlayerWeapon::Hammer);
    if (unlimitedResources_ || skillTree_.hasEffect(SkillEffect::UnlockRifle))
        tools.push_back(PlayerWeapon::Rifle);
    const auto current = std::ranges::find(tools, playerWeapons_.selectedWeapon());
    const std::size_t next = current == tools.end()
        ? 0 : (static_cast<std::size_t>(std::distance(tools.begin(), current)) + 1) % tools.size();
    playerWeapons_.selectWeapon(tools[next]);
    selectedBuilding_.reset();
    buildingPreview_.reset();
}

} // namespace ian
