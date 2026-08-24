#include "game/Simulation.hpp"

#include "core/DeterministicRandom.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace ian {
namespace {

constexpr std::uint64_t InsightHashSeed = 0x9e3779b97f4a7c15ULL;
constexpr double BattlePotionAttackSpeedMultiplier = 1.35;

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
    if (index >= skillTree_.nodes().size()) {
        return SkillPurchaseError::InvalidNode;
    }
    const auto core = buildings_.core();
    const int coreLevel = core ? static_cast<int>(core->level) : 0;
    if (!unlimitedResources_ &&
        skillTree_.state(index) == SkillNodeState::Available &&
        coreLevel < skillTree_.nodes()[index].minimumCoreLevel) {
        return SkillPurchaseError::CoreLevelRequired;
    }
    const double previousDayExtension =
        skillTree_.effectValue("day.duration_seconds");
    const SkillPurchaseError result = skillTree_.purchase(
        index, !unlimitedResources_);
    if (result != SkillPurchaseError::None) return result;
    const auto nodeHas = [&](std::string_view key) {
        return std::ranges::any_of(
            skillTree_.nodes()[index].effects,
            [key](const SkillEffectDefinition& effect) {
                return effect.key == key && effect.value != 0.0;
            });
    };
    if (nodeHas("unlock.axe")) playerWeapons_.selectWeapon(PlayerWeapon::Axe);
    else if (nodeHas("unlock.pickaxe")) playerWeapons_.selectWeapon(PlayerWeapon::Pickaxe);
    else if (nodeHas("unlock.club")) playerWeapons_.selectWeapon(PlayerWeapon::Club);
    else if (nodeHas("unlock.ice_wand")) playerWeapons_.selectWeapon(PlayerWeapon::IceWand);
    else if (nodeHas("unlock.fire_wand")) playerWeapons_.selectWeapon(PlayerWeapon::FireWand);
    else if (nodeHas("unlock.hammer")) playerWeapons_.selectWeapon(PlayerWeapon::Hammer);
    else if (nodeHas("unlock.rifle")) playerWeapons_.selectWeapon(PlayerWeapon::Rifle);
    else if (nodeHas("unlock.bombs")) playerWeapons_.selectWeapon(PlayerWeapon::Bomb);
    refreshSkillRuntimeEffects();
    const RunState effectiveState =
        state_ == RunState::Paused ? stateBeforePause_ : state_;
    if (effectiveState == RunState::BuildPhase) {
        const double addedSeconds = std::max(
            0.0,
            skillTree_.effectValue("day.duration_seconds") -
                previousDayExtension);
        phaseTimeRemaining_ += addedSeconds;
        phaseDuration_ += addedSeconds;
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

SkillTreeRunState Simulation::saveSkillTreeState() const {
    return skillTree_.saveState();
}

bool Simulation::loadSkillTreeState(const SkillTreeRunState& state) {
    if (!skillTree_.loadState(state)) return false;
    invalidateSnapshotCache();
    playerWeapons_.selectWeapon(PlayerWeapon::BareHands);
    grantPlayerClassStartingNodes();
    refreshSkillRuntimeEffects();
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
    std::fill(insightRewardedEnemyIds_.begin(),
              insightRewardedEnemyIds_.end(), EntityId{});
    invalidateSnapshotCache();
    playerWeapons_.selectWeapon(PlayerWeapon::BareHands);
    grantPlayerClassStartingNodes();
    refreshSkillRuntimeEffects();
    return true;
}

void Simulation::refreshSkillRuntimeEffects() {
    const auto multiplier = [this](std::string_view key) {
        return std::max(0.05, 1.0 + skillTree_.effectValue(key));
    };
    crystalMines_.setProductionSpeedMultiplier(
        productionSpeedMultiplier_ * runProductionSpeedMultiplier_ *
        multiplier("production.speed"));
    lootChests_.setCoinCostMultiplier(
        chestOpeningCostMultiplier_ * multiplier("loot.chest_cost"));
    buildings_.setMaxHealthMultiplier(
        buildingMaxHealthMultiplier_ * runBuildingMaxHealthMultiplier_ *
        multiplier("building.health"));
    foundations_.setMaxHealthMultiplier(
        buildingMaxHealthMultiplier_ * runBuildingMaxHealthMultiplier_ *
        multiplier("building.health"));

    const double defenseDamage = defenseDamageMultiplier_ *
        multiplier("defense.damage");
    const double highGround = multiplier("defense.high_ground_damage");
    towers_.setSkillModifiers(
        defenseDamage * multiplier("tower.damage"),
        multiplier("tower.range"), defenseFireRateMultiplier_ *
            multiplier("tower.fire_rate"),
        highGround);
    cannons_.setSkillModifiers(
        defenseDamage * multiplier("cannon.damage"),
        multiplier("cannon.radius"), defenseFireRateMultiplier_ *
            multiplier("cannon.fire_rate"),
        highGround);
    traps_.setSkillModifiers(
        defenseDamage * multiplier("trap.damage"),
        multiplier("trap.radius"), defenseFireRateMultiplier_ *
            multiplier("trap.fire_rate"),
        highGround);
    playerWeapons_.setRifleSkillModifiers(
        multiplier("rifle.damage"), multiplier("rifle.range"),
        playerAttackSpeedMultiplier_ *
            temporaryAttackSpeedMultiplier() *
            multiplier("rifle.fire_rate"),
        static_cast<int>(std::lround(
            skillTree_.effectValue("rifle.magazine"))));
    const double playerDamage = multiplier("player.damage");
    iceWand_.setSkillModifiers(
        playerDamage * multiplier("ice.damage"), multiplier("ice.radius"),
        multiplier("ice.freeze_duration"), 1.0, 0.0);
    fireWand_.setSkillModifiers(
        playerDamage * multiplier("fire.damage"), multiplier("fire.radius"),
        multiplier("fire.burn_duration"),
        multiplier("fire.burn_damage"),
        skillTree_.effectValue("element.thermal_shock"));
    const double castSpeed = temporaryAttackSpeedMultiplier() *
        (battlePotionBerserkRemaining_ > 0.0
             ? BattlePotionAttackSpeedMultiplier
             : 1.0);
    iceWand_.setCastSpeedMultiplier(castSpeed);
    fireWand_.setCastSpeedMultiplier(castSpeed);
}

void Simulation::prepareRunUpgradeChoices() {
    runUpgradeSelectionsRemaining_ = 1 + bonusSelectionsNextReward_ +
        riskyInvestmentActive_;
    bonusSelectionsNextReward_ = 0;
    riskyInvestmentActive_ = 0;
    generateRunUpgradeChoices();
    runUpgradeChoicePending_ = true;
    invalidateSnapshotCache();
}

void Simulation::generateRunUpgradeChoices() {
    const std::size_t extraChoices = std::min<std::size_t>(
        MaximumRunUpgradeChoices - MinimumRunUpgradeChoices,
        static_cast<std::size_t>(std::max(
            0, runUpgradeStacks_[runUpgradeIndex(
                   RunUpgradeEffect::WiderChoice)])));
    runUpgradeChoiceCount_ = MinimumRunUpgradeChoices + extraChoices;

    if (lockedRunUpgrade_ == RunUpgradeEffect::WiderChoice &&
        runUpgradeChoiceCount_ >= MaximumRunUpgradeChoices) {
        lockedRunUpgrade_.reset();
    }
    std::array<RunUpgradeEffect, RunUpgradeEffectCount> pool{};
    std::size_t poolSize = 0U;
    for (const RunUpgradeDefinition& definition :
         RunUpgradeDefinitions) {
        if (definition.effect == RunUpgradeEffect::WiderChoice &&
            runUpgradeChoiceCount_ >= MaximumRunUpgradeChoices) {
            continue;
        }
        if (definition.effect == RunUpgradeEffect::LockChoice &&
            runUpgradeLockUnlocked_) {
            continue;
        }
        if (lockedRunUpgrade_ && definition.effect == *lockedRunUpgrade_) {
            continue;
        }
        pool[poolSize++] = definition.effect;
    }
    std::uint64_t randomState = mixBits64(
        static_cast<std::uint64_t>(terrain_.seed()) ^
        (static_cast<std::uint64_t>(wave_) *
         0x9e3779b97f4a7c15ULL) ^
        (static_cast<std::uint64_t>(++runUpgradeOfferGeneration_) *
         0xd1b54a32d192ed03ULL));
    std::size_t firstRandomIndex = 0U;
    if (lockedRunUpgrade_) {
        runUpgradeChoices_[0] = *lockedRunUpgrade_;
        firstRandomIndex = 1U;
    }
    for (std::size_t index = firstRandomIndex;
         index < runUpgradeChoiceCount_;
         ++index) {
        randomState = mixBits64(
            randomState + 0x9e3779b97f4a7c15ULL);
        const std::size_t poolIndex = index - firstRandomIndex;
        const std::size_t selected = poolIndex +
            static_cast<std::size_t>(
                randomState % (poolSize - poolIndex));
        std::swap(pool[poolIndex], pool[selected]);
        runUpgradeChoices_[index] = pool[poolIndex];
    }
}

bool Simulation::selectRunUpgrade(std::size_t choiceIndex) {
    if (!runUpgradeChoicePending_ ||
        choiceIndex >= runUpgradeChoiceCount_) {
        return false;
    }
    invalidateSnapshotCache();
    const RunUpgradeEffect effect = runUpgradeChoices_[choiceIndex];
    if (lockedRunUpgrade_ == effect) lockedRunUpgrade_.reset();
    ++runUpgradeStacks_[runUpgradeIndex(effect)];
    switch (effect) {
    case RunUpgradeEffect::Damage:
        runPlayerDamageMultiplier_ += 0.10;
        break;
    case RunUpgradeEffect::AttackSpeed:
        playerAttackSpeedMultiplier_ += 0.08;
        break;
    case RunUpgradeEffect::MoveSpeed:
        runPlayerMoveSpeedMultiplier_ += 0.07;
        break;
    case RunUpgradeEffect::MaximumHealth:
        playerBonusMaxHealth_ += 12.0;
        playerHealth_ = std::min(
            playerPermanentMaxHealth() + playerTemporaryHealth_,
            playerHealth_ + 12.0);
        break;
    case RunUpgradeEffect::RecoverableArmor:
        playerMaxRecoverableArmor_ += 8.0;
        playerRecoverableArmor_ += 8.0;
        break;
    case RunUpgradeEffect::BuildingHealth:
        runBuildingMaxHealthMultiplier_ += 0.12;
        break;
    case RunUpgradeEffect::BuildRadius:
        buildings_.setCoreBuildRadius(
            buildings_.coreBuildRadius() + 3);
        break;
    case RunUpgradeEffect::DefenseDamage:
        defenseDamageMultiplier_ += 0.12;
        break;
    case RunUpgradeEffect::DefenseFireRate:
        defenseFireRateMultiplier_ += 0.10;
        break;
    case RunUpgradeEffect::ProductionSpeed:
        runProductionSpeedMultiplier_ += 0.15;
        break;
    case RunUpgradeEffect::NightlyBomb:
        ++runNightlyBombBonus_;
        break;
    case RunUpgradeEffect::WiderChoice:
        break;
    case RunUpgradeEffect::BloodHarvest:
    case RunUpgradeEffect::Overkill:
    case RunUpgradeEffect::Ricochet:
    case RunUpgradeEffect::Salvager:
        break;
    case RunUpgradeEffect::DoubleDown:
        ++bonusSelectionsNextReward_;
        break;
    case RunUpgradeEffect::LockChoice:
        runUpgradeLockUnlocked_ = true;
        break;
    case RunUpgradeEffect::RerollToken:
        ++runUpgradeRerollTokens_;
        break;
    case RunUpgradeEffect::RiskyInvestment:
        ++riskyInvestmentPending_;
        break;
    }
    --runUpgradeSelectionsRemaining_;
    if (runUpgradeSelectionsRemaining_ > 0) {
        generateRunUpgradeChoices();
    } else {
        runUpgradeChoicePending_ = false;
    }
    refreshSkillRuntimeEffects();
    return true;
}

bool Simulation::rerollRunUpgrades() {
    if (!runUpgradeChoicePending_ || runUpgradeRerollTokens_ <= 0) {
        return false;
    }
    --runUpgradeRerollTokens_;
    generateRunUpgradeChoices();
    invalidateSnapshotCache();
    return true;
}

bool Simulation::lockRunUpgrade(std::size_t choiceIndex) {
    if (!runUpgradeChoicePending_ || !runUpgradeLockUnlocked_ ||
        choiceIndex >= runUpgradeChoiceCount_) {
        return false;
    }
    const RunUpgradeEffect effect = runUpgradeChoices_[choiceIndex];
    if (lockedRunUpgrade_ == effect) {
        lockedRunUpgrade_.reset();
    } else {
        lockedRunUpgrade_ = effect;
    }
    invalidateSnapshotCache();
    return true;
}

void Simulation::salvageDestroyedBuilding(
    BuildingType type, Vec3 position) {
    if (type == BuildingType::Core) return;
    salvageDestroyedCost(buildings_.configuredCost(type), position);
}

void Simulation::salvageDestroyedModularBuilding(
    const ModularBuildingDamageResult& result, Vec3 position) {
    ModularBuildPiece piece = ModularBuildPiece::Foundation;
    if (result.wall) {
        piece = ModularBuildPiece::Wall;
    } else if (result.ramp) {
        piece = ModularBuildPiece::Ramp;
    } else if (result.platformFrame && result.platformFrame->storey > 0) {
        piece = ModularBuildPiece::FloorPlatform;
    }
    salvageDestroyedCost(
        modularBuildingCosts_[static_cast<std::size_t>(piece)], position);
}

void Simulation::salvageDestroyedCost(
    ResourceCost cost, Vec3 position) {
    const int stacks = runUpgradeStacks_[runUpgradeIndex(
        RunUpgradeEffect::Salvager)];
    if (stacks <= 0) return;
    const double fraction = std::min(
        0.90, 0.30 * static_cast<double>(stacks));
    const auto grant = [&](ResourceType resource, int amount) {
        if (amount <= 0) return;
        int before = 0;
        if (resource == ResourceType::Wood) {
            before = wood_;
            addWood(amount);
            amount = wood_ - before;
        } else if (resource == ResourceType::Stone) {
            before = stone_;
            addStone(amount);
            amount = stone_ - before;
        } else if (resource == ResourceType::Crystal) {
            before = crystals_;
            addCrystals(amount);
            amount = crystals_ - before;
        }
        if (amount > 0) {
            events_.push_back({
                .type = GameEventType::ResourceGranted,
                .resourceType = resource,
                .position = position,
                .amount = amount,
            });
        }
    };
    grant(ResourceType::Wood,
          static_cast<int>(std::lround(cost.wood * fraction)));
    grant(ResourceType::Stone,
          static_cast<int>(std::lround(cost.stone * fraction)));
    grant(ResourceType::Crystal,
          static_cast<int>(std::lround(cost.crystals * fraction)));
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

void Simulation::grantBlueprintInsightForType(
    BuildingType type, int blueprintStackOrdinal) {
    if (unlimitedResources_ || blueprintStackOrdinal <= 0) {
        return;
    }
    const std::uint64_t typeIndex =
        static_cast<std::uint64_t>(type);
    const std::uint64_t rewardKey =
        static_cast<std::uint64_t>(blueprintStackOrdinal - 1) *
            GameBalance::BuildingTypeCount +
        typeIndex + 1U;
    grantConfiguredInsight(
        insight_.config().firstBuildingTypeBonus,
        InsightSource::StructureBuilt,
        InsightCategory::Building,
        {.eventId = insightIntegerEvent(0x501U, rewardKey),
         .oneTime = true});
}

void Simulation::grantBlueprintInsightForExistingBuildings(
    int blueprintStackOrdinal) {
    std::array<bool, GameBalance::BuildingTypeCount> present{};
    for (const BuildingInstance& building : buildings_.buildings()) {
        present[static_cast<std::size_t>(building.type)] = true;
    }
    for (std::size_t type = 0; type < present.size(); ++type) {
        if (present[type]) {
            grantBlueprintInsightForType(
                static_cast<BuildingType>(type),
                blueprintStackOrdinal);
        }
    }
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
             .oneTime = true, .bypassDiminishing = true});
        insight_.beginNewDiminishingCycle();
        break;
    }
    case GameEventType::EnemyKilled:
        if (event.entityId) {
            auto found = enemies_.enemies().end();
            if (!event.enemyType) {
                found = std::ranges::find_if(
                    enemies_.enemies(), [&event](const EnemyInstance& enemy) {
                        return enemy.id == *event.entityId;
                    });
            }
            const EnemyType type = event.enemyType.value_or(
                found != enemies_.enemies().end()
                    ? found->type : EnemyType::Basic);
            const bool boss = type == EnemyType::Boss;
            const bool elite = event.enemyType
                ? event.enemyEliteAffixes != 0U
                : found != enemies_.enemies().end() &&
                    found->eliteAffixes != 0U;
            const double eliteMultiplier = elite ? 1.7 : 1.0;
            grantConfiguredInsight(
                config.enemy[static_cast<std::size_t>(type)] *
                    eliteMultiplier,
                boss ? InsightSource::BossKilled : InsightSource::EnemyKilled,
                InsightCategory::Combat,
                {.eventId = insightEntityEvent(0x300U, *event.entityId),
                 .oneTime = false, .bypassDiminishing = boss});
        }
        break;
    case GameEventType::ResourceHit:
    case GameEventType::ResourceCollected:
        if (!unlimitedResources_ && event.resourceType &&
            isHarvestableResource(*event.resourceType) && event.amount > 0) {
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
            const int blueprintStacks = lootStacks_[
                lootUpgradeIndex(LootUpgradeEffect::Blueprint)];
            for (int stack = 1; stack <= blueprintStacks; ++stack) {
                grantBlueprintInsightForType(
                    *event.buildingType, stack);
            }
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
    const auto recordGameplayObjective =
        [this, &emitCompletions](ObjectiveMetric metric, int amount = 1) {
            emitCompletions(objectives_.onGameplayEvent(
                metric, amount, elapsedSeconds_));
        };
    for (std::size_t index = firstEvent; index < lastGameplayEvent; ++index) {
        const GameEvent& event = events_[index];
        if ((event.type == GameEventType::ResourceHit ||
             event.type == GameEventType::ResourceCollected) &&
            event.resourceType &&
            isHarvestableResource(*event.resourceType)) {
            if (*event.resourceType == ResourceType::Crystal) {
                emitCompletions(objectives_.onCrystalsGathered(
                    std::max(0, event.amount), elapsedSeconds_,
                    event.night || nightNow));
                continue;
            }
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
        } else if (event.type == GameEventType::CrystalProduced) {
            emitCompletions(objectives_.onCrystalsGathered(
                std::max(0, event.amount), elapsedSeconds_, event.night || nightNow));
        } else if (event.type == GameEventType::ResourceGatherMissed) {
            objectives_.onGatheringMiss();
        } else if (event.type == GameEventType::WaveCompleted) {
            recordGameplayObjective(ObjectiveMetric::WavesCompleted);
            static_cast<void>(objectives_.beginNewDay());
        } else {
            switch (event.type) {
            case GameEventType::EnemyKilled:
                recordGameplayObjective(ObjectiveMetric::EnemiesKilled);
                break;
            case GameEventType::BuildingPlaced:
                recordGameplayObjective(ObjectiveMetric::BuildingsPlaced);
                break;
            case GameEventType::ModularBuildingPlaced:
                recordGameplayObjective(ObjectiveMetric::ModularPiecesPlaced);
                break;
            case GameEventType::BuildingUpgraded:
                recordGameplayObjective(ObjectiveMetric::BuildingsUpgraded);
                break;
            case GameEventType::BuildingRepaired:
            case GameEventType::ModularBuildingRepaired:
                recordGameplayObjective(ObjectiveMetric::StructuresRepaired);
                break;
            case GameEventType::CoinCollected:
                recordGameplayObjective(
                    ObjectiveMetric::CoinsCollected,
                    std::max(0, event.amount));
                break;
            case GameEventType::ChestOpened:
                recordGameplayObjective(ObjectiveMetric::ChestsOpened);
                break;
            case GameEventType::LootCollected:
                recordGameplayObjective(ObjectiveMetric::LootCollected);
                break;
            case GameEventType::PlayerDashed:
                recordGameplayObjective(ObjectiveMetric::PlayerDashes);
                break;
            case GameEventType::WeaponFired:
                recordGameplayObjective(ObjectiveMetric::RifleShots);
                break;
            case GameEventType::IceWandHit:
            case GameEventType::FireWandHit:
                recordGameplayObjective(ObjectiveMetric::ElementalHits);
                break;
            case GameEventType::TrapHit:
                recordGameplayObjective(ObjectiveMetric::TrapHits);
                break;
            case GameEventType::CannonFired:
                if (event.buildingType == BuildingType::Cannon) {
                    recordGameplayObjective(ObjectiveMetric::CannonShots);
                }
                break;
            case GameEventType::ConsumableUsed:
                recordGameplayObjective(ObjectiveMetric::BombsThrown);
                break;
            case GameEventType::EarlyWaveBonusGranted:
                recordGameplayObjective(ObjectiveMetric::EarlyWavesStarted);
                break;
            case GameEventType::BuildingFortified:
                recordGameplayObjective(ObjectiveMetric::StructuresFortified);
                break;
            case GameEventType::GateToggled:
                recordGameplayObjective(ObjectiveMetric::GatesToggled);
                break;
            case GameEventType::BuildingSold:
                recordGameplayObjective(ObjectiveMetric::BuildingsSold);
                break;
            case GameEventType::RopeFallSaved:
                recordGameplayObjective(ObjectiveMetric::FallsSaved);
                break;
            default:
                break;
            }
        }
    }
}

void Simulation::processInsightEvents(
    std::size_t firstEvent, bool suppressEnemyRewards) {
    const std::size_t lastGameplayEvent = events_.size();
    for (std::size_t index = firstEvent; index < lastGameplayEvent; ++index) {
        const GameEvent event = events_[index];
        if (event.type == GameEventType::EnemyKilled && event.entityId) {
            if (!markEnemyRewarded(
                    insightRewardedEnemyIds_, *event.entityId)) {
                continue;
            }
            if (suppressEnemyRewards) {
                continue;
            }
        }
        processInsightEvent(event);
    }
    for (const EnemyInstance& enemy : enemies_.enemies()) {
        if (enemy.active || enemy.state != EnemyState::Dead) continue;
        if (!markEnemyRewarded(
                insightRewardedEnemyIds_, enemy.id)) continue;
        if (suppressEnemyRewards) {
            continue;
        }
        processInsightEvent({
            .type = GameEventType::EnemyKilled,
            .entityId = enemy.id,
            .enemyType = enemy.type,
            .enemyEliteAffixes = enemy.eliteAffixes,
            .position = enemy.position,
        });
    }
}

void Simulation::cycleUnlockedTool() {
    std::array<PlayerWeapon, PlayerWeaponCount> tools{};
    std::size_t toolCount = 0U;
    const auto addTool = [&tools, &toolCount](PlayerWeapon tool) {
        tools[toolCount++] = tool;
    };
    addTool(PlayerWeapon::BareHands);
    if (unlimitedResources_ || skillTree_.hasEffect("unlock.axe"))
        addTool(PlayerWeapon::Axe);
    if (unlimitedResources_ || skillTree_.hasEffect("unlock.pickaxe"))
        addTool(PlayerWeapon::Pickaxe);
    if (unlimitedResources_ || skillTree_.hasEffect("unlock.club"))
        addTool(PlayerWeapon::Club);
    if (unlimitedResources_ || skillTree_.hasEffect("unlock.ice_wand"))
        addTool(PlayerWeapon::IceWand);
    if (unlimitedResources_ || skillTree_.hasEffect("unlock.fire_wand"))
        addTool(PlayerWeapon::FireWand);
    if (unlimitedResources_ || skillTree_.hasEffect("unlock.hammer"))
        addTool(PlayerWeapon::Hammer);
    if (unlimitedResources_ || skillTree_.hasEffect("unlock.rifle"))
        addTool(PlayerWeapon::Rifle);
    if (unlimitedResources_ || skillTree_.hasEffect("unlock.bombs"))
        addTool(PlayerWeapon::Bomb);
    const auto end = tools.begin() + static_cast<std::ptrdiff_t>(toolCount);
    const auto current = std::find(
        tools.begin(), end, playerWeapons_.selectedWeapon());
    const std::size_t next = current == end
        ? 0U
        : (static_cast<std::size_t>(
               std::distance(tools.begin(), current)) + 1U) %
              toolCount;
    playerWeapons_.selectWeapon(tools[next]);
    selectedBuilding_.reset();
    buildingPreview_.reset();
}

} // namespace ian
